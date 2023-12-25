#include "voice_peer.hpp"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/audio_stream_microphone.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>


#define MAX_PACKET_SIZE 3828 // 1276 * 3
#define SAMPLE_RATE 48000

VoicePeer::VoicePeer()
{
	base = nullptr;
	mic = nullptr;
	//mic_bus = -1;
	encoder = nullptr;
	frame_count = 960; // 48000 / 50
	use_microphone = false;

	sn_upload = StringName("_upload", true);

	int32_t err;
	decoder = opus_decoder_create(SAMPLE_RATE, 1, &err);
	if (err != OPUS_OK)
		UtilityFunctions::push_error("Failed to create Opus decoder: " + String::num_int64(err));
}

VoicePeer::~VoicePeer()
{
	if (encoder)
		opus_encoder_destroy(encoder);
	if (decoder)
		opus_decoder_destroy(decoder);
	if (!mic_busname.is_empty())
		AudioServer::get_singleton()->remove_bus(AudioServer::get_singleton()->get_bus_index(mic_busname));
}

void VoicePeer::setup(Node* base)
{
	// base is either an AudioStreamPlayer or an AudioStreamPlayer3D
	this->base = base;

	AudioStreamPlayer3D* playback3d = Object::cast_to<AudioStreamPlayer3D>(base);

	Ref<AudioStreamGenerator> generator;
	generator.instantiate();
	generator->set_mix_rate(SAMPLE_RATE);

	if (playback3d) {
		// 3d
		playback3d->set_stream(generator);
		playback3d->play();
		playback_generator = playback3d->get_stream_playback();
	}
	else {
		// none
		AudioStreamPlayer* playback = Object::cast_to<AudioStreamPlayer>(base);
		playback->set_stream(generator);
		playback->play();
		playback_generator = playback->get_stream_playback();
	}

	_update_use_microphone();
}

void VoicePeer::_update_use_microphone()
{
	if (!base)
		return;

	if (use_microphone) {
		if (!mic) {
			// check if microphone is enabled
			ProjectSettings* ps = ProjectSettings::get_singleton();
			if (ps->has_setting("audio/driver/enable_input")) {
				if (!ps->get_setting("audio/driver/enable_input")) {
					UtilityFunctions::push_error("Audio input is disabled in the project settings.");
				}
			}

			// create our private bus and the capture effect

			mic_busname = "mv_peermic" + String::num_int64(UtilityFunctions::randi(), 16);

			AudioServer* as = AudioServer::get_singleton();

			int32_t mic_bus = as->get_bus_count();
			as->add_bus(mic_bus); // add a new bus at the end
			as->set_bus_name(mic_bus, mic_busname);
			as->set_bus_mute(mic_bus, true);

			mic_capture.instantiate();
			as->add_bus_effect(mic_bus, mic_capture);

			Ref<AudioStreamMicrophone> mic_stream;
			mic_stream.instantiate();

			mic = memnew(AudioStreamPlayer);
			mic->set_name("_microphone");
			mic->set_multiplayer_authority(base->get_multiplayer_authority());
			mic->set_stream(mic_stream);
			base->add_child(mic, false, Node::INTERNAL_MODE_BACK);
			mic->set_bus(mic_busname);
			mic->play();

			// create the encoder
			int32_t err;
			encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
			if (err != OPUS_OK)
				UtilityFunctions::push_error("Failed to create Opus encoder: " + String::num_int64(err));

			opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
			opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
			opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
		}
	}
	else {
		// remove microphone
		if (mic) {
			base->remove_child(mic);
			memdelete(mic);
			mic = nullptr;
			mic_capture.unref();
			if (encoder) {
				opus_encoder_destroy(encoder);
				encoder = nullptr;
			}
		}
		if (!mic_busname.is_empty()) {
			AudioServer::get_singleton()->remove_bus(AudioServer::get_singleton()->get_bus_index(mic_busname));
			mic_busname = "";
		}
	}
}

void VoicePeer::set_use_microphone(bool yes) {
	if (use_microphone == yes)
		return;
	use_microphone = yes;
	_update_use_microphone();
}

void VoicePeer::_poll_receive() {
	if (decoder_buffer.is_empty())
		return;
	PackedByteArray pba = decoder_buffer.front()->get();
	decoder_buffer.pop_front();

	PackedVector2Array pv2;

	float* f = (float*)memalloc(sizeof(float) * frame_count * 6);
	int r = opus_decode_float(decoder, pba.ptr(), pba.size(), f, frame_count, 0);
	if (r < 0) {
		UtilityFunctions::push_error("Failed to decode Opus packet: " + String::num_int64(r));
		memfree(f);
		return;
	}

	pv2.resize(r);

	// apply to both channels
	for (int64_t i = 0; i < r; i++) {
		pv2[i].x = f[i];
		pv2[i].y = f[i];
	}

	memfree(f);
	//UtilityFunctions::prints("rec : ", pv2.size());
	playback_generator->push_buffer(pv2);
}

void VoicePeer::_poll_microphone() {
	if (mic_capture.is_null())
		return;

	if (!mic_capture->can_get_buffer(frame_count))
		return;

	PackedVector2Array pv2 = mic_capture->get_buffer(frame_count);

	// opus time
	// since we're using a mono microphone, we can just use the left channel
	float* mono = (float*)memalloc(sizeof(float) * pv2.size());
	for (int64_t i = 0; i < pv2.size(); i++) {
		mono[i] = pv2[i].x;
	}

	PackedByteArray pba;
	pba.resize(MAX_PACKET_SIZE); // MAX_PACKET_SIZE * channel(1)
	uint8_t* w = pba.ptrw();

	opus_int32 size = opus_encode_float(encoder, mono, frame_count, w, MAX_PACKET_SIZE);
	memfree(mono);

	if (size < 0) {
		UtilityFunctions::push_error("Failed to encode Opus packet: " + String::num_int64(size));
		return;
	}

	pba.resize(size); // resize to the actual size

	// send the packet
	base->call(sn_upload, pba);
	//UtilityFunctions::prints("snd : ", pba.size());
}

void VoicePeer::poll_notifications(int p_what)
{
	if (p_what != Node::NOTIFICATION_PROCESS)
		return;

	_poll_receive();
	_poll_microphone();
}

void VoicePeer::poll_receive(const PackedByteArray& data) {
	decoder_buffer.push_back(data);
}

void VoicePeer::set_frame_count(int32_t frame_count)
{
	this->frame_count = frame_count;
}

int32_t VoicePeer::get_frame_count() const
{
	return frame_count;
}

StringName VoicePeer::get_mic_busname() const {
	return mic_busname;
}
AudioStreamPlayer* VoicePeer::get_mic_player() const {
	return mic;
}
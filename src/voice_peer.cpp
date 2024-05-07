#include "voice_peer.hpp"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/audio_stream_microphone.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_generator.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/engine.hpp>


#define MAX_PACKET_SIZE 3828 // 1276 * 3
#define SAMPLE_RATE 48000

#define MAX_PROCESS_RATE 50

VoicePeer::VoicePeer()
{
	base = nullptr;
	mic = nullptr;
	//mic_bus = -1;
	encoder = nullptr;
	
	use_microphone = false;
	use_opus = true;

	opus_bitrate = 16000;

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
	cb_upload = Callable(base, "_upload");
	cb_upload_raw = Callable(base, "_upload_raw");

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

			opus_encoder_ctl(encoder, OPUS_SET_BITRATE(opus_bitrate));
			opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
			opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
			//opus_encoder_ctl(encoder, OPUS_SET_DTX(1));
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

void VoicePeer::_poll_microphone() {
	if (mic_capture.is_null())
		return;

	int target_frame_count;
	int processed_frame_count = 0;

	double fps = Engine::get_singleton()->get_frames_per_second(); // force update
	if (fps > 50)
		fps = 50; // cap at 50

	target_frame_count = 48000 / fps; // 1 second of audio

	const int frame_count = 960; // 20ms at 48kHz

	PackedVector2Array pv2;

	while (target_frame_count > processed_frame_count) {
		if (!mic_capture->can_get_buffer(frame_count))
			break;

		pv2 = mic_capture->get_buffer(frame_count);

		if (use_opus) {
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
				break;
			}

			pba.resize(size); // resize to the actual size
			// send the packet
			cb_upload.call(pba); // custom_
		} else {
			// raw time
			cb_upload_raw.call(pv2); // custom_
		
		}

		processed_frame_count += frame_count;
	}
}

void VoicePeer::poll_notifications(int p_what)
{
	if (p_what != Node::NOTIFICATION_PROCESS)
		return;

	_poll_microphone();
}

void VoicePeer::poll_receive(const PackedByteArray& data) {
	const int frame_count = 960; // 20ms at 48kHz
	float* f = (float*)memalloc(sizeof(float) * frame_count * 6);
	int r = opus_decode_float(decoder, data.ptr(), data.size(), f, frame_count, 0);
	if (r < 0) {
		UtilityFunctions::push_error("Failed to decode Opus packet: " + String::num_int64(r));
		memfree(f);
		return;
	}

	PackedVector2Array pv2;
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

void VoicePeer::poll_receive_raw(const PackedVector2Array& data) {
	playback_generator->push_buffer(data);
}

StringName VoicePeer::get_mic_busname() const {
	return mic_busname;
}
AudioStreamPlayer* VoicePeer::get_mic_player() const {
	return mic;
}

void VoicePeer::clear_buffer() {
	if (mic_capture.is_valid())
		mic_capture->clear_buffer();
}

void VoicePeer::set_opus_bitrate(uint32_t bitrate) {
	opus_bitrate = bitrate;
	if (encoder)
		opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));
}

void VoicePeer::set_use_opus(bool yes) {
	use_opus = yes;
}

void VoicePeer::set_use_dtx(bool yes) {
	if (encoder)
		opus_encoder_ctl(encoder, OPUS_SET_DTX(yes ? 1 : 0));
}
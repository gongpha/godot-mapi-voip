#pragma once

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_effect_capture.hpp>
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>

#include <opus/opus.h>

using namespace godot;

class VoicePeer
{
private:
	Node* base;
	Callable cb_upload;
	Callable cb_upload_raw;
	Ref<AudioStreamGeneratorPlayback> playback_generator;
	OpusDecoder* decoder;

	bool use_opus;
	//List<PackedByteArray> decoder_buffer;
	//void _poll_receive();

	AudioStreamPlayer* mic;
	Ref<AudioEffectCapture> mic_capture;
	StringName mic_busname; // "" means no microphone added
	OpusEncoder* encoder;
	void _poll_microphone();

	bool use_microphone;
	void _update_use_microphone();

	uint32_t opus_bitrate;
	bool use_dtx;


public:
	VoicePeer();
	~VoicePeer();

	void setup(Node* base);

	void poll_notifications(int p_what);
	void poll_receive(const PackedByteArray& data);
	void poll_receive_raw(const PackedVector2Array& data);

	void set_use_microphone(bool yes);

	StringName get_mic_busname() const;
	AudioStreamPlayer* get_mic_player() const;

	void set_opus_bitrate(uint32_t bitrate);
	void set_use_opus(bool yes);
	void set_use_dtx(bool yes);

	void clear_buffer();
};

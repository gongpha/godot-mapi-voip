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
	Ref<AudioStreamGeneratorPlayback> playback_generator;
	OpusDecoder* decoder;
	List<PackedByteArray> decoder_buffer;
	void _poll_receive();

	AudioStreamPlayer* mic;
	Ref<AudioEffectCapture> mic_capture;
	StringName mic_busname; // "" means no microphone added
	OpusEncoder* encoder;
	void _poll_microphone();

	int32_t frame_count;

	StringName sn_upload;

	bool use_microphone;
	void _update_use_microphone();


public:
	VoicePeer();
	~VoicePeer();

	void setup(Node* base);

	void poll_notifications(int p_what);
	void poll_receive(const PackedByteArray& data);

	void set_frame_count(int32_t frame_count);
	int32_t get_frame_count() const;

	void set_use_microphone(bool yes);

	StringName get_mic_busname() const;
	AudioStreamPlayer* get_mic_player() const;
};

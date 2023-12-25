#pragma once

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "voice_peer.hpp"

using namespace godot;

class VoiceInstance3D : public AudioStreamPlayer3D
{
	GDCLASS(VoiceInstance3D, AudioStreamPlayer3D);

	bool use_microphone;
	bool loopback;
	bool rpc_update;

	VoicePeer* voice_peer;

	StringName sn_receive;
	StringName sn_sent_frame_data;
	StringName sn_received_frame_data;

	void _recheck_use_microphone() const;

protected:
	static void _bind_methods();
	virtual void _notification(int p_what);

public:
	VoiceInstance3D();
	~VoiceInstance3D();

	virtual void _enter_tree();
	virtual void _exit_tree();

	void _upload(const PackedByteArray& data);
	void _receive(const PackedByteArray& data);

	StringName get_mic_busname() const;
	AudioStreamPlayer* get_mic_player() const;

	void set_use_microphone(bool yes);
	bool is_using_microphone() const;

	void set_loopback(bool yes);
	bool is_loopback() const;

	void set_rpc_update(bool yes);
	bool is_rpc_update() const;
};

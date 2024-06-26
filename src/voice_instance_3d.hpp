#pragma once

#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include "voice_peer.hpp"

using namespace godot;

/* also works for non-spatial playback */

class VoiceInstance3D : public AudioStreamPlayer3D
{
	GDCLASS(VoiceInstance3D, AudioStreamPlayer3D);

public:
	enum LoopbackMode {
		LOOPBACK_NONE,
		LOOPBACK_OPUS,
		LOOPBACK_ORIGINAL_LOCAL // loopback original audio, but only for local player. no rpc
	};
private:

	bool use_microphone;
	LoopbackMode loopback_mode;
	bool rpc_update;

	VoicePeer* voice_peer;

	HashSet<int> peer_list;
	HashSet<int> too_far_list;
	bool list_used_as_blacklist;

	StringName sn_receive;
	StringName sn_dist_too_far;
	StringName sn_sent_frame_data;
	StringName sn_received_frame_data;
	StringName sn_sent_frame_data_raw;
	StringName sn_received_frame_data_raw;
	StringName sn_sent_dist_too_far_msg;
	StringName sn_received_dist_too_far_msg;

	bool in_dist;
	
	bool use_dtx;

	void _recheck_use_microphone() const;
	void _send_voice(const PackedByteArray& data);
	void _send_voice_all(const PackedByteArray& data);
	void _send_voice_single(int32_t peer, const PackedByteArray& data);

	void _receive(const PackedByteArray& data);
	void _dist_too_far(bool yes);

	uint32_t opus_bitrate;

protected:
	static void _bind_methods();
	virtual void _notification(int p_what);

public:
	VoiceInstance3D();
	~VoiceInstance3D();

	void _upload();
	void _upload_raw();

	StringName get_mic_busname() const;
	AudioStreamPlayer* get_mic_player() const;

	void set_use_microphone(bool yes);
	bool is_using_microphone() const;

	void set_loopback_mode(LoopbackMode new_mode);
	LoopbackMode get_loopback_mode() const;

	void set_rpc_update(bool yes);
	bool is_rpc_update() const;

	void set_use_dtx(bool yes);
	bool is_using_dtx() const;

	void add_to_list(int peer_id);
	void remove_in_list(int peer_id);
	bool list_has(int peer_id) const;
	PackedInt32Array get_list() const;
	void set_list_used_as_blacklist(bool yes);
	bool is_list_used_as_blacklist() const;

	void clear_buffer();

	void set_opus_bitrate(uint32_t bitrate);
	uint32_t get_opus_bitrate() const;

	int get_pitch();
};

VARIANT_ENUM_CAST(VoiceInstance3D::LoopbackMode);
#include "voice_instance_3d.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>

#define RPC_CHANNEL 55

using namespace godot;

void VoiceInstance3D::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("_notification"), &VoiceInstance3D::_notification);
	ClassDB::bind_method(D_METHOD("_upload", "data"), &VoiceInstance3D::_upload);
	ClassDB::bind_method(D_METHOD("_upload_raw", "data"), &VoiceInstance3D::_upload_raw);
	ClassDB::bind_method(D_METHOD("_receive", "data"), &VoiceInstance3D::_receive);
	ClassDB::bind_method(D_METHOD("_dist_too_far", "yes"), &VoiceInstance3D::_dist_too_far);

	ClassDB::bind_method(D_METHOD("set_use_microphone", "yes"), &VoiceInstance3D::set_use_microphone);
	ClassDB::bind_method(D_METHOD("is_using_microphone"), &VoiceInstance3D::is_using_microphone);

	ClassDB::bind_method(D_METHOD("get_mic_busname"), &VoiceInstance3D::get_mic_busname);
	ClassDB::bind_method(D_METHOD("get_mic_player"), &VoiceInstance3D::get_mic_player);

	ClassDB::bind_method(D_METHOD("set_loopback_mode", "yes"), &VoiceInstance3D::set_loopback_mode);
	ClassDB::bind_method(D_METHOD("get_loopback_mode"), &VoiceInstance3D::get_loopback_mode);

	ClassDB::bind_method(D_METHOD("set_rpc_update", "yes"), &VoiceInstance3D::set_rpc_update);
	ClassDB::bind_method(D_METHOD("is_rpc_update"), &VoiceInstance3D::is_rpc_update);

	ClassDB::bind_method(D_METHOD("add_to_list", "peer_id"), &VoiceInstance3D::add_to_list);
	ClassDB::bind_method(D_METHOD("remove_in_list", "peer_id"), &VoiceInstance3D::remove_in_list);
	ClassDB::bind_method(D_METHOD("list_has", "peer_id"), &VoiceInstance3D::list_has);
	ClassDB::bind_method(D_METHOD("get_list"), &VoiceInstance3D::get_list);
	ClassDB::bind_method(D_METHOD("set_list_used_as_blacklist", "yes"), &VoiceInstance3D::set_list_used_as_blacklist);
	ClassDB::bind_method(D_METHOD("is_list_used_as_blacklist"), &VoiceInstance3D::is_list_used_as_blacklist);

	ClassDB::bind_method(D_METHOD("clear_buffer"), &VoiceInstance3D::clear_buffer);

	ClassDB::bind_method(D_METHOD("set_opus_bitrate", "bitrate"), &VoiceInstance3D::set_opus_bitrate);
	ClassDB::bind_method(D_METHOD("get_opus_bitrate"), &VoiceInstance3D::get_opus_bitrate);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_microphone"), "set_use_microphone", "is_using_microphone");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loopback_mode"), "set_loopback_mode", "get_loopback_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rpc_update"), "set_rpc_update", "is_rpc_update");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "list_used_as_blacklist"), "set_list_used_as_blacklist", "is_list_used_as_blacklist");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "opus_bitrate"), "set_opus_bitrate", "get_opus_bitrate");

	ADD_SIGNAL(MethodInfo("sent_frame_data", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("received_frame_data", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("sent_frame_data_raw", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("received_frame_data_raw", PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("sent_dist_too_far_msg", PropertyInfo(Variant::INT, "peer"), PropertyInfo(Variant::BOOL, "too_far")));
	ADD_SIGNAL(MethodInfo("received_dist_too_far_msg", PropertyInfo(Variant::INT, "peer"), PropertyInfo(Variant::BOOL, "too_far")));

	BIND_ENUM_CONSTANT(LOOPBACK_NONE);
	BIND_ENUM_CONSTANT(LOOPBACK_OPUS);
	BIND_ENUM_CONSTANT(LOOPBACK_ORIGINAL_LOCAL);
}

void VoiceInstance3D::_notification(int p_what)
{
	switch (p_what)
	{
		case NOTIFICATION_READY: {
			Dictionary opts;
			opts["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
			opts["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED;
			opts["call_local"] = false;
			opts["channel"] = RPC_CHANNEL;
			rpc_config(sn_receive, opts);

			opts = Dictionary();
			opts["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
			opts["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
			opts["call_local"] = false;
			opts["channel"] = RPC_CHANNEL;
			rpc_config(sn_dist_too_far, opts);
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint())
				// Don't initialize voice in editor
				return;
			if (voice_peer)
				memdelete(voice_peer);
			voice_peer = memnew(VoicePeer);
			voice_peer->setup(this);
			voice_peer->set_opus_bitrate(opus_bitrate);
			voice_peer->set_use_opus(loopback_mode != LOOPBACK_ORIGINAL_LOCAL);
			_recheck_use_microphone();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (voice_peer)
				memdelete(voice_peer);
			voice_peer = nullptr;
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			// Check if the distance is too far from the camera
			if (!is_multiplayer_authority()) {
				// this node is not the authority
				bool _in_dist;
				if (get_max_distance() > 0) {
					Vector3 pos = get_viewport()->get_camera_3d()->get_global_position();
					_in_dist = pos.distance_to(get_global_position()) <= get_max_distance();
				} else {
					_in_dist = true;
				}
				if (_in_dist != in_dist) {
					rpc_id(get_multiplayer_authority(), sn_dist_too_far, !_in_dist);
					in_dist = _in_dist;
					emit_signal(sn_sent_dist_too_far_msg, get_multiplayer_authority(), !in_dist);
				}
			}
			/**/
		} break;

	}

	if (voice_peer)
		voice_peer->poll_notifications(p_what);
}

VoiceInstance3D::VoiceInstance3D()
{
	voice_peer = nullptr;
	sn_receive = StringName("_receive", true);
	sn_dist_too_far = StringName("_dist_too_far", true);
	sn_sent_frame_data = StringName("sent_frame_data", true);
	sn_received_frame_data = StringName("received_frame_data", true);
	sn_sent_frame_data_raw = StringName("sent_frame_data_raw", true);
	sn_received_frame_data_raw = StringName("received_frame_data_raw", true);
	sn_sent_dist_too_far_msg = StringName("sent_dist_too_far_msg", true);
	sn_received_dist_too_far_msg = StringName("received_dist_too_far_msg", true);
	use_microphone = true;
	loopback_mode = LOOPBACK_NONE;
	rpc_update = true;
	list_used_as_blacklist = true;
	in_dist = true;

	opus_bitrate = 16000;

	set_notify_transform(true);

	if (!Engine::get_singleton()->is_editor_hint()) {
		set_process(true);
	}
}

VoiceInstance3D::~VoiceInstance3D()
{
	if (voice_peer)
		memdelete(voice_peer);
}

void VoiceInstance3D::_recheck_use_microphone() const
{
	if (!voice_peer)
		return;

	voice_peer->set_use_microphone(use_microphone && is_multiplayer_authority());
}

void VoiceInstance3D::set_use_microphone(bool yes) {
	use_microphone = yes;
	_recheck_use_microphone();
}

bool VoiceInstance3D::is_using_microphone() const {
	return use_microphone;
}

void VoiceInstance3D::set_loopback_mode(LoopbackMode new_mode) {
	loopback_mode = new_mode;
	if (voice_peer)
		voice_peer->set_use_opus(loopback_mode != LOOPBACK_ORIGINAL_LOCAL);
}

VoiceInstance3D::LoopbackMode VoiceInstance3D::get_loopback_mode() const {
	return loopback_mode;
}

void VoiceInstance3D::set_rpc_update(bool yes) {
	rpc_update = yes;	
}

bool VoiceInstance3D::is_rpc_update() const {
	return rpc_update;
}

void VoiceInstance3D::add_to_list(int peer_id) {
	ERR_FAIL_COND(peer_list.find(peer_id));
	peer_list.insert(peer_id);
}

void VoiceInstance3D::remove_in_list(int peer_id) {
	ERR_FAIL_COND(!peer_list.find(peer_id));
	peer_list.erase(peer_id);
}

bool VoiceInstance3D::list_has(int peer_id) const {
	return peer_list.find(peer_id) != peer_list.end();
}

PackedInt32Array VoiceInstance3D::get_list() const {
	PackedInt32Array arr;
	arr.resize(peer_list.size());
	int i = 0;
	for (HashSet<int>::Iterator it = peer_list.begin(); it; ++it){
		arr.set(i, *it);
		i++;
	}
	return arr;
}

void VoiceInstance3D::set_list_used_as_blacklist(bool yes) {
	list_used_as_blacklist = yes;
}

bool VoiceInstance3D::is_list_used_as_blacklist() const {
	return list_used_as_blacklist;
}

void VoiceInstance3D::clear_buffer() {
	ERR_FAIL_COND(!voice_peer);
	voice_peer->clear_buffer();
}

void VoiceInstance3D::_send_voice(const PackedByteArray& data) {
	if (list_used_as_blacklist) {
		if (peer_list.is_empty()) {
			// Send to all
			_send_voice_all(data);
			
		} else {
			// Send to all except blocked peers
			PackedInt32Array pia32 = get_multiplayer()->get_peers();
			for (int i = 0; i < pia32.size(); i++) {
				int peer_id = pia32[i];
				if (peer_list.find(peer_id))
					continue;
				_send_voice_single(peer_id, data);
			}
		}
	} else {
		// Send to listed peers
		for (HashSet<int>::Iterator it = peer_list.begin(); it; ++it){
			_send_voice_single(*it, data);
		}
	}
}

void VoiceInstance3D::_send_voice_all(const PackedByteArray& data) {
	if (too_far_list.is_empty()) {
		rpc(sn_receive, data);
		return;
	}

	PackedInt32Array pia32 = get_multiplayer()->get_peers();
	for (int i = 0; i < pia32.size(); i++) {
		int peer_id = pia32[i];
		if (too_far_list.find(peer_id))
			continue;
		rpc_id(peer_id, sn_receive, data);
	}
}

void VoiceInstance3D::_send_voice_single(int32_t peer, const PackedByteArray& data) {
	if (too_far_list.find(peer))
		return;
	rpc_id(peer, sn_receive, data);
}

void VoiceInstance3D::_upload(const PackedByteArray& data)
{
	ERR_FAIL_COND(!voice_peer);
	_recheck_use_microphone();
	if (use_microphone && is_multiplayer_authority()) {
		if (rpc_update) {
			_send_voice(data);
		}

		emit_signal(sn_sent_frame_data, data);

		if (loopback_mode == LOOPBACK_OPUS)
			voice_peer->poll_receive(data);
	}
}

void VoiceInstance3D::_upload_raw(const PackedVector2Array& data) {
	ERR_FAIL_COND(!voice_peer);
	_recheck_use_microphone();
	if (use_microphone && is_multiplayer_authority()) {
		emit_signal(sn_sent_frame_data_raw, data);

		if (loopback_mode == LOOPBACK_ORIGINAL_LOCAL)
			voice_peer->poll_receive_raw(data);
	}
}

void VoiceInstance3D::_receive(const PackedByteArray& data)
{
	ERR_FAIL_COND(!voice_peer);
	voice_peer->poll_receive(data);
	emit_signal(sn_received_frame_data, data);
}

void VoiceInstance3D::_dist_too_far(bool yes) {
	if (yes) {
		too_far_list.insert(get_multiplayer()->get_remote_sender_id());
	} else {
		too_far_list.erase(get_multiplayer()->get_remote_sender_id());
	}
	emit_signal(sn_received_dist_too_far_msg, get_multiplayer()->get_remote_sender_id(), yes);
}

StringName VoiceInstance3D::get_mic_busname() const {
	ERR_FAIL_COND_V(!voice_peer, StringName());
	return voice_peer->get_mic_busname();
}
AudioStreamPlayer* VoiceInstance3D::get_mic_player() const {
	ERR_FAIL_COND_V(!voice_peer, nullptr);
	return voice_peer->get_mic_player();
}

void VoiceInstance3D::set_opus_bitrate(uint32_t bitrate) {
	opus_bitrate = bitrate;
	if (voice_peer)
		voice_peer->set_opus_bitrate(bitrate);
}
uint32_t VoiceInstance3D::get_opus_bitrate() const {
	return opus_bitrate;
}
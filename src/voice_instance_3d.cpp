#include "voice_instance_3d.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/engine.hpp>

#define RPC_CHANNEL 55

using namespace godot;

void VoiceInstance3D::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("_notification"), &VoiceInstance3D::_notification);
	ClassDB::bind_method(D_METHOD("_upload", "data"), &VoiceInstance3D::_upload);
	ClassDB::bind_method(D_METHOD("_receive", "data"), &VoiceInstance3D::_receive);

	ClassDB::bind_method(D_METHOD("set_use_microphone", "yes"), &VoiceInstance3D::set_use_microphone);
	ClassDB::bind_method(D_METHOD("is_using_microphone"), &VoiceInstance3D::is_using_microphone);

	ClassDB::bind_method(D_METHOD("get_mic_busname"), &VoiceInstance3D::get_mic_busname);
	ClassDB::bind_method(D_METHOD("get_mic_player"), &VoiceInstance3D::get_mic_player);

	ClassDB::bind_method(D_METHOD("set_loopback", "yes"), &VoiceInstance3D::set_loopback);
	ClassDB::bind_method(D_METHOD("is_loopback"), &VoiceInstance3D::is_loopback);

	ClassDB::bind_method(D_METHOD("set_rpc_update", "yes"), &VoiceInstance3D::set_rpc_update);
	ClassDB::bind_method(D_METHOD("is_rpc_update"), &VoiceInstance3D::is_rpc_update);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_microphone"), "set_use_microphone", "is_using_microphone");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loopback"), "set_loopback", "is_loopback");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rpc_update"), "set_rpc_update", "is_rpc_update");

	ADD_SIGNAL(MethodInfo("sent_frame_data", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("received_frame_data", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
}

void VoiceInstance3D::_notification(int p_what)
{
	if (p_what == NOTIFICATION_READY) {
		Dictionary opts;
		opts["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
		opts["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE;
		opts["call_local"] = false;
		opts["channel"] = RPC_CHANNEL;
		rpc_config(sn_receive, opts);
	}

	if (voice_peer)
		voice_peer->poll_notifications(p_what);
}

VoiceInstance3D::VoiceInstance3D()
{
	voice_peer = nullptr;
	sn_receive = StringName("_receive", true);
	sn_sent_frame_data = StringName("sent_frame_data", true);
	sn_received_frame_data = StringName("received_frame_data", true);
	use_microphone = true;
	loopback = false;
	rpc_update = true;

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

void VoiceInstance3D::set_loopback(bool yes) {
	loopback = yes;
}

bool VoiceInstance3D::is_loopback() const {
	return loopback;
}

void VoiceInstance3D::set_rpc_update(bool yes) {
	rpc_update = yes;	
}

bool VoiceInstance3D::is_rpc_update() const {
	return rpc_update;
}

void VoiceInstance3D::_upload(const PackedByteArray& data)
{
	ERR_FAIL_COND(!voice_peer);
	_recheck_use_microphone();
	if (use_microphone && is_multiplayer_authority()) {
		if (rpc_update)
			rpc(sn_receive, data);

		emit_signal(sn_sent_frame_data, data);

		if (loopback) {
			voice_peer->poll_receive(data);
		}
	}
}

void VoiceInstance3D::_receive(const PackedByteArray& data)
{
	ERR_FAIL_COND(!voice_peer);
	voice_peer->poll_receive(data);
	emit_signal(sn_received_frame_data, data);
}

void VoiceInstance3D::_enter_tree()
{
	AudioStreamPlayer3D::_enter_tree();

	if (Engine::get_singleton()->is_editor_hint())
		// Don't initialize voice in editor
		return;
	if (voice_peer)
		memdelete(voice_peer);
	voice_peer = memnew(VoicePeer);
	voice_peer->setup(this);
	_recheck_use_microphone();
}

void VoiceInstance3D::_exit_tree()
{
	AudioStreamPlayer3D::_exit_tree();

	if (voice_peer) {
		memdelete(voice_peer);
		voice_peer = nullptr;
	}
}

StringName VoiceInstance3D::get_mic_busname() const {
	ERR_FAIL_COND_V(!voice_peer, StringName());
	return voice_peer->get_mic_busname();
}
AudioStreamPlayer* VoiceInstance3D::get_mic_player() const {
	ERR_FAIL_COND_V(!voice_peer, nullptr);
	return voice_peer->get_mic_player();
}
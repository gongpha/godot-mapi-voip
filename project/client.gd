extends Base
class_name Client

func _ready() -> void :
	super()
	print("client")
	
	var peer := ENetMultiplayerPeer.new()
	peer.create_client("127.0.0.1", 5555)
	multiplayer.multiplayer_peer = peer

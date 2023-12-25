extends Base
class_name Server

func _ready() -> void :
	super()
	print("server")
	
	var peer := ENetMultiplayerPeer.new()
	peer.create_server(5555, 2)
	multiplayer.multiplayer_peer = peer
	
	server()

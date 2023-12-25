extends Node
class_name Base

func _ready() -> void :
	multiplayer.connected_to_server.connect(_conn)
	multiplayer.peer_connected.connect(_peer_con)
	
func _conn() -> void :
	_peer_con(multiplayer.get_unique_id())
	
func server() -> void :
	_peer_con(1)
	
func _peer_con(peer : int) -> void :
	print("new ", peer)
	var player := preload("res://player.tscn").instantiate()
	player.name = str(peer)
	player.set_multiplayer_authority(peer)
	add_child(player)

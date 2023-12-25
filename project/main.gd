extends Control


func _on_client_pressed() -> void:
	var cl := Client.new()
	add_child(cl)
	$"3d/label".text = "client"
	hide()


func _on_server_pressed() -> void:
	var sv := Server.new()
	add_child(sv)
	$"3d/label".text = "server"
	hide()

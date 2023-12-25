extends Node3D

@onready var voice : VoiceInstance3D = $voice

func _ready() -> void :
	if get_multiplayer_authority() != 1 :
		# try disabling a microphone in clients
		voice.use_microphone = false

func _process(delta: float) -> void :
	position.x += Input.get_axis(&"ui_left", &"ui_right")

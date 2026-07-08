extends Node
var frames := 0
func _ready():
	DisplayServer.window_set_size(Vector2i(2200, 1320)) # 2x design
	var main: Node = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
func _process(_d):
	frames += 1
	if frames == 80:
		get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/stretch2x.png")
		get_tree().quit(0)

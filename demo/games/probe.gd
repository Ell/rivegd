extends Node
var frames := 0
var main: Node
const SEQ := ["menu", "breakout", "jrpg", "fps"]
var idx := 0
func _ready():
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
func _process(_d):
	frames += 1
	if frames % 70 == 0:
		var img := get_viewport().get_texture().get_image()
		img.save_png(ProjectSettings.globalize_path("res://") + "../out/game_%s.png" % SEQ[idx])
		idx += 1
		if idx >= SEQ.size():
			get_tree().quit(0)
		else:
			main.switch_to(SEQ[idx])

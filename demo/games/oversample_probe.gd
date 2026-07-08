extends Node
var c
var frames := 0
func _ready():
	get_tree().root.content_scale_factor = 2.0
	c = ClassDB.instantiate("RiveControl")
	c.set("file", load("res://fixtures/cards.riv"))
	c.set("artboard", "hud_panel")
	c.set("size", Vector2(240, 100))
	add_child(c)
func _process(_d):
	frames += 1
	if frames == 60:
		var tex := RenderingServer.texture_2d_get(c.call("get_texture_rid"))
		print("OVERSAMPLE texture=", tex.get_size())
		print("OVERSAMPLE screen_xform_scale=", c.get_screen_transform().get_scale())
		print("OVERSAMPLE final_xform_scale=", c.get_viewport().get_final_transform().get_scale())
		print("OVERSAMPLE canvas_xform_scale=", c.get_global_transform_with_canvas().get_scale())
		print("OVERSAMPLE win_factor=", get_tree().root.content_scale_factor)
		get_tree().quit(0)

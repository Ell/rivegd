extends Node2D
var s
var frames := 0
func _ready():
	DisplayServer.window_set_size(Vector2i(900, 300))
	s = ClassDB.instantiate("RiveSprite2D")
	s.set("file", load("res://fixtures/cards.riv"))
	s.set("artboard", "dialogue")
	s.set("size", Vector2i(820, 190))
	s.set("position", Vector2(20, 20))
	add_child(s)
	s.connect("state_changed", func(st): print("DLGPROBE state=", st))
	s.connect("property_changed", func(pa, va): print("DLGPROBE prop ", pa, "=", va))
	s.connect("loaded", func():
		s.call("watch_property", "shown")
		s.call("set_property", "shown", 1.0)
		s.call("set_property", "speaker", "TESTNAME")
		s.call("set_property", "line", "Test line of dialogue text."))
func _process(_d):
	frames += 1
	if frames == 60:
		get_viewport().get_texture().get_image().save_png(
			ProjectSettings.globalize_path("res://") + "../../out/dlg_probe.png")
		get_tree().quit(0)

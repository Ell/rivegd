extends Node
var frames := 0
var main: Node
func key(code: int) -> void:
	var ev := InputEventKey.new()
	ev.keycode = code
	ev.pressed = true
	Input.parse_input_event(ev)
	var up := InputEventKey.new()
	up.keycode = code
	up.pressed = false
	Input.parse_input_event(up)
func _ready():
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
	main.switch_to("jrpg")
func _process(_d):
	frames += 1
	match frames:
		30, 34, 38: key(KEY_W)      # walk toward Mira at (9,4)
		42, 46: key(KEY_A)
		60: key(KEY_E)               # talk
		90:
			var jrpg = main.current
			print("DLG TEXT: '", jrpg.dlg_text.text, "' visible=", jrpg.dlg_panel.visible)
			get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/play_dialogue.png")
			key(KEY_E)
		100: key(KEY_E)              # close (2 lines) -> +5 gold
		120: key(KEY_P)              # party screen
		150:
			get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/play_party.png")
			get_tree().quit(0)

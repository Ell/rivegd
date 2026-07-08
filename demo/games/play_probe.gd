extends Node
var frames := 0
var main: Node
func key_state(code: int, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.keycode = code
	ev.pressed = pressed
	Input.parse_input_event(ev)
func tap(code: int) -> void:
	key_state(code, true)
	key_state(code, false)
func _ready():
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
	main.switch_to("jrpg")
func _process(_d):
	frames += 1
	match frames:
		30: key_state(KEY_W, true)      # hold north toward Mira
		75: key_state(KEY_W, false)
		76: key_state(KEY_A, true)      # drift west
		100: key_state(KEY_A, false)
		110: tap(KEY_E)                  # talk
		160:
			var jrpg = main.current
			print("DLG visible=", jrpg.dlg_panel.visible, " talking=", jrpg.talking)
			get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/play_dialogue.png")
			tap(KEY_E)
		175: tap(KEY_E)                  # close (2 lines) -> +5 gold
		195: tap(KEY_P)                  # party screen
		225:
			get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/play_party.png")
			get_tree().quit(0)

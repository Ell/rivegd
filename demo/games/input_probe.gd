# Real-pipeline input test: synthetic mouse events through
# Input.parse_input_event (viewport -> gui -> unhandled), asserting each
# game's interactions actually fire. This is what a player's clicks do.
extends Node

var frames := 0
var main: Node
var failed := false


func click_at(pos: Vector2) -> void:
	var move := InputEventMouseMotion.new()
	move.position = pos
	move.global_position = pos
	get_viewport().push_input(move)
	var down := InputEventMouseButton.new()
	down.button_index = MOUSE_BUTTON_LEFT
	down.pressed = true
	down.position = pos
	down.global_position = pos
	get_viewport().push_input(down)
	var up := down.duplicate()
	up.pressed = false
	get_viewport().push_input(up)


func check(name: String, ok: bool) -> void:
	print("INPUTPROBE %s: %s" % [name, "OK" if ok else "FAIL"])
	if not ok:
		failed = true


func _ready() -> void:
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)


func _process(_d: float) -> void:
	frames += 1
	match frames:
		25:
			for child in main.current.get_children():
				if child is RiveControl:
					child.gui_input.connect(func(ev):
						print("INPUTPROBE card gui_input: ", ev))
					child.mouse_entered.connect(func():
						print("INPUTPROBE card hover"))
					break
		30:
			click_at(Vector2(206, 400))  # menu: BREAKOUT card
		60:
			check("menu click launches game", main.current.name == "breakout")
		75:
			click_at(Vector2(550, 500))  # breakout: launch ball
		85:
			check("breakout launch", main.current.playing == true)
			var esc := InputEventKey.new()
			esc.keycode = KEY_ESCAPE
			esc.pressed = true
			get_viewport().push_input(esc)
		100:
			check("escape to menu", main.current.name == "menu")
			click_at(Vector2(886, 400))  # menu: TERMINAL card
		130:
			check("fps launched", main.current.name == "fps")
			click_at(Vector2(550, 330))  # first click captures mouse
		140:
			check("fps mouse captured",
					Input.mouse_mode == Input.MOUSE_MODE_CAPTURED)
			# stand before terminal and click it for real
			var fps = main.current
			fps.body = Vector3(-0.6, 1.7, -2.0)
			fps.camera.global_position = fps.body
			fps.camera.global_transform.basis = Basis()
		145:
			click_at(Vector2(550, 330))
		160:
			check("fps click buys crate", main.current.blocks >= 1)
			print("INPUTPROBE DONE ", "FAIL" if failed else "ALL OK")
			get_tree().quit(1 if failed else 0)

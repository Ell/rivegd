extends Node
var frames := 0
var main: Node
func click() -> void:
	var down := InputEventMouseButton.new()
	down.button_index = MOUSE_BUTTON_LEFT
	down.pressed = true
	Input.parse_input_event(down)
var sweep_y := [0.5, 0.6, 0.7, 0.8, 0.86, 0.92]
var sweep_i := 0


func _ready():
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
	main.switch_to("fps")
func _process(_d):
	frames += 1
	var fps = main.current
	match frames:
		38:
			var fps0 = main.current
			print("FPSPROBE shop_items=", fps0.shop_items.size())
			for it in fps0.shop_items:
				print("FPSPROBE item pos=", it.position, " visible=", it.visible,
						" size=", it.size)
		40:
			# Stand in front of the terminal, face it dead on (-Z).
			fps.body = Vector3(-0.6, 1.7, -2.0)
			fps.yaw = 0.0
			fps.pitch = 0.0
			fps.camera.global_position = fps.body
			fps.camera.global_transform.basis = Basis()
			var uv = fps._screen_uv(fps.terminal_mesh, Vector2(1.2, 1.2))
			print("FPSPROBE terminal uv=", uv)
			fps._interact(1)
			fps._interact(2)
		64:
			main.current.switch_ui.state_changed.connect(func(state):
				print("FPSPROBE state=", state))
		70:
			# Face the switch screen, aimed low where the toggle sits
			# (light_switch's lever is at ~86% artboard height).
			fps.body = Vector3(3.5, 1.5 + 0.55 * 2.0 * (0.5 - 0.86), -3.0)
			fps.camera.global_position = fps.body
			fps.camera.global_transform.basis = Basis()
			var uv2 = fps._screen_uv(fps.switch_mesh, Vector2(0.55, 0.55))
			print("FPSPROBE switch uv=", uv2)
			fps._interact(1)
			fps._interact(2)
		80, 84, 88, 92, 96, 100:
			var fps2 = main.current
			var y: float = sweep_y[sweep_i]
			sweep_i += 1
			fps2.switch_ui.send_pointer_event(1, Vector2(128, y * 256.0))
			fps2.switch_ui.send_pointer_event(2, Vector2(128, y * 256.0))
		120:
			print("FPSPROBE blocks=", fps.blocks, " light=", fps.room_light.light_energy,
					" color=", fps.room_light.light_color)
			get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://") + "../out/fps_after.png")
			get_tree().quit(0)

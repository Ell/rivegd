extends Node
var frames := 0
var main: Node
var before: PackedByteArray
func _ready():
	DisplayServer.window_set_size(Vector2i(1100, 660))
	main = load("res://games/main.gd").new()
	main.name = "Games"
	add_child(main)
	main.switch_to("jrpg")
func drag_step(pos: Vector2, rel: Vector2, pressed_btn: bool) -> void:
	if pressed_btn:
		var down := InputEventMouseButton.new()
		down.button_index = MOUSE_BUTTON_LEFT
		down.pressed = true
		down.position = pos
		get_viewport().push_input(down)
	var mv := InputEventMouseMotion.new()
	mv.position = pos + rel
	mv.relative = rel
	mv.button_mask = MOUSE_BUTTON_MASK_LEFT
	get_viewport().push_input(mv)
func _process(_d):
	frames += 1
	match frames:
		20:
			main.current.party_panel.visible = true
		120:
			before = get_viewport().get_texture().get_image().get_region(
					Rect2i(560, 200, 370, 350)).get_data()
			drag_step(Vector2(740, 520), Vector2(0, -20), true)
		122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142, 144, 146, 148:
			drag_step(Vector2(740, 520 - (frames - 120) * 20), Vector2(0, -20), false)
		150:
			var up := InputEventMouseButton.new()
			up.button_index = MOUSE_BUTTON_LEFT
			up.pressed = false
			up.position = Vector2(740, 220)
			get_viewport().push_input(up)
		260:
			var after := get_viewport().get_texture().get_image().get_region(
					Rect2i(560, 200, 370, 350)).get_data()
			print("SCROLLPROBE moved=", before != after)
			get_viewport().get_texture().get_image().save_png(
					ProjectSettings.globalize_path("res://") + "../out/inv_scrolled.png")
			get_tree().quit(0)

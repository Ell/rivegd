extends Node2D
var c
var frames := 0
var before: PackedByteArray
func _ready():
	c = ClassDB.instantiate("RiveControl")
	c.set("file", load("res://fixtures/cards.riv"))
	c.set("artboard", OS.get_environment("PROBE_AB") if OS.get_environment("PROBE_AB") != "" else "inventory")
	c.set("position", Vector2(20, 20))
	c.set("size", Vector2(380, 420))
	add_child(c)
	c.connect("loaded", func():
		var ab: String = c.get("artboard")
		if ab == "inventory":
			for k in 30:
				c.call("list_append", "rows", "InvRowVM")
				c.call("list_set_property", "rows", k, "name", "Item %d" % k)
		else:
			for k in 12:
				c.call("list_append", "items", "CardVM")
				c.call("list_set_property", "items", k, "tint", Color.from_hsv(k * 0.08, 0.7, 0.9)))
func _process(_d):
	frames += 1
	match frames:
		90:
			before = get_viewport().get_texture().get_image().get_region(
					Rect2i(30, 40, 350, 380)).get_data()
			var down := InputEventMouseButton.new()
			down.button_index = MOUSE_BUTTON_LEFT
			down.pressed = true
			down.position = Vector2(200, 380)
			get_viewport().push_input(down)
		92, 94, 96, 98, 100, 102, 104, 106:
			var mv := InputEventMouseMotion.new()
			mv.position = Vector2(200, 380 - (frames - 90) * 20)
			mv.relative = Vector2(0, -40)
			mv.button_mask = MOUSE_BUTTON_MASK_LEFT
			get_viewport().push_input(mv)
		108:
			var up := InputEventMouseButton.new()
			up.button_index = MOUSE_BUTTON_LEFT
			up.pressed = false
			up.position = Vector2(200, 60)
			get_viewport().push_input(up)
		200:
			var after := get_viewport().get_texture().get_image().get_region(
					Rect2i(30, 40, 350, 380)).get_data()
			print("INVSCROLL moved=", before != after)
			get_viewport().get_texture().get_image().save_png(
					ProjectSettings.globalize_path("res://") + "../../out/invscroll.png")
			get_tree().quit(0)

# Phase 2 API smoke: ResourceFormatLoader, state-machine inputs, pointer
# forwarding, event signals. Saves before/after screenshots and asserts the
# rendered pixels change when an input changes.
#   godot --path tests/project api_smoke.tscn
extends Node2D

var control: RiveControl
var frames := 0
var image_a: Image
var events_seen := 0


func fail(msg: String) -> void:
	push_error("API SMOKE FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	# 1. Loader: .riv loads as a resource directly.
	var res := load("res://fixtures/light_switch.riv")
	if res == null or not (res is RiveFileResource):
		fail("loader did not return a RiveFileResource")
		return
	if res.get_artboard_names().is_empty():
		fail("no artboards")
		return

	# 2. Metadata: the 'On' bool input is described.
	var inputs: Array = res.get_input_descriptions("", "")
	var found := false
	for description in inputs:
		if description["name"] == "On" and description["type"] == "bool":
			found = true
	if not found:
		fail("input metadata missing 'On' bool (got %s)" % [inputs])
		return

	# 3. RiveControl renders it.
	control = RiveControl.new()
	control.file = res
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	add_child(control)
	control.rive_event.connect(func(_name, _properties): events_seen += 1)


func _screenshot() -> Image:
	return get_viewport().get_texture().get_image()


func _save(img: Image, name: String) -> void:
	img.save_png(ProjectSettings.globalize_path("res://").path_join(
			"../../out/" + name))


func _process(_delta: float) -> void:
	frames += 1
	if frames == 40:
		image_a = _screenshot()
		_save(image_a, "api_smoke_on.png")
		# 4. Input path: 'On' defaults to true — switch the lamp off.
		control.set_bool_input("On", false)
	elif frames == 80:
		var image_b := _screenshot()
		_save(image_b, "api_smoke_off.png")
		if image_a.get_data() == image_b.get_data():
			fail("pixels did not change after set_bool_input('On', false)")
			return
		# 5. Pointer path: click the switch (artboard coords 150,258 — same
		# as rive's own listener test) through the viewport; the pointer
		# listener toggles the lamp back on.
		var ab: Vector2 = control.file.get_artboard_size("")
		var s: float = min(control.size.x / ab.x, control.size.y / ab.y)
		var offset: Vector2 = (control.size - ab * s) / 2.0
		var target: Vector2 = control.global_position \
				+ Vector2(150, 258) * s + offset
		var down := InputEventMouseButton.new()
		down.button_index = MOUSE_BUTTON_LEFT
		down.pressed = true
		down.position = target
		down.global_position = target
		get_viewport().push_input(down)
		var up := down.duplicate()
		up.pressed = false
		get_viewport().push_input(up)
	elif frames == 120:
		var image_c := _screenshot()
		_save(image_c, "api_smoke_clicked.png")
		# The switch's pointer listener toggles the lamp back on.
		var image_b := Image.load_from_file(
				ProjectSettings.globalize_path("res://").path_join(
						"../../out/api_smoke_off.png"))
		if image_c.get_data() == image_b.get_data():
			fail("pixels did not change after simulated click (listener)")
			return
		print("events seen: ", events_seen)
		print("API SMOKE OK")
		get_tree().quit(0)

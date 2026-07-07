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
		_start_databind_phase()
	elif frames == 160:
		image_a = _screenshot()
		_save(image_a, "api_smoke_databind_before.png")
		# Click at artboard (75,75): a nested-artboard listener updates a
		# view model value that doubles a rectangle's width (mirrors rive's
		# data_binding_cycle_test).
		var ab: Vector2 = control.file.get_artboard_size("main-1")
		var s: float = min(control.size.x / ab.x, control.size.y / ab.y)
		var offset: Vector2 = (control.size - ab * s) / 2.0
		var target: Vector2 = control.global_position \
				+ Vector2(75, 75) * s + offset
		var down := InputEventMouseButton.new()
		down.button_index = MOUSE_BUTTON_LEFT
		down.pressed = true
		down.position = target
		down.global_position = target
		get_viewport().push_input(down)
		var up := down.duplicate()
		up.pressed = false
		get_viewport().push_input(up)
		# Also exercise the direct write path (unknown paths must be safe).
		control.set_property("nonexistent/path", 1.0)
	elif frames == 200:
		var image_b := _screenshot()
		_save(image_b, "api_smoke_databind_after.png")
		if image_a.get_data() == image_b.get_data():
			fail("pixels did not change after data-binding click")
			return
		# Hot reload: re-setting the data must rebuild the instance cleanly.
		control.file.set_data(control.file.get_data())
	elif frames == 240:
		if not control.file.is_valid():
			fail("file invalid after hot reload")
			return
		_start_texture_phase()
	elif frames == 280:
		image_a = _screenshot()
		_save(image_a, "api_smoke_texture_a.png")
	elif frames == 320:
		var image_b := _screenshot()
		_save(image_b, "api_smoke_texture_b.png")
		if image_a.get_data() == image_b.get_data():
			fail("RiveTexture is not animating inside a TextureRect")
			return
		_start_property_phase()
	elif frames == 350:
		# Baseline value must have been reported by the initial watch.
		if property_changes.is_empty():
			fail("no baseline property_changed for watched 'width'")
			return
		if not is_equal_approx(float(vm_sprite.get_property("width")), 100.0):
			fail("baseline get_property('width') = %s, expected 100" %
					[vm_sprite.get_property("width")])
			return
		vm_sprite.set_property("width", 42.0)
	elif frames == 380:
		if not is_equal_approx(float(vm_sprite.get_property("width")), 42.0):
			fail("get_property('width') = %s, expected 42 after set" %
					[vm_sprite.get_property("width")])
			return
		print("property changes: ", property_changes)
		print("API SMOKE OK")
		get_tree().quit(0)


var vm_sprite: RiveSprite2D
var property_changes: Array = []


func _start_property_phase() -> void:
	# Data-binding reads: watch a property, write it, observe the change
	# signal and the cached read (data_binding_test.riv: 'width' drives
	# bound_rect, starts at 100).
	vm_sprite = RiveSprite2D.new()
	vm_sprite.file = load("res://fixtures/data_binding_test.riv")
	vm_sprite.artboard = "artboard-1"
	add_child(vm_sprite)
	vm_sprite.property_changed.connect(func(path, value):
		property_changes.push_back([path, value]))
	vm_sprite.watch_property("width")
	# VM properties surface as inspector properties too.
	var has_db_property := false
	for property in vm_sprite.get_property_list():
		if property["name"] == "data_binding/width":
			has_db_property = true
	if not has_db_property:
		fail("data_binding/width missing from property list")


func _start_texture_phase() -> void:
	control.queue_free()
	control = null
	# Rive as a plain Texture2D in a stock TextureRect: no Rive node at all.
	var texture: RiveTexture = RiveTexture.new()
	texture.file = load("res://fixtures/bullet_man.riv")
	texture.render_size = Vector2i(400, 400)
	var rect := TextureRect.new()
	rect.texture = texture
	rect.position = Vector2(32, 32)
	rect.size = Vector2(400, 400)
	add_child(rect)


func _start_databind_phase() -> void:
	control.queue_free()
	control = RiveControl.new()
	control.file = load("res://fixtures/data_binding_test_3.riv")
	control.artboard = "main-1"
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	add_child(control)

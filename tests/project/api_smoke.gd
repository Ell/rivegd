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
		# Enum + trigger phase: artboard-2 has state.enum and trigger-prop.
		vm_sprite.artboard = "artboard-2"
		var enum_hint := ""
		for property in vm_sprite.get_property_list():
			if property["name"] == "data_binding/state":
				enum_hint = property["hint_string"]
		if not enum_hint.contains("state-green"):
			fail("enum dropdown hint missing values (got '%s')" % enum_hint)
			return
		property_changes.clear()
		vm_sprite.watch_property("state")
		vm_sprite.watch_property("trigger-prop")
		vm_sprite.set_property("state", "state-green")
		vm_sprite.fire_property_trigger("trigger-prop")
	elif frames == 420:
		var saw_enum := false
		var saw_trigger := false
		for change in property_changes:
			if change[0] == "state" and change[1] == "state-green":
				saw_enum = true
			if change[0] == "trigger-prop":
				saw_trigger = true
		if not saw_enum:
			fail("enum change not observed (changes: %s)" % [property_changes])
			return
		if not saw_trigger:
			fail("trigger fire not observed (changes: %s)" % [property_changes])
			return
		print("enum+trigger changes: ", property_changes)
		_start_list_phase()
	elif frames == 470:
		var size_changes: Array = []
		for change in property_changes:
			if change[0] == "Test List":
				size_changes.push_back(int(change[1]))
		# Baseline (fixture pre-populates the list), then +1 from our append.
		if size_changes.size() < 2 \
				or size_changes[-1] != size_changes[0] + 1:
			fail("unexpected list size changes: %s" % [size_changes])
			return
		# List item read (G4.4): write a scalar on the appended item, then
		# read it back through the async property mailbox.
		var last: int = size_changes[-1] - 1
		vm_sprite.list_set_property("Test List", last, "Test Num", 77.0)
		vm_sprite.list_read_property("Test List", last, "Test Num")
		list_item_key = "Test List[%d]/Test Num" % last
	elif frames == 490:
		var v = vm_sprite.get_property(list_item_key)
		if v == null or not is_equal_approx(float(v), 77.0):
			fail("list item read: %s = %s, expected 77" % [list_item_key, v])
			return
		print("list item read OK: ", list_item_key, " = ", v)
		vm_sprite.list_clear("Test List")
	elif frames == 500:
		if int(vm_sprite.get_property("Test List")) != 0:
			fail("list_clear did not empty the list (size %s)" %
					[vm_sprite.get_property("Test List")])
			return
		print("list size after ops: ", vm_sprite.get_property("Test List"))
		_start_artboard_phase()
	elif frames == 540:
		image_a = _screenshot()
		_save(image_a, "api_smoke_artboard_ch1.png")
		# Swap the bound artboard; pixels must change.
		vm_sprite.set_artboard_property("ab", "ch2")
		# Image writes to unknown paths must be safe (no bound image fixture
		# is available; the decode path is API-covered).
		vm_sprite.set_property("nonexistent_image", Image.create_empty(
				4, 4, false, Image.FORMAT_RGBA8))
	elif frames == 580:
		var image_b := _screenshot()
		_save(image_b, "api_smoke_artboard_ch2.png")
		if image_a.get_data() == image_b.get_data():
			fail("pixels did not change after set_artboard_property ch1->ch2")
			return
		_start_keyboard_phase()
	elif frames == 600:
		image_a = _screenshot()  # pre-focus baseline (fixture is static)
	elif frames == 620:
		# Instance is live now (post-M1 load/instantiate is async); focus a
		# keyboard-listening element.
		key_control.focus_previous_element()
	elif frames == 660:
		# Focus verification: focus_previous_element (frame 620) must have
		# produced a visible focus ring — pixels differ from the pre-focus
		# baseline taken at phase start. HONESTY NOTE: keyboard_listener.riv
		# drives its space-toggle through a SAMPLE-SIGNED Listener Action
		# Script, which never runs in our production-key build — so a
		# space-key behavioral assert is impossible with this fixture. Key
		# delivery (rt_key) is exercised (space is sent below, must not
		# crash); behavioral verification needs an editor-signed keyboard
		# fixture (tracked with the MCP fixture task).
		var image_b := _screenshot()
		_save(image_b, "api_smoke_keyboard_focused.png")
		if image_a.get_data() == image_b.get_data():
			fail("focus ring did not appear after focus_previous_element")
			return
		key_control.grab_focus()
		var down := InputEventKey.new()
		down.keycode = KEY_SPACE
		down.pressed = true
		get_viewport().push_input(down)
		var up := down.duplicate()
		up.pressed = false
		get_viewport().push_input(up)
	elif frames == 720:
		_start_audio_phase()
	elif frames > 720 and frames < 860:
		if audio_playback != null:
			audio_peak = max(audio_peak, audio_playback.get_last_peak())
	elif frames == 860:
		if audio_peak <= 0.0:
			fail("no audio flowed through RiveAudioStream (peak 0)")
			return
		print("audio peak observed: ", audio_peak)
		_start_gamepad_phase()
	elif frames == 900:
		print("gamepadLog: ", gamepad_control.get_property("gamepadLog"))
		_start_luau_phase()
	elif frames == 980:
		# Editor-signed Luau MUST have executed: the script rewrites 'label'
		# (default "-no-script-") to "script-init"/"scaled:<n>". Anything but
		# the default proves the VM ran the signed bytecode.
		var echo := str(probe_sprite.get_property("label"))
		if echo == "-no-script-" or echo == "":
			fail("Luau did not run (label='%s' — script bytecode rejected or "
					% echo + "scripting not compiled in)")
			return
		if not echo.begins_with("scaled:") and echo != "script-init":
			fail("Luau ran but wrote unexpected label='%s'" % echo)
			return
		# Now drive a specific value through the script's update loop.
		probe_sprite.set_property("box-scale", 5.0)
	elif frames == 1060:
		var echo := str(probe_sprite.get_property("label"))
		if echo != "scaled:5":
			fail("Luau update loop wrong (label='%s', expected 'scaled:5')"
					% echo)
			return
		print("LUAU BEHAVIORAL OK: ", echo)
		# Targeted callables (G4.3): on_property fires only for its path.
		on_prop_hits = 0
		on_prop_last = null
		probe_sprite.on_property("label", func(v):
			on_prop_hits += 1
			on_prop_last = v)
		probe_sprite.set_property("box-scale", 9.0)
	elif frames == 1140:
		# The label callable must have fired with the script's new value.
		if on_prop_hits == 0 or str(on_prop_last) != "scaled:9":
			fail("on_property('label') wrong (hits=%d last=%s)" %
					[on_prop_hits, on_prop_last])
			return
		print("ON_PROPERTY OK: ", on_prop_last)
		_start_visual_vm_phase()
	elif frames == 1220:
		image_a = _screenshot()
		_save(image_a, "api_smoke_vmvisual_base.png")
		visual_control.set_property("box-scale", 3.0)
		visual_control.set_property("box-color", Color.RED)
	elif frames == 1300:
		var image_b := _screenshot()
		_save(image_b, "api_smoke_vmvisual_changed.png")
		if image_a.get_data() == image_b.get_data():
			fail("vm-visual pixels unchanged after scale+color set")
			return
		print("API SMOKE OK")
		get_tree().quit(0)


var probe_sprite: RiveSprite2D
var on_prop_hits := 0
var on_prop_last = null
var visual_control: RiveControl


func _start_luau_phase() -> void:
	gamepad_control.queue_free()
	# The scripted node lives on "fixtures", bound to VisVM. It writes the
	# 'label' string: "script-init" on init, "scaled:<box-scale>" on change.
	probe_sprite = RiveSprite2D.new()
	probe_sprite.file = load("res://fixtures/rivegd_fixtures.riv")
	probe_sprite.artboard = "fixtures"
	probe_sprite.position = Vector2(32, 32)
	add_child(probe_sprite)
	probe_sprite.watch_property("label")


func _start_visual_vm_phase() -> void:
	visual_control = RiveControl.new()
	visual_control.file = load("res://fixtures/rivegd_fixtures.riv")
	visual_control.artboard = "fixtures"
	visual_control.position = Vector2(500, 32)
	visual_control.size = Vector2(450, 450)
	add_child(visual_control)


var gamepad_control: RiveControl


func _start_gamepad_phase() -> void:
	gamepad_control = RiveControl.new()
	gamepad_control.file = load("res://fixtures/gamepad_test.riv")
	gamepad_control.position = Vector2(32, 32)
	gamepad_control.size = Vector2(400, 400)
	add_child(gamepad_control)
	gamepad_control.watch_property("gamepadLog")
	# Hand-built wire-v2 batch: connect a standard pad, press south (A).
	var batch := PackedByteArray()
	batch.resize(4)
	batch.encode_u32(0, 2)              # version
	batch.push_back(0)                  # record: connected
	var at := batch.size()
	batch.resize(at + 4)
	batch.encode_u32(at, 0)             # deviceId 0
	batch.push_back(0)                  # mapping: standard
	batch.push_back(17)                 # nButtons
	batch.push_back(6)                  # nAxes
	batch.push_back(0)                  # padding
	for i in 17 + 6:                    # zeroed buttons + axes
		at = batch.size()
		batch.resize(at + 4)
		batch.encode_float(at, 0.0)
	batch.push_back(1)                  # record: update
	at = batch.size()
	batch.resize(at + 4)
	batch.encode_u32(at, 0)             # deviceId 0
	batch.push_back(1)                  # nChanges
	batch.push_back(0)                  # kind: button
	batch.push_back(0)                  # index: south
	at = batch.size()
	batch.resize(at + 4)
	batch.encode_float(at, 1.0)         # pressed
	gamepad_control.submit_gamepad_batch(batch)


var audio_playback: RiveAudioStreamPlayback
var audio_peak := 0.0


func _start_audio_phase() -> void:
	key_control.queue_free()
	# Rive audio: a state-machine timeline fires an AudioEvent; the shared
	# engine's PCM flows through RiveAudioStream into Godot's mixer.
	var player := AudioStreamPlayer.new()
	player.stream = RiveAudioStream.new()
	add_child(player)
	player.play()
	audio_playback = player.get_stream_playback()

	var sound_sprite := RiveSprite2D.new()
	sound_sprite.file = load("res://fixtures/sound2.riv")
	sound_sprite.position = Vector2(32, 32)
	add_child(sound_sprite)


var key_control: RiveControl


func _start_keyboard_phase() -> void:
	vm_sprite.queue_free()
	key_control = RiveControl.new()
	key_control.file = load("res://fixtures/keyboard_listener.riv")
	key_control.position = Vector2(32, 32)
	key_control.size = Vector2(400, 400)
	key_control.focus_mode = Control.FOCUS_ALL
	add_child(key_control)
	# Focus a keyboard-listening element (child index 5, as in rive's test).
	key_control.focus_previous_element()


func _start_artboard_phase() -> void:
	vm_sprite.queue_free()
	vm_sprite = RiveSprite2D.new()
	vm_sprite.file = load("res://fixtures/data_binding_artboards_test.riv")
	vm_sprite.artboard = "main"
	vm_sprite.position = Vector2(32, 32)
	vm_sprite.size = Vector2i(400, 400)
	add_child(vm_sprite)
	vm_sprite.set_artboard_property("ab", "ch1")


func _start_list_phase() -> void:
	vm_sprite.queue_free()
	vm_sprite = RiveSprite2D.new()
	vm_sprite.file = load("res://fixtures/data_bind_test_cmdq.riv")
	vm_sprite.artboard = "Test Artboard"
	add_child(vm_sprite)
	property_changes.clear()
	vm_sprite.property_changed.connect(func(path, value):
		property_changes.push_back([path, value]))
	vm_sprite.watch_property("Test List")
	vm_sprite.list_append("Test List", "Test All")


var vm_sprite: RiveSprite2D
var list_item_key := ""
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

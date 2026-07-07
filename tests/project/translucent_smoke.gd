# Listener-aware hit testing (hit_test_behavior = TRANSLUCENT, Unity
# "Translucent" equivalent): a click over an interactive Rive listener is
# captured by the RiveControl; a click over empty artboard space falls
# through to the Godot control behind it.
#   godot --path tests/project translucent_smoke.tscn   (needs a display)
extends Control

var control: RiveControl
var button: Button
var button_clicks := 0
var frames := 0
var image_a: Image
var switch_pos: Vector2
var empty_pos: Vector2


func fail(msg: String) -> void:
	push_error("TRANSLUCENT FAIL: " + msg)
	get_tree().quit(1)


func _click_at(pos: Vector2) -> void:
	for pressed in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = pressed
		ev.position = pos
		ev.global_position = pos
		get_viewport().push_input(ev)


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)

	# The "game" behind: a button covering the whole area.
	button = Button.new()
	button.text = "BEHIND"
	button.position = Vector2(32, 32)
	button.size = Vector2(400, 400)
	button.pressed.connect(func(): button_clicks += 1)
	add_child(button)

	control = RiveControl.new()
	control.file = load("res://fixtures/light_switch.riv")
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	control.hit_test_behavior = 1 # TRANSLUCENT
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 50:
		var ab: Vector2 = control.file.get_artboard_size("")
		var s: float = min(control.size.x / ab.x, control.size.y / ab.y)
		var offset: Vector2 = (control.size - ab * s) / 2.0
		switch_pos = control.position + Vector2(150, 258) * s + offset
		empty_pos = control.position + Vector2(12, 12) # no listener there
		image_a = get_viewport().get_texture().get_image()
		# 1. Click empty artboard space: must fall THROUGH to the button.
		_click_at(empty_pos)
	elif frames == 90:
		if button_clicks != 1:
			fail("empty-space click did not fall through (clicks=%d)"
					% button_clicks)
			return
		# 2. Click the switch listener: Rive captures, button must NOT.
		_click_at(switch_pos)
	elif frames == 150:
		if button_clicks != 1:
			fail("listener click leaked to the button (clicks=%d)"
					% button_clicks)
			return
		var image_b := get_viewport().get_texture().get_image()
		if image_a.get_data() == image_b.get_data():
			fail("listener click did not toggle the lamp")
			return
		print("TRANSLUCENT OK: listener captures, empty space falls through")
		get_tree().quit(0)

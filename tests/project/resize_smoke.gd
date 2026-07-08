# Resize preserves state (texture-only swap): light_switch's lamp state is
# STATE MACHINE state (not a replayed input) — toggle it OFF, resize the
# control past the debounce, and the lamp must STILL be off. The historical
# resize path recreated the whole instance, resetting the lamp to its
# default ON.
#   godot --path tests/project resize_smoke.tscn   (needs a display)
extends Control

var control: RiveControl
var frames := 0
var lum_on := -1.0
var lum_off := -1.0


func fail(msg: String) -> void:
	push_error("RESIZE FAIL: " + msg)
	get_tree().quit(1)


func _luminance() -> float:
	# Average luminance over the control's current rect.
	var img := get_viewport().get_texture().get_image()
	var total := 0.0
	var samples := 0
	var rect := Rect2(control.position, control.size)
	for y in range(int(rect.position.y) + 4, int(rect.end.y) - 4, 12):
		for x in range(int(rect.position.x) + 4, int(rect.end.x) - 4, 12):
			var c := img.get_pixel(x, y)
			total += c.r + c.g + c.b
			samples += 1
	return total / maxf(1.0, float(samples))


func _click_switch() -> void:
	var ab: Vector2 = control.file.get_artboard_size("")
	var s: float = min(control.size.x / ab.x, control.size.y / ab.y)
	var offset: Vector2 = (control.size - ab * s) / 2.0
	var target: Vector2 = control.position + Vector2(150, 258) * s + offset
	for pressed in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = pressed
		ev.position = target
		ev.global_position = target
		get_viewport().push_input(ev)


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	control = RiveControl.new()
	control.file = load("res://fixtures/light_switch.riv")
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 50:
		lum_on = _luminance() # 'On' defaults true: lamp bright
		_click_switch()       # toggle OFF (state machine state)
	elif frames == 90:
		lum_off = _luminance()
		if absf(lum_off - lum_on) < 0.01:
			fail("click did not toggle the lamp (on=%f off=%f)"
					% [lum_on, lum_off])
			return
		# Resize: debounce (0.3s) then texture-only swap.
		control.size = Vector2(300, 300)
	elif frames == 180: # well past the debounce at any framerate
		var lum_now := _luminance()
		# The lamp must still be OFF: closer to the off luminance than on.
		if absf(lum_now - lum_on) < absf(lum_now - lum_off):
			fail("state machine RESET on resize (lum now=%f on=%f off=%f)"
					% [lum_now, lum_on, lum_off])
			return
		# And the swapped texture must still be live (interactive check).
		_click_switch()
	elif frames == 230:
		var lum_back := _luminance()
		if absf(lum_back - lum_on) > absf(lum_back - lum_off):
			fail("post-resize click did not toggle back (lum=%f)" % lum_back)
			return
		print("RESIZE OK: state survived; post-resize input works")
		get_tree().quit(0)

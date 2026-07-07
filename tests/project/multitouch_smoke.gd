# Multitouch: two pointers must track independently. Pointer 1 presses the
# light_switch toggle and HOLDS; pointer 0 taps down+up elsewhere; pointer 1
# then releases on the switch. With per-pointer tracking the switch click
# completes and pixels change. If ids were collapsed into one pointer, the
# stray tap would have moved/released the press and no toggle would fire.
#   godot --path tests/project multitouch_smoke.tscn   (needs a display)
extends Control

var control: RiveControl
var frames := 0
var image_a: Image
var switch_pos: Vector2


func fail(msg: String) -> void:
	push_error("MULTITOUCH FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	control = RiveControl.new()
	control.file = load("res://fixtures/light_switch.riv")
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 50:
		image_a = get_viewport().get_texture().get_image()
		# Switch listener at artboard (150, 258) -> control-local coords.
		var ab: Vector2 = control.file.get_artboard_size("")
		var s: float = min(control.size.x / ab.x, control.size.y / ab.y)
		var offset: Vector2 = (control.size - ab * s) / 2.0
		switch_pos = Vector2(150, 258) * s + offset
		# Pointer 1: press the switch and hold.
		control.send_pointer_event(1, switch_pos, 1)  # 1 = down
	elif frames == 60:
		# Pointer 0: tap in a far corner while pointer 1 holds.
		control.send_pointer_event(1, Vector2(10, 10), 0)
		control.send_pointer_event(2, Vector2(10, 10), 0)  # 2 = up
	elif frames == 70:
		# Pointer 1: release on the switch -> the click must complete.
		control.send_pointer_event(2, switch_pos, 1)
	elif frames == 130:
		var image_b := get_viewport().get_texture().get_image()
		if image_a.get_data() == image_b.get_data():
			fail("held pointer's click did not toggle (ids collapsed?)")
			return
		print("MULTITOUCH OK: independent pointers tracked")
		get_tree().quit(0)

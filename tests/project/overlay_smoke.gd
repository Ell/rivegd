# Overlay-over-game pattern (the "Rive HUD over a Godot game" case):
# a RiveControl with mouse_filter=IGNORE must (a) keep animating/data-binding
# and (b) let clicks fall through to a Godot control behind it.
#   godot --path tests/project overlay_smoke.tscn   (needs a display for input)
extends Control

var button_clicks := 0
var overlay: RiveControl
var frame_a: Image
var frames := 0


func fail(msg: String) -> void:
	push_error("OVERLAY FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)

	# The "game": a plain Godot button, behind the overlay.
	var button := Button.new()
	button.text = "GAME BUTTON"
	button.position = Vector2(160, 200)
	button.size = Vector2(180, 80)
	button.pressed.connect(func(): button_clicks += 1)
	add_child(button)

	# The Rive HUD overlay, covering the button. IGNORE = display-only:
	# it animates but never captures input.
	overlay = RiveControl.new()
	overlay.file = load("res://fixtures/bullet_man.riv")
	overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(overlay)


func _screenshot() -> Image:
	return get_viewport().get_texture().get_image()


func _click_at(pos: Vector2) -> void:
	for pressed in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = pressed
		ev.position = pos
		ev.global_position = pos
		get_viewport().push_input(ev)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 40:
		frame_a = _screenshot()
	elif frames == 60:
		# (a) Click through the overlay onto the button behind it.
		_click_at(Vector2(250, 240))
	elif frames == 100:
		if button_clicks == 0:
			fail("click did not reach the Godot button through IGNORE overlay")
			return
		# (b) The overlay must still be animating (its pixels changed).
		var frame_b := _screenshot()
		if frame_a.get_data() == frame_b.get_data():
			fail("overlay not animating under mouse_filter=IGNORE")
			return
		print("OVERLAY OK: button_clicks=%d, overlay animating" % button_clicks)
		get_tree().quit(0)

# Declarative listener -> view model write (no scripts): the card artboard
# has a Click listener targeting card-bg that sets CardVM.tint to red.
# Clicking through the Godot input path must fire it: tint changes via
# watch_property AND the card visibly turns red.
#   godot --path tests/project clicklistener_smoke.tscn  (needs a display)
extends Control

var control: RiveControl
var frames := 0
var tints: Array = []
var before: PackedByteArray


func fail(msg: String) -> void:
	push_error("CLICKLISTENER FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	control = RiveControl.new()
	control.file = load("res://fixtures/cards.riv")
	control.artboard = "card"
	control.position = Vector2(32, 32)
	control.size = Vector2(240, 320)
	add_child(control)
	control.watch_property("tint")
	control.property_changed.connect(func(path, value):
		if path == "tint":
			tints.push_back(value))


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		before = get_viewport().get_texture().get_image().get_data()
		# Click the card center (card-bg covers most of the artboard).
		var center := control.position + control.size / 2.0
		for pressed in [true, false]:
			var ev := InputEventMouseButton.new()
			ev.button_index = MOUSE_BUTTON_LEFT
			ev.pressed = pressed
			ev.position = center
			ev.global_position = center
			get_viewport().push_input(ev)
	elif frames == 120:
		var red_seen := false
		for t in tints:
			if t is Color and t.r > 0.9 and t.g < 0.1:
				red_seen = true
		if not red_seen:
			fail("tint never reported red (changes: %s)" % [tints])
			return
		if get_viewport().get_texture().get_image().get_data() == before:
			fail("pixels unchanged after listener click")
			return
		print("CLICKLISTENER OK: click -> VM tint red (watch + pixels)")
		get_tree().quit(0)

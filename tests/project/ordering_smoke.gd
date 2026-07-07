# Ordering guarantees (guards the CommandQueue migration; headless-safe):
#  O1: multiple events reported in ONE advance arrive via rive_event in
#      reportedEventAt index order ("Footstep" then "Event 3").
#  O2: events from a later advance never arrive before an earlier advance's.
#  O3: property_changed for multiple watched paths changed in one frame
#      arrives in WATCH-registration order, not write order.
#  O4: commands posted from the main thread apply in post order (a write
#      then an overwrite in the same frame reads back as the overwrite).
#   godot --headless --path tests/project ordering_smoke.tscn
extends Node2D

var control: RiveControl
var sprite: RiveSprite2D
var frames := 0
var events: Array = []
var changes: Array = []


func fail(msg: String) -> void:
	push_error("ORDERING FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	control = RiveControl.new()
	control.file = load("res://fixtures/event_on_listener.riv")
	control.position = Vector2(0, 0)
	control.size = Vector2(500, 500)
	add_child(control)
	control.rive_event.connect(func(event_name, _properties):
		events.push_back(event_name))



func _click_artboard(target_control: RiveControl, artboard_point: Vector2) -> void:
	var ab: Vector2 = target_control.file.get_artboard_size("")
	var s: float = min(target_control.size.x / ab.x, target_control.size.y / ab.y)
	var offset: Vector2 = (target_control.size - ab * s) / 2.0
	# Programmatic injection (headless-safe; same path as _gui_input).
	var local: Vector2 = artboard_point * s + offset
	target_control.send_pointer_event(1, local) # down
	target_control.send_pointer_event(2, local) # up


func _process(_delta: float) -> void:
	frames += 1
	if frames == 20:
		# O1: one click fires two events in one advance.
		_click_artboard(control, Vector2(343, 116))
	elif frames == 60:
		if events != ["Footstep", "Event 3"]:
			fail("O1 event order within one advance: %s" % [events])
			return
		# O2: a second click's events must append strictly after.
		_click_artboard(control, Vector2(343, 116))
	elif frames == 100:
		if events != ["Footstep", "Event 3", "Footstep", "Event 3"]:
			fail("O2 cross-advance event order: %s" % [events])
			return
		_start_property_ordering()
	elif frames == 140:
		# Drop the baseline reports (one per watch, in watch order).
		if changes.size() < 2:
			fail("O3 baselines missing: %s" % [changes])
			return
		if changes[0][0] != "width" or changes[1][0] != "rotation":
			fail("O3 baseline order should match watch order: %s" % [changes])
			return
		changes.clear()
		# Write in REVERSE of watch order, same frame.
		sprite.set_property("rotation", 33.0)
		sprite.set_property("width", 77.0)
	elif frames == 180:
		# O3: change reports follow watch-registration order.
		var paths: Array = []
		for change in changes:
			paths.push_back(change[0])
		if paths != ["width", "rotation"]:
			fail("O3 change order (want watch order): %s" % [changes])
			return
		# O4: two writes to one path in one frame -> last one wins.
		sprite.set_property("width", 1.0)
		sprite.set_property("width", 2.0)
	elif frames == 220:
		if not is_equal_approx(float(sprite.get_property("width")), 2.0):
			fail("O4 post-order: width = %s, expected 2 (last write)" %
					[sprite.get_property("width")])
			return
		print("ORDERING OK")
		get_tree().quit(0)


func _start_property_ordering() -> void:
	sprite = RiveSprite2D.new()
	sprite.file = load("res://fixtures/data_binding_test.riv")
	sprite.artboard = "artboard-1"
	add_child(sprite)
	sprite.property_changed.connect(func(path, value):
		changes.push_back([path, value]))
	sprite.watch_property("width")
	sprite.watch_property("rotation")

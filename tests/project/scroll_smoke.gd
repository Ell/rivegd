# Interactive scroll (editor-authored ScrollConstraint on the cards grid):
# a pointer drag through Godot input must scroll the list content. The
# cards are identical tiles, so the assertion tracks the Y of the FIRST
# GAP line (grid background between card rows) in a fixed column — it
# shifts when the content scrolls.
#   godot --path tests/project scroll_smoke.tscn   (needs a display)
extends Node2D

var control: RiveControl
var frames := 0
var gap_before := -1


func fail(msg: String) -> void:
	push_error("SCROLL FAIL: " + msg)
	get_tree().quit(1)


func _first_gap_y() -> int:
	# Scan down column x=96 (inside card column 1) for the first horizontal
	# background run below the first card top.
	var img := get_viewport().get_texture().get_image()
	for y in range(40, 480):
		var c := img.get_pixel(96, 16 + y)
		if c.g < 0.4: # not card green -> gap/background line
			return y
	return -1


func _ready() -> void:
	control = RiveControl.new()
	control.file = load("res://fixtures/cards.riv")
	control.artboard = "cards"
	control.position = Vector2(16, 16)
	control.size = Vector2(512, 512)
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 40:
		for i in 30:
			control.list_append("items", "CardVM", "default")
	elif frames == 100:
		gap_before = _first_gap_y()
		if gap_before < 0:
			fail("no gap line found pre-drag (grid not populated?)")
			return
		control.send_pointer_event(1, Vector2(256, 400)) # down
	elif frames > 100 and frames <= 130:
		control.send_pointer_event(0, Vector2(256, 400 - (frames - 100) * 10))
	elif frames == 131:
		var gap_end := _first_gap_y()
		control.send_pointer_event(2, Vector2(256, 100)) # up
		if gap_end < 0 or absi(gap_end - gap_before) < 8:
			fail("content did not scroll (gap %d -> %d)"
					% [gap_before, gap_end])
			return
		print("SCROLL OK: drag moved content (gap %d -> %d)"
				% [gap_before, gap_end])
		get_tree().quit(0)

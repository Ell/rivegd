# Dynamic lists (the "inventory of Rive cards" case): cards.riv's `cards`
# artboard has a layout grid whose ArtboardComponentList is driven by
# ListVM.items — appending CardVM instances from Godot must make card
# components APPEAR in the layout, per-item writes must change them, and
# clear must empty the grid back to baseline.
#   godot --path tests/project cards_smoke.tscn   (needs a display)
extends Node2D

var sprite: Node
var frames := 0
var empty_grid: PackedByteArray
var filled_grid: PackedByteArray


func fail(msg: String) -> void:
	push_error("CARDS FAIL: " + msg)
	get_tree().quit(1)


func _shot() -> PackedByteArray:
	return get_viewport().get_texture().get_image().get_data()


func _ready() -> void:
	var res = load("res://fixtures/cards.riv")
	var names: PackedStringArray = res.get_artboard_names()
	if not ("cards" in names and "card" in names):
		fail("expected cards+card artboards, got %s" % [names])
		return
	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", res)
	sprite.set("artboard", "cards")
	sprite.set("size", Vector2i(512, 512))
	sprite.set("position", Vector2(16, 16))
	add_child(sprite)
	sprite.call("watch_property", "items")


func _process(_delta: float) -> void:
	frames += 1
	if frames == 50:
		empty_grid = _shot()
		# Append three cards from GDScript.
		for i in 3:
			sprite.call("list_append", "items", "CardVM", "default")
	elif frames == 110:
		if int(sprite.call("get_property", "items")) != 3:
			fail("list size %s after 3 appends" %
					[sprite.call("get_property", "items")])
			return
		filled_grid = _shot()
		if filled_grid == empty_grid:
			fail("cards did not appear in the layout after append")
			return
		# Per-item write: repaint card 1 red and shrink its bar.
		sprite.call("list_set_property", "items", 1, "tint", Color.RED)
		sprite.call("list_set_property", "items", 1, "value", 0.25)
	elif frames == 170:
		var changed := _shot()
		if changed == filled_grid:
			fail("per-item property write did not change the render")
			return
		sprite.call("list_clear", "items")
	elif frames == 230:
		if int(sprite.call("get_property", "items")) != 0:
			fail("list_clear left size %s" %
					[sprite.call("get_property", "items")])
			return
		if _shot() != empty_grid:
			fail("cleared grid does not match the empty baseline")
			return
		print("CARDS OK: append renders, per-item writes render, clear empties")
		get_tree().quit(0)

# Out-of-band assets (G3.6): walle.riv references two PNGs it does not
# embed. With the sibling files present (rive's export naming convention,
# "<name>-<id>.<ext>" next to the .riv), the runtime must resolve them
# through the CommandQueue global-asset registry and the image must be
# VISIBLE in the render. Compares content against a reference expectation:
# a sprite with unresolved assets renders geometry-only (much less content).
#   godot --path tests/project oob_smoke.tscn   (needs a display)
extends Node2D

var sprite: Node
var frames := 0


func fail(msg: String) -> void:
	push_error("OOB FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	var res = load("res://fixtures/oob/walle.riv")
	if res == null:
		fail("walle.riv did not load")
		return

	# Metadata: the file must report its referenced assets.
	var descriptions: Array = res.get_asset_descriptions()
	var found := 0
	for d in descriptions:
		if String(d["unique_filename"]).ends_with(".png") \
				and d["type"] == "image":
			found += 1
	if found < 2:
		fail("expected 2 referenced image assets, got %d (%s)"
				% [found, descriptions])
		return

	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", res)
	sprite.set("size", Vector2i(400, 400))
	sprite.set("position", Vector2(40, 40))
	add_child(sprite)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 80:
		# The referenced PNGs must be visible: count pixels that are not
		# background. An unresolved import shows only vector geometry (or
		# nothing); the walle/eve images fill a large area.
		var img := get_viewport().get_texture().get_image()
		var content := 0
		for y in range(40, 440, 4):
			for x in range(40, 440, 4):
				var c := img.get_pixel(x, y)
				if absf(c.r - 0.302) > 0.06 or absf(c.g - 0.302) > 0.06 \
						or absf(c.b - 0.302) > 0.06:
					content += 1
		if content < 500:
			fail("out-of-band images not visible (content px: %d)" % content)
			return
		print("OOB OK: referenced images resolved and visible (%d px)"
				% content)
		get_tree().quit(0)

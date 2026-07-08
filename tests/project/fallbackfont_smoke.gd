# Fallback fonts, behaviorally (G5.1): textbed's CJK line is authored with
# a latin-only font — glyphs are MISSING by construction. Registering a
# system CJK font via RiveFileResource.add_fallback_font must make the CJK
# line actually render on a fresh instance (shaping consults fallbacks).
#   godot --path tests/project fallbackfont_smoke.tscn
extends Node2D

const CJK_FONT := "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc"
const ARABIC_FONT := "/usr/share/fonts/noto/NotoKufiArabic-Regular.ttf"

var sprite: Node
var frames := 0
var before := 0
var before_ar := 0


func fail(msg: String) -> void:
	push_error("FALLBACKFONT FAIL: " + msg)
	get_tree().quit(1)


func _row_density(green: bool) -> int:
	# Row fill density. Missing glyphs render as SOLID tofu boxes (dense);
	# real glyphs are strokes (sparse) — fallback success = density DROPS
	# while the row keeps content.
	var img := get_viewport().get_texture().get_image()
	var content := 0
	for y in range(120, 290, 2):
		for x in range(18, 420, 2):
			var c := img.get_pixel(x, y)
			var hit := (c.g > 0.5 and c.r < 0.5) if green \
					else (c.b > 0.55 and c.r < 0.5)
			if hit:
				content += 1
	return content


func _make_sprite() -> void:
	if sprite != null:
		sprite.queue_free()
	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", load("res://fixtures/cards.riv"))
	sprite.set("artboard", "textbed")
	sprite.set("size", Vector2i(400, 300))
	sprite.set("position", Vector2(16, 16))
	add_child(sprite)


func _ready() -> void:
	if not FileAccess.file_exists(CJK_FONT) \
			or not FileAccess.file_exists(ARABIC_FONT):
		print("FALLBACKFONT SKIP: no system CJK font")
		get_tree().quit(0)
		return
	RiveFileResource.clear_fallback_fonts()
	_make_sprite()


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		before = _row_density(true)
		before_ar = _row_density(false)
		if before < 20 or before_ar < 20:
			fail("expected dense tofu rows pre-fallback (cjk=%d ar=%d)"
					% [before, before_ar])
			return
		# Register BOTH fallbacks (also exercises index iteration) and
		# re-instance for fresh shaping.
		if not RiveFileResource.add_fallback_font(
				FileAccess.get_file_as_bytes(CJK_FONT)):
			fail("add_fallback_font rejected NotoSansCJK")
			return
		if not RiveFileResource.add_fallback_font(
				FileAccess.get_file_as_bytes(ARABIC_FONT)):
			fail("add_fallback_font rejected NotoKufiArabic")
			return
		_make_sprite()
	elif frames == 130:
		# Tofu (solid boxes) -> real glyphs (strokes): density drops but
		# content remains.
		var after := _row_density(true)
		var after_ar := _row_density(false)
		if not (after < before and after > 5):
			fail("CJK row not re-shaped (before=%d after=%d)" % [before, after])
			return
		if not (after_ar < before_ar and after_ar > 5):
			fail("Arabic row not re-shaped (before=%d after=%d)"
					% [before_ar, after_ar])
			return
		print("FALLBACKFONT OK: cjk %d->%d, arabic(RTL) %d->%d" %
				[before, after, before_ar, after_ar])
		RiveFileResource.clear_fallback_fonts()
		get_tree().quit(0)

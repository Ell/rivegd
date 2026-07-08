# Web export smoke (G2.3): runs INSIDE the browser. Asserts the extension
# initialized, a .riv loads, the artboard renders through WebGL2 (pixel
# readback), and data binding round-trips. Verdicts print to the browser
# console (Playwright reads them).
extends Node2D

var sprite: Node
var frames := 0
var image_a: Image


func verdict(msg: String) -> void:
	print(msg) # lands in the JS console on web builds


func _ready() -> void:
	if not ClassDB.class_exists("RiveSprite2D"):
		verdict("WEB SMOKE FAIL: extension classes missing")
		return
	var res = load("res://fixtures/cards.riv")
	if res == null or not res.is_valid():
		verdict("WEB SMOKE FAIL: cards.riv did not load")
		return
	verdict("WEB SMOKE: file loaded, artboards=%s" % [res.get_artboard_names()])
	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", res)
	sprite.set("artboard", "card")
	sprite.set("size", Vector2i(240, 320))
	sprite.set("position", Vector2(20, 20))
	add_child(sprite)
	sprite.call("watch_property", "tint")


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		image_a = get_viewport().get_texture().get_image()
		# Render check: the card must have drawn something non-uniform.
		var colors := {}
		for y in range(20, 340, 12):
			for x in range(20, 260, 12):
				colors[image_a.get_pixel(x, y).to_html()] = true
		if colors.size() < 2:
			verdict("WEB SMOKE FAIL: frame is uniform (no render)")
			return
		verdict("WEB SMOKE: rendering (%d distinct colors)" % colors.size())
		sprite.call("set_property", "tint", Color(1, 0, 1)) # magenta
	elif frames == 120:
		var t = sprite.call("get_property", "tint")
		if not (t is Color and t.b > 0.9 and t.r > 0.9):
			verdict("WEB SMOKE FAIL: data binding round-trip (tint=%s)" % [t])
			return
		var image_b := get_viewport().get_texture().get_image()
		if image_a.get_data() == image_b.get_data():
			verdict("WEB SMOKE FAIL: pixels unchanged after tint write")
			return
		verdict("WEB SMOKE OK: load + render + data binding on web")

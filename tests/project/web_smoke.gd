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
		# Live texture on web: a shader-driven SubViewport bound into a
		# Rive image property must animate without rebinding (GL adopt).
		_start_live_phase()
	elif frames == 180:
		live_a = get_viewport().get_texture().get_image().get_data()
	elif frames == 240:
		var live_b := get_viewport().get_texture().get_image().get_data()
		if live_a == live_b:
			verdict("WEB SMOKE FAIL: live texture frozen on web")
			return
		verdict("WEB SMOKE OK: load + render + data binding + live texture")


var live_sprite: Node
var live_a: PackedByteArray


func _start_live_phase() -> void:
	sprite.queue_free()
	var vp := SubViewport.new()
	vp.size = Vector2i(128, 128)
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = "shader_type canvas_item; void fragment() { COLOR = vec4(abs(sin(TIME*3.0)), fract(TIME*0.5), abs(cos(TIME*2.0)), 1.0); }"
	mat.shader = sh
	rect.material = mat
	vp.add_child(rect)
	add_child(vp)
	live_sprite = ClassDB.instantiate("RiveSprite2D")
	live_sprite.set("file", load("res://fixtures/data_binding_images_test.riv"))
	live_sprite.set("artboard", "main")
	live_sprite.set("size", Vector2i(300, 300))
	live_sprite.set("position", Vector2(20, 20))
	add_child(live_sprite)
	live_sprite.call("set_property", "main_im", vp)

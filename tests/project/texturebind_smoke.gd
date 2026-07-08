# Live GPU texture -> Rive image property: a shader-animated SubViewport
# texture bound ONCE via set_property must (a) visibly appear in the
# artboard's image element and (b) keep animating with no further writes —
# rive samples the adopted VkImage in place. Static Image binding is also
# checked (PNG path).
#   godot --path tests/project texturebind_smoke.tscn   (needs a display)
extends Node2D

var sprite: Node
var vp: SubViewport
var frames := 0
var baseline: PackedByteArray
var bound_a: PackedByteArray


func fail(msg: String) -> void:
	push_error("TEXTUREBIND FAIL: " + msg)
	get_tree().quit(1)


func _shot() -> PackedByteArray:
	return get_viewport().get_texture().get_image().get_data()


func _ready() -> void:
	vp = SubViewport.new()
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

	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", load("res://fixtures/data_binding_images_test.riv"))
	sprite.set("artboard", "main")
	sprite.set("size", Vector2i(400, 400))
	sprite.set("position", Vector2(16, 16))
	add_child(sprite)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 40:
		baseline = _shot()
		sprite.call("set_property", "main_im", vp.get_texture())
	elif frames == 100:
		bound_a = _shot()
		if bound_a == baseline:
			fail("bound texture not visible in the artboard")
			return
	elif frames == 160:
		if _shot() == bound_a:
			fail("live texture froze (should animate without rebinding)")
			return
		# Static path: a plain Image must still bind (and stay static).
		var img := Image.create_empty(64, 64, false, Image.FORMAT_RGBA8)
		img.fill(Color.WHITE)
		sprite.call("set_property", "main_im", img)
	elif frames == 220:
		bound_a = _shot()
	elif frames == 260:
		if _shot() != bound_a:
			fail("static Image binding is unexpectedly animating")
			return
		# Portrait case: a plain ImageTexture (RD-backed, static) — the
		# common "dialogue portrait" texture — must adopt and display.
		var img := Image.create_empty(64, 64, false, Image.FORMAT_RGBA8)
		img.fill(Color.ORANGE)
		sprite.call("set_property", "main_im", ImageTexture.create_from_image(img))
	elif frames == 320:
		if _shot() == bound_a:
			fail("ImageTexture portrait bind not visible")
			return
		print("TEXTUREBIND OK: live viewport + static image + portrait texture")
		get_tree().quit(0)

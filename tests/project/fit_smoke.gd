# Fit modes (video-parity with rive-unity's widget Fit setting): CONTAIN
# letterboxes a square artboard in a wide control; FILL stretches content
# into the letterbox region; LAYOUT resizes the artboard itself to the
# texture. Also asserts the `loaded` signal fires.
#   godot --path tests/project fit_smoke.tscn   (needs a display)
extends Control

const FIT_CONTAIN := 0
const FIT_FILL := 2
const FIT_LAYOUT := 7

var control: RiveControl
var frames := 0
var loads := 0
var contain_left: PackedColorArray
var layout_full: PackedColorArray


func fail(msg: String) -> void:
	push_error("FIT FAIL: " + msg)
	get_tree().quit(1)


func _sample_full(img: Image) -> PackedColorArray:
	var out := PackedColorArray()
	for y in range(36, 268, 8):
		for x in range(36, 628, 8):
			out.append(img.get_pixel(x, y))
	return out


func _sample_left(img: Image) -> PackedColorArray:
	# The left letterbox band of the wide control (square artboard,
	# contain-fit leaves this empty).
	var out := PackedColorArray()
	for y in range(60, 240, 10):
		for x in range(36, 90, 6):
			out.append(img.get_pixel(x, y))
	return out


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	control = RiveControl.new()
	control.file = load("res://fixtures/light_switch.riv")
	control.position = Vector2(32, 32)
	control.size = Vector2(600, 240) # wide: square artboard letterboxes
	control.fit = FIT_CONTAIN
	control.loaded.connect(func(): loads += 1)
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 50:
		if loads == 0:
			fail("loaded signal never fired")
			return
		contain_left = _sample_left(get_viewport().get_texture().get_image())
		control.fit = FIT_FILL
	elif frames == 110:
		var fill_left := _sample_left(get_viewport().get_texture().get_image())
		if fill_left == contain_left:
			fail("FILL renders identically to CONTAIN in the letterbox band")
			return
		control.fit = FIT_LAYOUT
	elif frames == 170:
		var img := get_viewport().get_texture().get_image()
		if _sample_left(img) == contain_left:
			fail("LAYOUT renders identically to CONTAIN in the letterbox band")
			return
		if loads < 3:
			fail("loaded should re-fire per recreate (got %d)" % loads)
			return
		layout_full = _sample_full(img)
		control.layout_scale = 2.0
	elif frames == 230:
		var scaled_full := _sample_full(
				get_viewport().get_texture().get_image())
		if scaled_full == layout_full:
			fail("layout_scale=2 renders identically to 1")
			return
		print("FIT OK: contain/fill/layout/layout_scale render distinctly")
		get_tree().quit(0)

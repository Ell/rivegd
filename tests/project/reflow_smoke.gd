# FIT_LAYOUT reflow (responsive artboard): fixed 48px header + 25% sidebar
# + fill content. Under Layout fit at 600x200 the header must still be
# ~48px tall and the sidebar exactly 25% wide — proportions authored in
# Rive layout, reflowed to the node size (not scaled).
#   godot --path tests/project reflow_smoke.tscn
extends Control

var control: RiveControl
var frames := 0


func fail(msg: String) -> void:
	push_error("REFLOW FAIL: " + msg)
	get_tree().quit(1)


func _pix(x: float, y: float) -> Color:
	return get_viewport().get_texture().get_image().get_pixel(
			int(control.position.x + x), int(control.position.y + y))


func _reddish(c: Color) -> bool:
	return c.r > 0.5 and c.g < 0.4


func _ready() -> void:
	set_anchors_preset(Control.PRESET_FULL_RECT)
	control = RiveControl.new()
	control.file = load("res://fixtures/cards.riv")
	control.artboard = "responsive"
	control.fit = 7 # Layout
	control.position = Vector2(32, 32)
	control.size = Vector2(600, 200) # very different from authored 400x300
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		print("control.size=", control.size, " min=", control.get_minimum_size())
		get_viewport().get_texture().get_image().save_png(
				ProjectSettings.globalize_path("res://").path_join(
						"../../out/reflow_debug.png"))
		# Header: fixed 48px regardless of node size.
		if not _reddish(_pix(300, 30)):
			fail("no header at y=30 (got %s)" % [_pix(300, 30)])
			return
		if _reddish(_pix(300, 70)):
			fail("header taller than authored 48px — scaled, not reflowed")
			return
		# Sidebar: 25% of 600 = 150px wide.
		var side := _pix(100, 130)
		var content := _pix(220, 130)
		if not (side.b > 0.5 and side.r < 0.4):
			fail("sidebar not at 25%% width (x=100: %s)" % [side])
			return
		if content.b > 0.5 and content.r < 0.4:
			fail("sidebar wider than 25%% (x=220 still blue: %s)" % [content])
			return
		print("REFLOW OK: header 48px, sidebar 25% at 600x200")
		get_tree().quit(0)

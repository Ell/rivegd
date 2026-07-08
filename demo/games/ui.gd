# Shared look for the collection: pixel font, dark panels, one palette.
class_name GameUI

const BG := Color("12141c")


static func font() -> FontFile:
	return load("res://games/assets/kenpixel.ttf")


static func title_font() -> FontFile:
	return load("res://games/assets/kenvector_future.ttf")


static func label(text: String, size: int, pos: Vector2, color := Color.WHITE,
		use_title := false) -> Label:
	var l := Label.new()
	l.text = text
	l.position = pos
	l.add_theme_font_override("font", title_font() if use_title else font())
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.6))
	l.add_theme_constant_override("shadow_offset_y", 2)
	return l


static func panel(pos: Vector2, panel_size: Vector2, border := Color("4a5169"),
		bg := Color(0.09, 0.1, 0.14, 0.96)) -> PanelContainer:
	var p := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = bg
	style.border_color = border
	style.set_border_width_all(2)
	style.set_corner_radius_all(10)
	style.shadow_color = Color(0, 0, 0, 0.45)
	style.shadow_size = 10
	p.add_theme_stylebox_override("panel", style)
	p.position = pos
	p.custom_minimum_size = panel_size
	p.size = panel_size
	return p


static func vignette(parent: Control) -> void:
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = """
shader_type canvas_item;
void fragment() {
	vec2 d = UV - vec2(0.5);
	float v = smoothstep(0.35, 0.95, length(d) * 1.4);
	COLOR = vec4(0.0, 0.0, 0.02, v * 0.55);
}"""
	mat.shader = sh
	rect.material = mat
	parent.add_child(rect)

extends Node3D
var frames := 0
func _ready():
	var vp := SubViewport.new()
	vp.size = Vector2i(256, 256)
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(vp)
	var rect := ColorRect.new()
	rect.color = Color.ORANGE
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	vp.add_child(rect)
	var lbl := Label.new()
	lbl.text = "HELLO 3D"
	lbl.add_theme_font_size_override("font_size", 40)
	lbl.position = Vector2(30, 100)
	vp.add_child(lbl)
	var cam := Camera3D.new()
	cam.position = Vector3(0, 0, 3)
	add_child(cam)
	var spr := Sprite3D.new()
	spr.texture = vp.get_texture()
	spr.pixel_size = 0.01
	spr.shaded = false
	add_child(spr)
	var quad := MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = Vector2(2, 2)
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = vp.get_texture()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	qm.material = mat
	quad.mesh = qm
	quad.position = Vector3(2.2, 0, 0)
	add_child(quad)
func _process(_d):
	frames += 1
	pass

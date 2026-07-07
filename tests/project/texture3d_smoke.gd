# Rive-on-3D interaction: a RiveTexture on a sphere's material, clicked
# through a camera raycast -> hit point -> analytic sphere UV ->
# send_pointer_uv. The artboard's pointer listener (light_switch's toggle)
# must fire and visibly change the projected texture.
#   godot --path tests/project texture3d_smoke.tscn   (needs a display)
extends Node3D

var tex: RiveTexture
var sphere_body: StaticBody3D
var camera: Camera3D
var frames := 0
var image_a: Image


func fail(msg: String) -> void:
	push_error("TEXTURE3D FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	camera = Camera3D.new()
	camera.position = Vector3(0, 0, 4)
	add_child(camera)

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-30, 20, 0)
	add_child(light)

	# The Rive texture: light_switch has a pointer listener on the switch.
	tex = RiveTexture.new()
	tex.file = load("res://fixtures/light_switch.riv")
	tex.render_size = Vector2i(512, 512)

	var material := StandardMaterial3D.new()
	material.albedo_texture = tex
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED

	var mesh := SphereMesh.new()
	mesh.radius = 1.0
	mesh.height = 2.0
	mesh.material = material

	var mesh_node := MeshInstance3D.new()
	mesh_node.mesh = mesh
	add_child(mesh_node)

	# Collider for the raycast (same sphere).
	sphere_body = StaticBody3D.new()
	var shape := CollisionShape3D.new()
	shape.shape = SphereShape3D.new()
	shape.shape.radius = 1.0
	sphere_body.add_child(shape)
	add_child(sphere_body)


# Godot SphereMesh UV mapping (its generator): u wraps atan2(x, z) starting
# at +Z going clockwise seen from +Y; v runs 0 at the top pole to 1 at the
# bottom.
func _sphere_uv(local: Vector3) -> Vector2:
	var n := local.normalized()
	var u := 1.0 - (atan2(n.x, n.z) / TAU + 0.5)
	var v := acos(clampf(n.y, -1.0, 1.0)) / PI
	return Vector2(u, v)


func _click_artboard(artboard_point: Vector2) -> bool:
	# Artboard point -> UV on the sphere's FRONT hemisphere. The contain-fit
	# in send_pointer_uv maps texture<->artboard 1:1 here (square texture,
	# square artboard), so uv = point / artboard_size. To hit that uv on the
	# sphere we invert _sphere_uv for the front face and raycast at it.
	var ab: Vector2 = tex.file.get_artboard_size("")
	var uv := artboard_point / ab
	# Invert the mapping: pick the 3D point on the sphere with that UV.
	var phi := (1.0 - uv.x - 0.5) * TAU  # atan2(x, z)
	var theta := uv.y * PI               # from +Y
	var local := Vector3(sin(theta) * sin(phi), cos(theta),
			sin(theta) * cos(phi))
	if local.z < 0.05:
		return false # not on the camera-facing hemisphere
	# Raycast from the camera through that world point (validates the whole
	# pick chain rather than trusting the math above).
	var from := camera.global_position
	var dir := (sphere_body.to_global(local) - from).normalized()
	var params := PhysicsRayQueryParameters3D.create(from, from + dir * 20.0)
	var hit := get_world_3d().direct_space_state.intersect_ray(params)
	if hit.is_empty():
		return false
	var hit_uv := _sphere_uv(sphere_body.to_local(hit.position))
	tex.send_pointer_uv(1, hit_uv) # down
	tex.send_pointer_uv(2, hit_uv) # up
	return true


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		image_a = get_viewport().get_texture().get_image()
	elif frames == 70:
		# light_switch's toggle listener lives at artboard (150, 258).
		if not _click_artboard(Vector2(150, 258)):
			fail("raycast/uv pick missed the sphere")
			return
	elif frames == 130:
		var image_b := get_viewport().get_texture().get_image()
		if image_a.get_data() == image_b.get_data():
			fail("projected texture unchanged after 3D-picked click")
			return
		print("TEXTURE3D OK: click on sphere toggled the Rive listener")
		get_tree().quit(0)

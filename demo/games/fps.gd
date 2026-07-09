# First-person room where Rive artboards are projected DIRECTLY onto
# world-space geometry — no SubViewports. Each screen is a hidden
# RiveControl whose render texture binds into an ImageTexture via
# RenderingServer.texture_replace, shown on a Sprite3D; this path works
# on every renderer (Vulkan, GL, web). Crosshair raycasts map clicks
# back into artboard space, so Rive's own listeners fire from 3D aim.
extends Control

var world: Node3D
var camera: Camera3D
var body := Vector3(0, 1.6, 4)
var yaw := 0.0
var pitch := 0.0
var room_light: OmniLight3D
var shop_items: Array = []
var switch_ui: RiveControl
var terminal_center := Vector3(0, 1.55, -5.82)
var lamp_on := true
var blocks := 0
var info: Label
var last_click_ms := 0


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	world = Node3D.new()
	add_child(world)
	camera = Camera3D.new()
	world.add_child(camera)
	camera.make_current()
	room_light = OmniLight3D.new()
	room_light.position = Vector3(0, 3.4, 0)
	room_light.omni_range = 18.0
	room_light.light_energy = 2.4
	world.add_child(room_light)
	var env := WorldEnvironment.new()
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color(0.02, 0.02, 0.03)
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color(0.45, 0.45, 0.55)
	environment.ambient_light_energy = 1.1
	env.environment = environment
	world.add_child(env)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-55, 35, 0)
	key.light_energy = 0.8
	world.add_child(key)

	var wall_mat := StandardMaterial3D.new()
	wall_mat.albedo_texture = _panel_texture(Color(0.22, 0.24, 0.29), Color(0.14, 0.15, 0.19))
	wall_mat.uv1_scale = Vector3(6, 3, 1)
	var floor_mat := StandardMaterial3D.new()
	floor_mat.albedo_texture = _panel_texture(Color(0.27, 0.29, 0.34), Color(0.17, 0.18, 0.22))
	floor_mat.uv1_scale = Vector3(8, 8, 1)
	_box(Vector3(0, -0.1, 0), Vector3(12, 0.2, 12), floor_mat)
	_box(Vector3(0, 2, -6), Vector3(12, 4.4, 0.2), wall_mat)
	_box(Vector3(0, 2, 6), Vector3(12, 4.4, 0.2), wall_mat)
	_box(Vector3(-6, 2, 0), Vector3(0.2, 4.4, 12), wall_mat)
	_box(Vector3(6, 2, 0), Vector3(0.2, 4.4, 12), wall_mat)

	# --- Terminal: the shop, Rive projected straight onto the wall ---
	var header := Label3D.new()
	header.text = "RIVETON SUPPLY CO."
	header.font = load("res://games/assets/kenvector_future.ttf")
	header.font_size = 60
	header.pixel_size = 0.004
	header.modulate = Color(0.5, 0.9, 0.7)
	header.position = Vector3(0, 2.85, -5.8)
	world.add_child(header)
	_box(Vector3(0, 1.55, -5.88), Vector3(2.85, 2.35, 0.08), wall_mat) # bezel
	for i in 4:
		var item := RiveControl.new()
		item.file = load("res://fixtures/cards.riv")
		item.artboard = "shop_item"
		item.size = Vector2(300, 420)
		item.visible = false
		item.pause_when_hidden = false
		add_child(item)
		item.loaded.connect(_place_item.bind(item, i), CONNECT_ONE_SHOT)
		shop_items.push_back(item)

	# --- Light switch: its own Rive listener toggles the room ---
	switch_ui = RiveControl.new()
	switch_ui.file = load("res://fixtures/light_switch.riv")
	switch_ui.size = Vector2(512, 512)
	switch_ui.visible = false
	switch_ui.pause_when_hidden = false
	add_child(switch_ui)
	switch_ui.loaded.connect(func():
		var spr := _rive_sprite(switch_ui.get_texture_rid(), Vector2i(512, 512),
				1.1 / 512.0, Vector3(3.5, 1.5, -5.82))
		spr.name = "switch_sprite", CONNECT_ONE_SHOT)
	_box(Vector3(3.5, 1.5, -5.88), Vector3(1.3, 1.3, 0.08), wall_mat) # bezel
	switch_ui.state_changed.connect(func(_state):
		lamp_on = not lamp_on
		room_light.light_energy = 2.4 if lamp_on else 0.5
		room_light.light_color = Color(1.0, 0.95, 0.8) if lamp_on \
				else Color(0.4, 0.45, 0.7))

	var pedestal_mat := StandardMaterial3D.new()
	pedestal_mat.albedo_color = Color(0.34, 0.37, 0.45)
	pedestal_mat.roughness = 0.85
	_box(Vector3(-3, 0.4, -3), Vector3(1, 0.8, 1), pedestal_mat)

	var cross := GameUI.label("+", 22, Vector2.ZERO, Color(1, 1, 1, 0.8))
	cross.set_anchors_preset(Control.PRESET_CENTER)
	add_child(cross)
	info = GameUI.label("CLICK captures mouse   WASD move   click the wall screens   ESC menu",
			13, Vector2(230, 12), Color(1, 1, 1, 0.55))
	add_child(info)


func _rive_sprite(texture_rid: RID, tex_size: Vector2i, pixel_size: float,
		pos: Vector3) -> Sprite3D:
	# Bind the rive render texture into a plain ImageTexture: after
	# texture_replace, every Texture2D consumer presents rive live.
	var itex := ImageTexture.create_from_image(
			Image.create_empty(tex_size.x, tex_size.y, false, Image.FORMAT_RGBA8))
	# KNOWN ISSUE (task #32): on web GL the 3D pass samples rive-rendered
	# textures as blank — 2D canvas presents the same textures fine, and
	# CPU readback is also unavailable there. Desktop renders correctly.
	RenderingServer.texture_replace(itex.get_rid(), texture_rid)
	var spr := Sprite3D.new()
	spr.texture = itex
	spr.pixel_size = pixel_size
	spr.shaded = false
	# Rive textures carry no mip chain; the default mipmap filter makes
	# them incomplete (black) under strict WebGL2.
	spr.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR
	spr.position = pos
	world.add_child(spr)
	return spr


func _place_item(item: RiveControl, i: int) -> void:
	item.set_property("label", "CRATE %d" % (i + 1))
	item.set_property("price", "%d G" % (25 * (i + 1)))
	item.set_property("tint", Color.from_hsv(0.08 + i * 0.18, 0.75, 0.9))
	item.set_property("sold", 1.0)
	_rive_sprite(item.get_texture_rid(), Vector2i(300, 420), 0.55 / 420.0,
			Vector3(-0.68 + (i % 2) * 1.36, 1.98 - (i / 2) * 0.92, -5.82))


func _panel_texture(base: Color, line: Color) -> ImageTexture:
	var img := Image.create_empty(128, 128, false, Image.FORMAT_RGB8)
	img.fill(base)
	for i in 128:
		for edge in [0, 1, 126, 127]:
			img.set_pixel(i, edge, line)
			img.set_pixel(edge, i, line)
	for rivet_x in [8, 119]:
		for rivet_y in [8, 119]:
			for dx in range(-1, 2):
				for dy in range(-1, 2):
					img.set_pixel(rivet_x + dx, rivet_y + dy, line.lightened(0.25))
	return ImageTexture.create_from_image(img)


func _box(pos: Vector3, box_size: Vector3, mat: Material) -> MeshInstance3D:
	var m := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = box_size
	mesh.material = mat
	m.mesh = mesh
	m.position = pos
	world.add_child(m)
	return m


func _buy(index: int) -> void:
	blocks += 1
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _panel_texture(Color(0.85, 0.5, 0.15), Color(0.6, 0.32, 0.08))
	_box(Vector3(-3, 1.0 + (blocks - 1) * 0.4, -3), Vector3(0.4, 0.4, 0.4), mat)
	shop_items[index].set_property("sold", 0.3)
	shop_items[index].set_property("price", "SOLD")


func _wall_hit(center: Vector3, half: Vector2) -> Variant:
	var origin := camera.global_position
	var dir := -camera.global_transform.basis.z
	var plane_z: float = center.z + 0.01
	if absf(dir.z) < 0.0001:
		return null
	var t := (plane_z - origin.z) / dir.z
	if t <= 0 or t > 8.0:
		return null
	var hit := origin + dir * t
	var local := Vector2(hit.x - center.x, hit.y - center.y)
	if absf(local.x) > half.x or absf(local.y) > half.y:
		return null
	return Vector2(local.x / (half.x * 2) + 0.5, 0.5 - local.y / (half.y * 2))


func _interact(phase: int) -> void:
	var uv = _wall_hit(terminal_center, Vector2(1.36, 1.1))
	if uv != null:
		# 2x2 item grid: route the pointer into the item under the ray.
		var col := 0 if uv.x < 0.5 else 1
		var row := 0 if uv.y < 0.5 else 1
		var idx := row * 2 + col
		var item_uv := Vector2(fmod(uv.x, 0.5) * 2.0, fmod(uv.y, 0.5) * 2.0)
		shop_items[idx].send_pointer_event(phase, item_uv * Vector2(300, 420))
		if phase == 1:
			_buy(idx)
		return
	uv = _wall_hit(Vector3(3.5, 1.5, -5.82), Vector2(0.55, 0.55))
	if uv != null:
		switch_ui.send_pointer_event(phase, uv * 512.0)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed:
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
			return
		# Web pointer lock can re-dispatch the same click; duplicate
		# toggles cancel each other out, so collapse them.
		var now := Time.get_ticks_msec()
		if now - last_click_ms < 200:
			return
		last_click_ms = now
		_interact(1)
		_interact(2)
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		yaw -= event.relative.x * 0.003
		pitch = clamp(pitch - event.relative.y * 0.003, -1.2, 1.2)


func _process(delta: float) -> void:
	var fwd := Vector3(-sin(yaw), 0, -cos(yaw))
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	var wish := Vector3.ZERO
	if Input.is_key_pressed(KEY_W): wish += fwd
	if Input.is_key_pressed(KEY_S): wish -= fwd
	if Input.is_key_pressed(KEY_D): wish += right
	if Input.is_key_pressed(KEY_A): wish -= right
	body += wish.normalized() * 4.0 * delta if wish.length() > 0.1 else Vector3.ZERO
	body.x = clamp(body.x, -5.3, 5.3)
	body.z = clamp(body.z, -5.0, 5.3)
	camera.global_position = body
	camera.global_transform.basis = Basis.from_euler(Vector3(pitch, yaw, 0))
	_interact(0)

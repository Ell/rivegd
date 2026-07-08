# First-person room. A Rive UI is projected onto a terminal screen in the
# world (RiveTexture on a 3D mesh); looking at it and clicking raycasts
# the hit into UV space and forwards it to Rive listeners. The terminal's
# state drives the actual room: the lamp switch toggles the room lights,
# and each card you click spawns a block at the pedestal.
extends Control

var camera: Camera3D
var body := Vector3(0, 1.6, 4)
var yaw := 0.0
var pitch := 0.0
var term_vp: SubViewport
var shop_items: Array = []
var lamp_on := true
var switch_vp: SubViewport
var switch_ui: RiveControl
var terminal_mesh: MeshInstance3D
var switch_mesh: MeshInstance3D
var room_light: OmniLight3D
var world: Node3D
var blocks := 0
var info: Label
var shop_overlay: Control
var overlay_items: Array = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	# The room renders in the ROOT viewport (3D nodes join its World3D);
	# the Control UI draws over it. No 3D-in-SubViewport — that path
	# darkens on the Compatibility/WebGL renderer (color-space mismatch).
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

	# Terminal: the shop — a Rive card grid on a screen. Click cards to
	# buy blocks (each spawns one at the pedestal).
	term_vp = SubViewport.new()
	term_vp.size = Vector2i(512, 512)
	term_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(term_vp)
	var term_bg := ColorRect.new()
	term_bg.color = Color(0.04, 0.06, 0.08)
	term_bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	term_vp.add_child(term_bg)
	term_vp.add_child(GameUI.label("RIVETON SUPPLY CO.", 22,
			Vector2(24, 18), Color(0.5, 0.9, 0.7)))
	term_vp.add_child(GameUI.label("click a crate to purchase", 14,
			Vector2(24, 52), Color(1, 1, 1, 0.5)))
	for i in 4:
		var item := RiveControl.new()
		item.file = load("res://fixtures/cards.riv")
		item.artboard = "shop_item"
		item.position = Vector2(10 + (i % 2) * 250, 92 + (i / 2) * 208)
		item.size = Vector2(242, 198)
		term_vp.add_child(item)
		item.loaded.connect(func():
			item.set_property("label", "CRATE %d" % (i + 1))
			item.set_property("price", "%d G" % (25 * (i + 1)))
			item.set_property("tint", Color.from_hsv(0.08 + i * 0.18, 0.75, 0.9))
			item.set_property("sold", 1.0), CONNECT_ONE_SHOT)
		shop_items.push_back(item)
	var term_mat := StandardMaterial3D.new()
	term_mat.albedo_texture = term_vp.get_texture()
	term_mat.emission_enabled = true
	term_mat.emission_texture = term_vp.get_texture()
	term_mat.emission_energy_multiplier = 1.4
	_box(Vector3(0, 1.7, -5.88), Vector3(2.6, 2.6, 0.08), wall_mat) # bezel
	terminal_mesh = _screen(Vector3(0, 1.7, -5.82), 2.4, 2.4, term_mat)

	# Light switch screen: the light_switch artboard's own Click listener
	# toggles its lamp; we watch the state and drive the room light.
	# (SubViewport-hosted RiveControl — works on every renderer, including
	# web; RiveTexture itself needs the RD renderers.)
	switch_vp = SubViewport.new()
	switch_vp.size = Vector2i(256, 256)
	switch_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(switch_vp)
	switch_ui = RiveControl.new()
	switch_ui.file = load("res://fixtures/light_switch.riv")
	switch_ui.set_anchors_preset(Control.PRESET_FULL_RECT)
	switch_vp.add_child(switch_ui)
	var sw_mat := StandardMaterial3D.new()
	sw_mat.albedo_texture = switch_vp.get_texture()
	sw_mat.emission_enabled = true
	sw_mat.emission_texture = switch_vp.get_texture()
	_box(Vector3(3.5, 1.5, -5.88), Vector3(1.3, 1.3, 0.08), wall_mat) # bezel
	switch_mesh = _screen(Vector3(3.5, 1.5, -5.82), 1.1, 1.1, sw_mat)
	# The artboard's click listener toggles its state machine; mirror the
	# lamp state onto the room light.
	switch_ui.state_changed.connect(func(state):
		lamp_on = not lamp_on
		room_light.light_energy = 2.4 if lamp_on else 0.5
		room_light.light_color = Color(1.0, 0.95, 0.8) if lamp_on \
				else Color(0.4, 0.45, 0.7))

	# Pedestal where purchases appear.
	_box(Vector3(-3, 0.4, -3), Vector3(1, 0.8, 1), wall_mat)

	# Universal 2D shop overlay (the projected wall screen is a bonus on
	# renderers that support viewport-on-material; see task #32 for web).
	shop_overlay = Control.new()
	shop_overlay.visible = false
	shop_overlay.z_index = 20
	add_child(shop_overlay)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.6)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_IGNORE
	shop_overlay.add_child(dim)
	var shop_bg := GameUI.panel(Vector2(240, 90), Vector2(620, 480), Color(0.4, 0.85, 0.6))
	shop_overlay.add_child(shop_bg)
	shop_overlay.add_child(GameUI.label("RIVETON SUPPLY CO.", 26, Vector2(270, 116),
			Color(0.5, 0.9, 0.7), true))
	shop_overlay.add_child(GameUI.label("click a crate to buy   E to close", 14,
			Vector2(272, 152), Color(1, 1, 1, 0.5)))
	for i in 4:
		var item := RiveControl.new()
		item.file = load("res://fixtures/cards.riv")
		item.artboard = "shop_item"
		item.position = Vector2(285 + (i % 2) * 280, 185 + (i / 2) * 190)
		item.size = Vector2(250, 175)
		shop_overlay.add_child(item)
		item.loaded.connect(func():
			item.set_property("label", "CRATE %d" % (i + 1))
			item.set_property("price", "%d G" % (25 * (i + 1)))
			item.set_property("tint", Color.from_hsv(0.08 + i * 0.18, 0.75, 0.9))
			item.set_property("sold", 1.0), CONNECT_ONE_SHOT)
		item.gui_input.connect(func(ev):
			if ev is InputEventMouseButton and ev.pressed:
				_buy(i))
		overlay_items.push_back(item)

	var cross := GameUI.label("+", 22, Vector2.ZERO, Color(1, 1, 1, 0.8))
	cross.set_anchors_preset(Control.PRESET_CENTER)
	add_child(cross)
	info = GameUI.label("CLICK captures mouse   WASD move   E near terminal: shop   click switch: lights   ESC menu",
			13, Vector2(170, 12), Color(1, 1, 1, 0.55))
	add_child(info)


func _panel_texture(base: Color, line: Color) -> ImageTexture:
	# Simple sci-fi panel: base fill, darker seams, corner rivets.
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


func _screen(pos: Vector3, w: float, h: float, mat: Material) -> MeshInstance3D:
	# Full-texture quad (BoxMesh atlases its six faces across the UV space).
	var m := MeshInstance3D.new()
	var quad := QuadMesh.new()
	quad.size = Vector2(w, h)
	quad.material = mat
	m.mesh = quad
	m.position = pos
	world.add_child(m)
	return m


func _box(pos: Vector3, box_size: Vector3, mat: Material) -> MeshInstance3D:
	var m := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = box_size
	mesh.material = mat
	m.mesh = mesh
	m.position = pos
	world.add_child(m)
	return m


func _toggle_shop() -> void:
	shop_overlay.visible = not shop_overlay.visible
	if shop_overlay.visible:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	else:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _buy(index: int) -> void:
	blocks += 1
	var mat := StandardMaterial3D.new()
	mat.albedo_texture = _panel_texture(Color(0.85, 0.5, 0.15), Color(0.6, 0.32, 0.08))
	_box(Vector3(-3, 1.0 + blocks * 0.45, -3), Vector3(0.4, 0.4, 0.4), mat)
	shop_items[index].set_property("sold", 0.3)
	shop_items[index].set_property("price", "SOLD")
	overlay_items[index].set_property("sold", 0.3)
	overlay_items[index].set_property("price", "SOLD")


func _screen_uv(mesh: MeshInstance3D, half: Vector2) -> Variant:
	# Ray from camera center to the screen plane (walls face +Z).
	var origin := camera.global_position
	var dir := -camera.global_transform.basis.z
	var plane_z: float = mesh.position.z + 0.01
	if absf(dir.z) < 0.0001:
		return null
	var t := (plane_z - origin.z) / dir.z
	if t <= 0 or t > 8.0:
		return null
	var hit := origin + dir * t
	var local := Vector2(hit.x - mesh.position.x, hit.y - mesh.position.y)
	if absf(local.x) > half.x or absf(local.y) > half.y:
		return null
	return Vector2(local.x / (half.x * 2) + 0.5, 0.5 - local.y / (half.y * 2))


func _interact(phase: int) -> void:
	var uv = _screen_uv(terminal_mesh, Vector2(1.2, 1.2))
	if uv != null:
		var p: Vector2 = uv * 512.0
		if phase == 1 and p.y > 92:
			var idx := int(p.x / 256.0) + 2 * int((p.y - 92.0) / 208.0)
			if idx >= 0 and idx < 4:
				_buy(idx)
		return
	uv = _screen_uv(switch_mesh, Vector2(0.55, 0.55))
	if uv != null:
		switch_ui.send_pointer_event(phase, uv * 256.0)


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_E:
		# Shop opens when near/facing the terminal (or from anywhere on a
		# renderer without projected screens — just be within reach).
		if shop_overlay.visible or body.distance_to(Vector3(0, 1.6, -4)) < 3.5:
			_toggle_shop()
		return
	if shop_overlay.visible:
		return
	if event is InputEventMouseButton and event.pressed:
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
			return
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
	# Hover feedback on the screens.
	_interact(0)

# Riveton — open meadow, smooth movement. Rive drives the UI: the status
# panel (hud_panel artboard), the dialogue box (dialogue artboard with
# bound speaker/line text — the typewriter types into the Rive property),
# and the party screen (a Rive list). WASD/arrows move, E talks, P party.
extends Control

const TILE := 48
const MAP_W := 22
const MAP_H := 14
const SPEED := 230.0

const NPCS := [
	{"name": "Mira", "pos": Vector2(9.0, 4.0), "tint": Color(0.85, 0.4, 0.5),
		"lines": ["Welcome to Riveton, traveler!", "The old terminal in the tower\nstill hums at night..."]},
	{"name": "Bosk", "pos": Vector2(5.0, 9.0), "tint": Color(0.4, 0.6, 0.85),
		"lines": ["I saw the tavern cards stack\nthemselves this morning.", "Data binding, the elders call it."]},
	{"name": "Twig", "pos": Vector2(15.0, 9.0), "tint": Color(0.5, 0.8, 0.4),
		"lines": ["Press P to inspect our party!", "We all live in one .riv file,\nyou know."]},
]
const PARTY := [
	["HERO", 0.9, Color(0.3, 0.7, 0.9)], ["MIRA", 0.65, Color(0.85, 0.4, 0.5)],
	["BOSK", 0.4, Color(0.4, 0.6, 0.85)], ["TWIG", 1.0, Color(0.5, 0.8, 0.4)],
]

var world: Node2D
var player: Sprite2D
var player_pos := Vector2(11, 7) * TILE   # pixels, feet point
var facing := 0 # 0 down, 1 up, 2 left, 3 right
var walk_frame := 0
var walk_clock := 0.0
var sheet: Texture2D
var npc_sheet: Texture2D
var obstacles: Array = []                 # [center: Vector2, radius: float]
var hud: RiveControl
var dialogue_box: RiveControl
var portrait_rect: TextureRect
var dlg_panel: Control
var party_panel: Control
var talking := -1
var line := 0
var typed := 0.0
var gold := 0
var hp := 0.9
var help_label: Label


func _sub(tex: Texture2D, region: Rect2) -> ImageTexture:
	var img := tex.get_image()
	var out := Image.create_empty(int(region.size.x), int(region.size.y), false, Image.FORMAT_RGBA8)
	out.blit_rect(img, region, Vector2i.ZERO)
	return ImageTexture.create_from_image(out)


func _tile(tex: Texture2D, tx: int, ty: int, cell: Vector2, w := 1, h := 1,
		z := 0) -> Sprite2D:
	var s := Sprite2D.new()
	s.texture = _sub(tex, Rect2(tx * 16, ty * 16, w * 16, h * 16))
	s.scale = Vector2(3, 3)
	s.centered = false
	s.position = cell * TILE
	s.z_index = z
	world.add_child(s)
	return s


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	world = Node2D.new()
	world.z_index = -20
	add_child(world)
	var overworld: Texture2D = load("res://games/assets/overworld.png")
	sheet = load("res://games/assets/character.png")
	npc_sheet = load("res://games/assets/npc.png")
	seed(7)

	var grass_tiles := [Vector2(4, 9), Vector2(5, 9), Vector2(6, 9), Vector2(5, 10), Vector2(7, 10)]
	for x in MAP_W:
		for y in MAP_H:
			var g: Vector2 = grass_tiles[randi() % grass_tiles.size()]
			_tile(overworld, int(g.x), int(g.y), Vector2(x, y))
	for i in 22:
		var cell := Vector2(1 + randi() % (MAP_W - 2), 1 + randi() % (MAP_H - 2))
		_tile(overworld, 3 if i % 2 == 0 else 0, 11 if i % 2 == 0 else 8, cell)
	for x in MAP_W:
		_tile(overworld, 1, 13, Vector2(x, 0), 1, 1, 4)
		_tile(overworld, 1, 13, Vector2(x, MAP_H - 1), 1, 1, 4)
	for y in MAP_H:
		_tile(overworld, 1, 13, Vector2(0, y), 1, 1, 4)
		_tile(overworld, 1, 13, Vector2(MAP_W - 1, y), 1, 1, 4)
	for bush in [Vector2(5, 3), Vector2(16, 4), Vector2(4, 10), Vector2(18, 9)]:
		_tile(overworld, 3, 14, bush, 1, 1, 8)
		obstacles.push_back([bush * TILE + Vector2(24, 30), 26.0])

	for i in NPCS.size():
		var npc := Sprite2D.new()
		npc.texture = _sub(npc_sheet, Rect2(i * 16, 0, 16, 32))
		npc.scale = Vector2(3, 3)
		npc.centered = false
		npc.modulate = NPCS[i]["tint"].lightened(0.55)
		npc.position = NPCS[i]["pos"] * TILE - Vector2(0, 48)
		npc.z_index = 10
		world.add_child(npc)
		obstacles.push_back([NPCS[i]["pos"] * TILE + Vector2(24, 24), 30.0])

	player = Sprite2D.new()
	player.scale = Vector2(3, 3)
	player.centered = false
	player.z_index = 10
	world.add_child(player)
	_update_player_sprite()

	# Status panel: the hud_panel Rive artboard, all data binding.
	hud = RiveControl.new()
	hud.file = load("res://fixtures/cards.riv")
	hud.artboard = "hud_panel"
	hud.position = Vector2(14, 12)
	hud.size = Vector2(240, 100)
	hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(hud)
	hud.loaded.connect(_hud_refresh, CONNECT_ONE_SHOT)

	help_label = GameUI.label("WASD move   E talk   P party   ESC menu", 13,
			Vector2(360, 644), Color(1, 1, 1, 0.55))
	add_child(help_label)

	# Dialogue: the dialogue Rive artboard; Godot overlays only the
	# pixel-art portrait into the artboard's portrait slot.
	dlg_panel = Control.new()
	dlg_panel.visible = false
	add_child(dlg_panel)
	dialogue_box = RiveControl.new()
	dialogue_box.file = load("res://fixtures/cards.riv")
	dialogue_box.artboard = "dialogue"
	dialogue_box.position = Vector2(140, 458)
	dialogue_box.size = Vector2(820, 190)
	dialogue_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	dlg_panel.add_child(dialogue_box)
	portrait_rect = TextureRect.new()
	portrait_rect.position = dialogue_box.position + Vector2(24, 24)
	portrait_rect.size = Vector2(134, 134)
	portrait_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	portrait_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	portrait_rect.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	dlg_panel.add_child(portrait_rect)
	dlg_panel.add_child(GameUI.label("E ▸", 14, dialogue_box.position + Vector2(770, 156),
			Color(1, 1, 1, 0.5)))

	# Party screen: unchanged Rive list.
	party_panel = Control.new()
	party_panel.visible = false
	add_child(party_panel)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.55)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_IGNORE
	party_panel.add_child(dim)
	var pp := GameUI.panel(Vector2(130, 90), Vector2(840, 500), Color(0.55, 0.65, 0.95))
	party_panel.add_child(pp)
	party_panel.add_child(GameUI.label("PARTY", 26, Vector2(165, 112), Color.WHITE, true))
	party_panel.add_child(GameUI.label("INVENTORY", 26, Vector2(560, 112), Color.WHITE, true))
	party_panel.add_child(GameUI.label("drag to scroll", 13, Vector2(560, 146), Color(1, 1, 1, 0.45)))
	for i in PARTY.size():
		var member := RiveControl.new()
		member.file = load("res://fixtures/cards.riv")
		member.artboard = "hud_panel"
		member.position = Vector2(160, 165 + i * 100)
		member.size = Vector2(300, 90)
		member.mouse_filter = Control.MOUSE_FILTER_IGNORE
		party_panel.add_child(member)
		member.loaded.connect(func():
			member.set_property("title", PARTY[i][0])
			member.set_property("value", PARTY[i][1])
			member.set_property("tint", PARTY[i][2])
			# Plain Image -> static decode path (adopted GPU textures
			# currently mishandle transparency; see task #33).
			member.set_property("face", _face_image(i)), CONNECT_ONE_SHOT)
	var inv := RiveControl.new()
	inv.file = load("res://fixtures/cards.riv")
	inv.artboard = "inventory"
	inv.position = Vector2(550, 165)
	inv.size = Vector2(390, 400)
	party_panel.add_child(inv)
	inv.loaded.connect(func():
		var objects: Texture2D = load("res://games/assets/objects.png")
		# [name, qty, tint, icon cell in objects.png (16px grid)]
		var items := [["Potion", "x3", Color(0.4, 0.8, 0.5), Vector2(13, 0)],
				["Hi-Potion", "x1", Color(0.3, 0.7, 0.9), Vector2(14, 0)],
				["Antidote", "x2", Color(0.7, 0.5, 0.9), Vector2(3, 1)],
				["Bronze Key", "x1", Color(0.85, 0.7, 0.35), Vector2(0, 4)],
				["Herb", "x7", Color(0.5, 0.8, 0.4), Vector2(2, 0)],
				["Ether", "x2", Color(0.55, 0.55, 0.95), Vector2(9, 3)],
				["Tent", "x1", Color(0.85, 0.55, 0.4), Vector2(9, 0)],
				["Riveton Pass", "x1", Color(0.5, 0.9, 0.7), Vector2(16, 0)],
				["Old Coin", "x12", Color(0.8, 0.75, 0.5), Vector2(0, 5)],
				["Map Fragment", "x4", Color(0.6, 0.6, 0.65), Vector2(18, 0)]]
		# Fill the pack out so the list genuinely scrolls.
		var adjectives := ["Iron", "Silver", "Oak", "Crystal", "Moss", "Sun",
				"River", "Old", "Fine", "Cursed"]
		var kinds := ["Sword", "Shield", "Ring", "Amulet", "Scroll",
				"Tonic", "Loaf", "Gem", "Charm", "Bolt"]
		var icon_cells := [Vector2(13, 0), Vector2(14, 0), Vector2(2, 0),
				Vector2(9, 0), Vector2(16, 0), Vector2(17, 0), Vector2(18, 0),
				Vector2(0, 4), Vector2(0, 5), Vector2(3, 5), Vector2(4, 3),
				Vector2(4, 0), Vector2(20, 0), Vector2(2, 5)]
		for a in adjectives.size():
			for b in range(0, 5):
				items.push_back(["%s %s" % [adjectives[a], kinds[(a + b) % kinds.size()]],
						"x%d" % ((a * 7 + b * 3) % 9 + 1),
						Color.from_hsv(fmod(0.07 * (a * 5 + b), 1.0), 0.6, 0.85),
						icon_cells[(a * 3 + b) % icon_cells.size()]])
		for k in items.size():
			inv.list_append("rows", "InvRowVM")
			inv.list_set_property("rows", k, "name", items[k][0])
			inv.list_set_property("rows", k, "qty", items[k][1])
			inv.list_set_property("rows", k, "tint", items[k][2])
			# Godot texture -> rive image property, per list item.
			var icon := _sub(objects, Rect2(items[k][3] * 16.0, Vector2(16, 16))).get_image()
			icon.resize(32, 32, Image.INTERPOLATE_NEAREST)
			inv.list_set_property("rows", k, "icon",
					ImageTexture.create_from_image(icon)), CONNECT_ONE_SHOT)


func _face_image(member_index: int) -> Image:
	# The WHOLE character sprite (16x32), doubled to 32x64 and centered
	# in a transparent 64x64 square — rive Image elements render bound
	# images at natural size, so a fixed square keeps every member
	# aligned identically in the slot.
	var body: Image
	if member_index == 0:
		body = _sub(sheet, Rect2(0, 0, 16, 32)).get_image()
	else:
		body = _sub(npc_sheet, Rect2((member_index - 1) * 16, 0, 16, 32)).get_image()
		var tint: Color = PARTY[member_index][2].lightened(0.55)
		for px in body.get_width():
			for py in body.get_height():
				var p := body.get_pixel(px, py)
				body.set_pixel(px, py, Color(p.r * tint.r, p.g * tint.g, p.b * tint.b, p.a))
	body.resize(32, 64, Image.INTERPOLATE_NEAREST)
	var square := Image.create_empty(64, 64, false, Image.FORMAT_RGBA8)
	square.blit_rect(body, Rect2i(0, 0, 32, 64), Vector2i(16, 0))
	return square


func _hud_refresh() -> void:
	hud.set_property("title", "%d G" % gold)
	hud.set_property("value", hp)
	hud.set_property("tint", Color(0.34, 0.75, 0.54) if hp > 0.5 else Color(0.9, 0.4, 0.2))


func _update_player_sprite() -> void:
	var row := 0
	match facing:
		1: row = 2
		2, 3: row = 3
	player.flip_h = facing == 3
	player.texture = _sub(sheet, Rect2(walk_frame * 16, row * 32, 16, 32))
	player.position = player_pos + Vector2(-24, -84)


func _npc_near() -> int:
	for i in NPCS.size():
		var center: Vector2 = NPCS[i]["pos"] * TILE + Vector2(24, 24)
		if center.distance_to(player_pos) <= 78.0:
			return i
	return -1


func _process(delta: float) -> void:
	# Typewriter into the Rive-bound line property.
	if talking >= 0:
		var full: String = NPCS[talking]["lines"][line]
		var prev := int(typed)
		typed = min(typed + delta * 40.0, full.length())
		if int(typed) != prev:
			dialogue_box.set_property("line", full.substr(0, int(typed)))
		return

	# Smooth movement.
	var wish := Vector2.ZERO
	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP): wish.y -= 1
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN): wish.y += 1
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT): wish.x -= 1
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT): wish.x += 1
	if wish != Vector2.ZERO:
		wish = wish.normalized()
		var next := player_pos + wish * SPEED * delta
		next.x = clamp(next.x, TILE + 14.0, (MAP_W - 1) * TILE - 14.0)
		next.y = clamp(next.y, TILE + 30.0, (MAP_H - 1) * TILE - 6.0)
		for ob in obstacles:
			var away: Vector2 = next - ob[0]
			if away.length() < ob[1]:
				next = ob[0] + away.normalized() * ob[1]
		player_pos = next
		facing = 3 if wish.x > 0.3 else 2 if wish.x < -0.3 \
				else 1 if wish.y < 0 else 0
		walk_clock += delta
		if walk_clock > 0.14:
			walk_clock = 0.0
			walk_frame = (walk_frame + 1) % 4
	else:
		walk_frame = 0
	_update_player_sprite()


func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed):
		return
	if talking >= 0:
		if event.keycode == KEY_E:
			var full: String = NPCS[talking]["lines"][line]
			if typed < full.length():
				typed = full.length()
				dialogue_box.set_property("line", full)
				return
			line += 1
			typed = 0.0
			if line >= NPCS[talking]["lines"].size():
				talking = -1
				dialogue_box.set_property("shown", 0.0)
				dlg_panel.visible = false
				help_label.visible = true
				gold += 5
				_hud_refresh()
			else:
				dialogue_box.set_property("line", "")
		return
	match event.keycode:
		KEY_P: party_panel.visible = not party_panel.visible
		KEY_E:
			var i := _npc_near()
			if i >= 0:
				talking = i
				line = 0
				typed = 0.0
				dlg_panel.visible = true
				help_label.visible = false
				var fade := create_tween()
				fade.tween_method(func(v): dialogue_box.set_property("shown", v),
						0.0, 1.0, 0.18)
				dialogue_box.set_property("speaker", NPCS[i]["name"])
				dialogue_box.set_property("line", "")
				# Portrait: the villager's full sprite, tinted to match.
				portrait_rect.texture = ImageTexture.create_from_image(_face_image(i + 1))

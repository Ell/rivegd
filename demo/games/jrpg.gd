# Riveton — a small town scene. Rive drives the UI surfaces: the status
# card (data binding), the dialogue box portrait (live image property),
# and the party screen (a Rive list, one card per member).
# WASD/arrows move, E talks, P toggles the party screen.
extends Control

const TILE := 48
const MAP_W := 22
const MAP_H := 14

const NPCS := [
	{"name": "Mira", "pos": Vector2(9, 4), "tint": Color(0.85, 0.4, 0.5), "face": 0,
		"lines": ["Welcome to Riveton, traveler!", "The old terminal in the tower\nstill hums at night..."]},
	{"name": "Bosk", "pos": Vector2(5, 9), "tint": Color(0.4, 0.6, 0.85), "face": 1,
		"lines": ["I saw the tavern cards stack\nthemselves this morning.", "Data binding, the elders call it."]},
	{"name": "Twig", "pos": Vector2(15, 9), "tint": Color(0.5, 0.8, 0.4), "face": 2,
		"lines": ["Press P to inspect our party!", "We all live in one .riv file,\nyou know."]},
]
const PARTY := [
	["HERO", 0.9, Color(0.3, 0.7, 0.9)], ["MIRA", 0.65, Color(0.85, 0.4, 0.5)],
	["BOSK", 0.4, Color(0.4, 0.6, 0.85)], ["TWIG", 1.0, Color(0.5, 0.8, 0.4)],
]

var world: Node2D
var player: Sprite2D
var player_cell := Vector2(11, 7)
var facing := 0 # 0 down, 1 up, 2 left, 3 right
var walk_frame := 0
var sheet: Texture2D
var npc_sheet: Texture2D
var blocked := {}
var hud: RiveControl
var portrait_rect: TextureRect
var dlg_panel: Control
var dlg_text: Label
var dlg_name: Label
var party_panel: Control
var talking := -1
var line := 0
var typed := 0.0
var gold := 0
var hp := 0.9


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


func _block(cell: Vector2, w := 1, h := 1) -> void:
	for dx in w:
		for dy in h:
			blocked[cell + Vector2(dx, dy)] = true


func _ready() -> void:
	world = Node2D.new()
	world.z_index = -20 # sprites use z_index 0..10; keep ALL below the UI
	add_child(world)
	var overworld: Texture2D = load("res://games/assets/overworld.png")
	sheet = load("res://games/assets/character.png")
	npc_sheet = load("res://games/assets/npc.png")
	seed(7)

	# Ground: grass variants everywhere, sandy plaza in the middle.
	var grass_tiles := [Vector2(4, 9), Vector2(5, 9), Vector2(6, 9), Vector2(5, 10), Vector2(7, 10)]
	for x in MAP_W:
		for y in MAP_H:
			var g: Vector2 = grass_tiles[randi() % grass_tiles.size()]
			_tile(overworld, int(g.x), int(g.y), Vector2(x, y))
	# Fountain centerpiece.
	_tile(overworld, 22, 9, Vector2(10.4, 7.2), 2, 2, 5)
	_block(Vector2(10, 7), 2, 2)

	# Flower + sprout decoration.
	for i in 26:
		var cell := Vector2(randi() % MAP_W, randi() % MAP_H)
		if blocked.has(cell) or (cell.x >= 8 and cell.x < 14 and cell.y >= 6 and cell.y < 10):
			continue
		_tile(overworld, 3 if i % 2 == 0 else 0, 11 if i % 2 == 0 else 8, cell)

	# Hedge border (keeps the player in-world).
	for x in MAP_W:
		_tile(overworld, 1, 13, Vector2(x, 0), 1, 1, 4)
		_tile(overworld, 1, 13, Vector2(x, MAP_H - 1), 1, 1, 4)
		_block(Vector2(x, 0)); _block(Vector2(x, MAP_H - 1))
	for y in MAP_H:
		_tile(overworld, 1, 13, Vector2(0, y), 1, 1, 4)
		_tile(overworld, 1, 13, Vector2(MAP_W - 1, y), 1, 1, 4)
		_block(Vector2(0, y)); _block(Vector2(MAP_W - 1, y))

	# Houses + trees + pond.
	for house in [Vector2(2, 1.6), Vector2(12.5, 1.4), Vector2(17, 8.4)]:
		_tile(overworld, 6, 0, house, 5, 5, 8)
		_block(Vector2(house.x, house.y + 1.4).floor(), 5, 3)
	for bush in [Vector2(6, 2), Vector2(16, 3.4), Vector2(3, 10.6), Vector2(19, 5), Vector2(12, 11.5)]:
		_tile(overworld, 3, 14, bush, 1, 1, 8)
		_block(bush.floor())
	_tile(overworld, 2, 6, Vector2(6.4, 10.8), 3, 3, 2)
	_block(Vector2(6, 10), 3, 3)

	for i in NPCS.size():
		var npc := Sprite2D.new()
		npc.texture = _sub(npc_sheet, Rect2(i * 16, 0, 16, 32))
		npc.scale = Vector2(3, 3)
		npc.centered = false
		npc.modulate = NPCS[i]["tint"].lightened(0.55)
		npc.position = NPCS[i]["pos"] * TILE + Vector2(0, TILE) - Vector2(0, 84)
		npc.z_index = 10
		world.add_child(npc)
		_block(NPCS[i]["pos"])

	player = Sprite2D.new()
	player.scale = Vector2(3, 3)
	player.centered = false
	player.z_index = 10
	world.add_child(player)
	_update_player_sprite()

	# --- Rive UI layer ---
	var status := GameUI.panel(Vector2(14, 12), Vector2(196, 78))
	add_child(status)
	hud = RiveControl.new()
	hud.file = load("res://fixtures/cards.riv")
	hud.artboard = "card"
	hud.position = Vector2(24, 20)
	hud.size = Vector2(44, 62)
	hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(hud)
	hud.loaded.connect(_hud_refresh, CONNECT_ONE_SHOT)
	add_child(GameUI.label("HERO", 16, Vector2(82, 24), Color(1, 0.9, 0.6)))
	gold_label = GameUI.label("0 G", 15, Vector2(82, 50), Color(0.95, 0.85, 0.4))
	add_child(gold_label)

	help_label = GameUI.label("WASD move   E talk   P party   ESC menu", 13,
			Vector2(360, 644), Color(1, 1, 1, 0.55))
	add_child(help_label)

	# Dialogue panel (hidden until talking).
	dlg_panel = Control.new()
	dlg_panel.visible = false
	add_child(dlg_panel)
	var dlg_box := GameUI.panel(Vector2(150, 470), Vector2(800, 172), Color(0.85, 0.7, 0.35))
	dlg_panel.add_child(dlg_box)
	var frame := GameUI.panel(Vector2(170, 442), Vector2(150, 150), Color(0.85, 0.7, 0.35), Color(0.12, 0.1, 0.09))
	dlg_panel.add_child(frame)
	portrait_rect = TextureRect.new()
	portrait_rect.position = Vector2(182, 454)
	portrait_rect.size = Vector2(126, 126)
	portrait_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	portrait_rect.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	portrait_rect.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	dlg_panel.add_child(portrait_rect)
	dlg_name = GameUI.label("", 18, Vector2(344, 486), Color(1, 0.85, 0.4))
	dlg_panel.add_child(dlg_name)
	dlg_text = GameUI.label("", 20, Vector2(344, 520))
	dlg_panel.add_child(dlg_text)
	dlg_panel.add_child(GameUI.label("E  ▸", 14, Vector2(880, 606), Color(1, 1, 1, 0.5)))

	# Party screen.
	party_panel = Control.new()
	party_panel.visible = false
	add_child(party_panel)
	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.55)
	dim.set_anchors_preset(Control.PRESET_FULL_RECT)
	party_panel.add_child(dim)
	var pp := GameUI.panel(Vector2(190, 80), Vector2(720, 500), Color(0.55, 0.65, 0.95))
	party_panel.add_child(pp)
	party_panel.add_child(GameUI.label("PARTY", 30, Vector2(220, 102), Color.WHITE, true))
	party_panel.add_child(GameUI.label("bar = HP", 14, Vector2(800, 116), Color(1, 1, 1, 0.5)))
	var party_grid := RiveControl.new()
	party_grid.file = load("res://fixtures/cards.riv")
	party_grid.artboard = "cards"
	party_grid.position = Vector2(215, 150)
	party_grid.size = Vector2(670, 400)
	party_panel.add_child(party_grid)
	party_grid.loaded.connect(func():
		for i in PARTY.size():
			party_grid.list_append("items", "CardVM")
			party_grid.list_set_property("items", i, "value", PARTY[i][1])
			party_grid.list_set_property("items", i, "tint", PARTY[i][2]), CONNECT_ONE_SHOT)
	for i in PARTY.size():
		party_panel.add_child(GameUI.label(PARTY[i][0], 17,
				Vector2(248 + (i % 3) * 172, 566 if i >= 3 else 336)))


var gold_label: Label
var help_label: Label


func _hud_refresh() -> void:
	hud.set_property("value", hp)
	hud.set_property("tint", Color(0.3, 0.7, 0.4) if hp > 0.5 else Color(0.9, 0.4, 0.2))
	if gold_label != null:
		gold_label.text = "%d G" % gold


func _update_player_sprite() -> void:
	# character.png: 16x32 cells, 4 walk frames per row; rows: 0 down,
	# 2 up, 3 side (flip_h for left).
	var row := 0
	match facing:
		1: row = 2
		2, 3: row = 3
	player.flip_h = facing == 2
	player.texture = _sub(sheet, Rect2(walk_frame * 16, row * 32, 16, 32))
	player.position = player_cell * TILE + Vector2(0, TILE) - Vector2(0, 84)


func _move_player(dir: Vector2, face: int) -> void:
	facing = face
	walk_frame = (walk_frame + 1) % 4
	var next := player_cell + dir
	if not blocked.has(next) and next.x >= 0 and next.y >= 0 \
			and next.x < MAP_W and next.y < MAP_H:
		player_cell = next
	_update_player_sprite()


func _npc_near() -> int:
	for i in NPCS.size():
		if NPCS[i]["pos"].distance_to(player_cell) <= 1.5:
			return i
	return -1


func _process(delta: float) -> void:
	if talking >= 0 and dlg_text != null:
		var full: String = NPCS[talking]["lines"][line]
		typed = min(typed + delta * 40.0, full.length())
		dlg_text.text = full.substr(0, int(typed))


func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey and event.pressed):
		return
	if talking >= 0:
		if event.keycode == KEY_E:
			var full: String = NPCS[talking]["lines"][line]
			if typed < full.length():
				typed = full.length()
				return
			line += 1
			typed = 0.0
			if line >= NPCS[talking]["lines"].size():
				talking = -1
				dlg_panel.visible = false
				help_label.visible = true
				gold += 5
				_hud_refresh()
		return
	match event.keycode:
		KEY_W, KEY_UP: _move_player(Vector2(0, -1), 1)
		KEY_S, KEY_DOWN: _move_player(Vector2(0, 1), 0)
		KEY_A, KEY_LEFT: _move_player(Vector2(-1, 0), 2)
		KEY_D, KEY_RIGHT: _move_player(Vector2(1, 0), 3)
		KEY_P: party_panel.visible = not party_panel.visible
		KEY_E:
			var i := _npc_near()
			if i >= 0:
				talking = i
				line = 0
				typed = 0.0
				dlg_panel.visible = true
				help_label.visible = false
				dlg_name.text = NPCS[i]["name"]
				var face := _sub(npc_sheet, Rect2(i * 16, 2, 16, 14)).get_image()
				var tint: Color = NPCS[i]["tint"].lightened(0.55)
				for px in face.get_width():
					for py in face.get_height():
						var p := face.get_pixel(px, py)
						face.set_pixel(px, py, Color(p.r * tint.r, p.g * tint.g, p.b * tint.b, p.a))
				portrait_rect.texture = ImageTexture.create_from_image(face)

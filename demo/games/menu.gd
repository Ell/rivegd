# Main menu: each tile is a live Rive card (hover pulses the value bar;
# the card's own Click listener flashes it on launch).
extends Control

const TILES := [
	["BREAKOUT", "breakout", Color(0.95, 0.55, 0.15), "bricks, paddle, rive HUD"],
	["RIVETON", "jrpg", Color(0.25, 0.75, 0.45), "a town, its people, their UI"],
	["TERMINAL", "fps", Color(0.3, 0.55, 0.95), "first person, rive on walls"],
]


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var bg := ColorRect.new()
	bg.color = GameUI.BG
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(bg)
	GameUI.vignette(self)

	add_child(GameUI.label("RIVEGD", 52, Vector2(72, 52), Color.WHITE, true))
	add_child(GameUI.label("GAME COLLECTION", 18, Vector2(76, 118), Color(1, 1, 1, 0.5)))
	add_child(GameUI.label("Rive UI running inside Godot — pick a cartridge",
			15, Vector2(76, 152), Color(0.65, 0.75, 1.0, 0.8)))

	for i in TILES.size():
		var frame := GameUI.panel(Vector2(66 + i * 340, 208), Vector2(300, 384),
				TILES[i][2].darkened(0.1))
		add_child(frame)
		var card := RiveControl.new()
		card.file = load("res://fixtures/cards.riv")
		card.artboard = "card"
		card.position = Vector2(80 + i * 340, 222)
		card.size = Vector2(272, 310)
		add_child(card)
		card.loaded.connect(_style.bind(card, i), CONNECT_ONE_SHOT)
		card.gui_input.connect(_clicked.bind(i))
		card.mouse_entered.connect(func():
			card.set_property("value", 1.0)
			frame.scale = Vector2(1.02, 1.02))
		card.mouse_exited.connect(func():
			card.set_property("value", 0.35)
			frame.scale = Vector2.ONE)
		add_child(GameUI.label(TILES[i][0], 24, Vector2(84 + i * 340, 542), Color.WHITE, true))
		add_child(GameUI.label(TILES[i][3], 13, Vector2(84 + i * 340, 572), Color(1, 1, 1, 0.5)))

	add_child(GameUI.label("ESC returns here from any game", 13,
			Vector2(420, 636), Color(1, 1, 1, 0.4)))


func _style(card: RiveControl, i: int) -> void:
	card.set_property("tint", TILES[i][2])
	card.set_property("value", 0.35)


func _clicked(event: InputEvent, i: int) -> void:
	if event is InputEventMouseButton and event.pressed \
			and event.button_index == MOUSE_BUTTON_LEFT:
		get_tree().get_first_node_in_group("game_manager").call_deferred(
				"switch_to", TILES[i][1])

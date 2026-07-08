# rivegd game collection: a main menu and three micro-games, each showing
# a different way to drive Rive from gameplay. Escape returns to the menu.
extends Node

const GAMES := {
	"menu": "res://games/menu.gd",
	"breakout": "res://games/breakout.gd",
	"jrpg": "res://games/jrpg.gd",
	"fps": "res://games/fps.gd",
}

var current: Node


func _ready() -> void:
	switch_to("menu")


func switch_to(game: String) -> void:
	if current != null:
		current.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	current = Node.new() if game == "" else null
	var root := Control.new()
	root.name = game
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.set_script(load(GAMES[game]))
	add_child(root)
	current = root


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		if current == null or current.name != "menu":
			switch_to("menu")

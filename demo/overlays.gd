# Overlays demo: the patterns for "Rive over a game" — health bars that
# follow enemies, and an anchored dialogue box. Built in code so it reads as
# a recipe. Run: godot --path demo overlays.tscn
#
# Pattern 1 (2D world-anchored, e.g. health bar over an enemy): a
#   RiveSprite2D is a CHILD of the enemy node, so it follows automatically
#   with no per-frame tracking. It never captures input (Node2D). Data-bind
#   the artboard's view model to the entity's stats.
# Pattern 2 (screen HUD / dialogue): a RiveControl with
#   mouse_filter = IGNORE — it animates and data-binds but lets gameplay
#   clicks fall through. Use STOP instead for a modal box with buttons.
extends Node2D

const ENEMY_COUNT := 4
const FIXTURE := "res://fixtures/data_binding_test.riv"

var enemies: Array = []
var frames := 0


class Enemy:
	var node: Node2D
	var bar  # RiveSprite2D — the health overlay, a child of `node`
	var health := 1.0
	var velocity: Vector2


func _ready() -> void:
	var file: RiveFileResource = load(FIXTURE)

	for i in ENEMY_COUNT:
		var e := Enemy.new()
		# The "enemy": a plain Godot sprite (here a ColorRect for simplicity).
		e.node = Node2D.new()
		e.node.position = Vector2(180 + i * 200, 300)
		add_child(e.node)
		var body := ColorRect.new()
		body.color = Color(0.3, 0.35, 0.5)
		body.size = Vector2(64, 64)
		body.position = Vector2(-32, -32)
		e.node.add_child(body)

		# The health bar: a RiveSprite2D CHILD → follows the enemy for free.
		# `width` (a data-bound rect) stands in for a health-bar fill; swap
		# in your own bar artboard and property name.
		e.bar = ClassDB.instantiate("RiveSprite2D")
		e.bar.set("file", file)
		e.bar.set("artboard", "artboard-1")
		e.bar.set("size", Vector2i(140, 140))
		e.bar.set("position", Vector2(-70, -150))  # hover above the enemy
		e.node.add_child(e.bar)

		e.health = 1.0 - i * 0.22  # staggered so the bars show a clear range
		e.velocity = Vector2(randf_range(-60, 60), randf_range(-40, 40))
		enemies.append(e)

	# A dialogue box: screen-anchored RiveControl, IGNORE so it never eats
	# gameplay clicks. (Use STOP if it has its own buttons.)
	var dialogue = ClassDB.instantiate("RiveControl")
	dialogue.set("file", file)
	dialogue.set("artboard", "artboard-1")
	dialogue.set("mouse_filter", Control.MOUSE_FILTER_IGNORE)
	dialogue.set("position", Vector2(60, 520))
	dialogue.set("size", Vector2(880, 130))
	add_child(dialogue)


func _process(delta: float) -> void:
	frames += 1
	var bounds := Rect2(40, 120, 960, 320)
	for e in enemies:
		# Move the enemy; the bar follows because it's a child.
		e.node.position += e.velocity * delta
		if not bounds.has_point(e.node.position):
			e.velocity = -e.velocity
			e.node.position = e.node.position.clamp(bounds.position,
					bounds.position + bounds.size)
		# Drain health → drive the bar's data binding (size + colour).
		e.health = e.health - delta * 0.15
		if e.health <= 0.0:
			e.health = 1.0  # respawn health so the demo loops visibly
		# Data binding drives the bar: width tracks health (each enemy's
		# instance is independent — this is the "many enemies" case).
		e.bar.call("set_property", "width", 20.0 + e.health * 180.0)

	var shot := OS.get_environment("RIVEGD_OVERLAY_SHOT")
	if shot != "" and frames == 150:
		get_viewport().get_texture().get_image().save_png(shot)
		print("OVERLAY DEMO SHOT SAVED")
		get_tree().quit(0)

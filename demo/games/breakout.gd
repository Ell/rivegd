# Breakout. Gameplay is plain Godot; the HUD is Rive data binding: the
# score card's bar tracks clear progress, and each life is an item in a
# Rive list — losing one removes it from the layout live.
extends Control

const BRICK_TEX := ["brick_red", "brick_yellow", "brick_green", "brick_blue", "brick_purple"]
const FIELD := Rect2(70, 100, 960, 540)

var paddle: Sprite2D
var ball: Sprite2D
var ball_vel := Vector2.ZERO
var bricks: Array = []
var score := 0
var lives := 3
var total := 0
var hud_score: RiveControl
var hud_lives: RiveControl
var score_label: Label
var playing := false
var msg: Label
var trail: CPUParticles2D


func _ready() -> void:
	var bg := ColorRect.new()
	bg.color = GameUI.BG
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	GameUI.vignette(self)

	# Play field frame.
	var field := GameUI.panel(FIELD.position - Vector2(10, 10),
			FIELD.size + Vector2(20, 20), Color(0.35, 0.4, 0.55),
			Color(0.05, 0.06, 0.09, 0.85))
	add_child(field)

	# Top HUD strip: rive progress card + score + rive lives list.
	var strip := GameUI.panel(Vector2(60, 10), Vector2(980, 78))
	add_child(strip)
	hud_score = RiveControl.new()
	hud_score.file = load("res://fixtures/cards.riv")
	hud_score.artboard = "card"
	hud_score.position = Vector2(76, 18)
	hud_score.size = Vector2(44, 62)
	hud_score.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(hud_score)
	hud_score.loaded.connect(func():
		hud_score.set_property("tint", Color(0.95, 0.6, 0.15))
		hud_score.set_property("value", 0.0), CONNECT_ONE_SHOT)
	add_child(GameUI.label("BREAKOUT", 20, Vector2(136, 22), Color.WHITE, true))
	score_label = GameUI.label("SCORE 0", 16, Vector2(136, 52), Color(0.95, 0.85, 0.4))
	add_child(score_label)
	add_child(GameUI.label("LIVES", 14, Vector2(806, 34), Color(1, 1, 1, 0.6)))
	hud_lives = RiveControl.new()
	hud_lives.file = load("res://fixtures/cards.riv")
	hud_lives.artboard = "cards"
	hud_lives.position = Vector2(870, 14)
	hud_lives.size = Vector2(160, 70)
	hud_lives.fit = 3 # Fit Width — first row of the list, clipped
	hud_lives.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(hud_lives)
	hud_lives.loaded.connect(func():
		for i in lives:
			hud_lives.list_append("items", "CardVM")
			hud_lives.list_set_property("items", i, "tint", Color(0.9, 0.25, 0.3))
			hud_lives.list_set_property("items", i, "value", 1.0), CONNECT_ONE_SHOT)

	trail = CPUParticles2D.new()
	trail.amount = 40
	trail.lifetime = 0.4
	trail.spread = 180.0
	trail.gravity = Vector2.ZERO
	trail.initial_velocity_min = 8.0
	trail.initial_velocity_max = 20.0
	trail.scale_amount_min = 2.0
	trail.scale_amount_max = 4.0
	trail.color = Color(0.5, 0.75, 1.0, 0.5)
	add_child(trail)

	paddle = Sprite2D.new()
	paddle.texture = load("res://games/assets/paddle.png")
	add_child(paddle)
	ball = Sprite2D.new()
	ball.texture = load("res://games/assets/ball.png")
	add_child(ball)

	for row in 5:
		for col in 11:
			var b := Sprite2D.new()
			b.texture = load("res://games/assets/%s.png" % BRICK_TEX[row])
			b.position = Vector2(FIELD.position.x + 80 + col * 82, FIELD.position.y + 60 + row * 42)
			add_child(b)
			bricks.push_back(b)
	total = bricks.size()

	msg = GameUI.label("CLICK TO LAUNCH", 22, Vector2(430, 420), Color(1, 1, 1, 0.85))
	add_child(msg)
	_reset_ball()


func _reset_ball() -> void:
	playing = false
	ball_vel = Vector2.ZERO
	msg.visible = true


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and not playing \
			and lives > 0 and not bricks.is_empty():
		playing = true
		msg.visible = false
		ball_vel = Vector2(randf_range(-0.4, 0.4), -1).normalized() * 620.0


func _process(delta: float) -> void:
	paddle.position = Vector2(
			clamp(get_local_mouse_position().x, FIELD.position.x + 52, FIELD.end.x - 52),
			FIELD.end.y - 26)
	trail.position = ball.position
	trail.emitting = playing
	if not playing:
		ball.position = paddle.position + Vector2(0, -26)
		return
	ball.position += ball_vel * delta
	if ball.position.x < FIELD.position.x + 12 or ball.position.x > FIELD.end.x - 12:
		ball_vel.x = -ball_vel.x
		ball.position.x = clamp(ball.position.x, FIELD.position.x + 12, FIELD.end.x - 12)
	if ball.position.y < FIELD.position.y + 12:
		ball_vel.y = absf(ball_vel.y)
	if ball.position.distance_to(paddle.position) < 55 and ball_vel.y > 0 \
			and absf(ball.position.y - paddle.position.y) < 24:
		ball_vel = Vector2((ball.position.x - paddle.position.x) / 45.0, -1.2).normalized() * 620.0
	for b in bricks:
		if is_instance_valid(b) and absf(ball.position.x - b.position.x) < 40 \
				and absf(ball.position.y - b.position.y) < 24:
			b.queue_free()
			bricks.erase(b)
			ball_vel.y = -ball_vel.y
			score += 10
			score_label.text = "SCORE %d" % score
			hud_score.set_property("value", 1.0 - bricks.size() / float(total))
			if bricks.is_empty():
				msg.text = "YOU WIN  —  ESC FOR MENU"
				_reset_ball()
			break
	if ball.position.y > FIELD.end.y + 20:
		lives -= 1
		if lives >= 0:
			hud_lives.list_remove_at("items", lives)
		if lives <= 0:
			msg.text = "GAME OVER  —  ESC FOR MENU"
			msg.visible = true
			playing = false
			ball.visible = false
		else:
			_reset_ball()

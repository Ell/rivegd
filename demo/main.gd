# rivegd demo: one scene exercising the whole public API surface.
# Everything is built in code so the scene doubles as example snippets.
extends Control

var switch_control: RiveControl
var character: RiveSprite2D
var databind: RiveSprite2D
var event_log: RichTextLabel
var frames := 0


func _ready() -> void:
	var root := HBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.add_theme_constant_override("separation", 16)
	add_child(root)

	# --- Column 1: interactive light switch (pointer listeners + inputs) ---
	var col1 := _column(root, "RiveControl — click the switch!")
	switch_control = RiveControl.new()
	switch_control.file = load("res://fixtures/light_switch.riv")
	switch_control.custom_minimum_size = Vector2(320, 320)
	col1.add_child(switch_control)

	var toggle := CheckButton.new()
	toggle.text = "On (set_bool_input)"
	toggle.button_pressed = true
	toggle.toggled.connect(func(on): switch_control.set_bool_input("On", on))
	col1.add_child(toggle)

	# --- Column 2: state machine character + RiveTexture in a TextureRect ---
	var col2 := _column(root, "RiveSprite2D — state machine")
	var sprite_holder := Control.new()
	sprite_holder.custom_minimum_size = Vector2(320, 220)
	col2.add_child(sprite_holder)
	character = RiveSprite2D.new()
	character.file = load("res://fixtures/bullet_man.riv")
	character.size = Vector2i(320, 220)
	sprite_holder.add_child(character)

	col2.add_child(_label("RiveTexture — plain TextureRect"))
	var texture := RiveTexture.new()
	texture.file = load("res://fixtures/bullet_man.riv")
	texture.render_size = Vector2i(320, 200)
	var rect := TextureRect.new()
	rect.texture = texture
	rect.custom_minimum_size = Vector2(320, 200)
	rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	col2.add_child(rect)

	# --- Column 3: data binding + event log ---
	var col3 := _column(root, "Data binding — width property")
	var holder := Control.new()
	holder.custom_minimum_size = Vector2(320, 220)
	col3.add_child(holder)
	databind = RiveSprite2D.new()
	databind.file = load("res://fixtures/data_binding_test.riv")
	databind.artboard = "artboard-1"
	databind.size = Vector2i(320, 220)
	holder.add_child(databind)
	databind.watch_property("width")

	var slider := HSlider.new()
	slider.min_value = 20
	slider.max_value = 300
	slider.value = 100
	slider.value_changed.connect(func(v): databind.set_property("width", v))
	col3.add_child(slider)

	col3.add_child(_label("Signals"))
	event_log = RichTextLabel.new()
	event_log.custom_minimum_size = Vector2(320, 180)
	event_log.scroll_following = true
	col3.add_child(event_log)

	for source in [switch_control, character, databind]:
		source.rive_event.connect(func(event_name, properties):
			_log("[color=orange]rive_event[/color] %s %s" %
					[event_name, properties]))
		source.state_changed.connect(func(state_name):
			_log("[color=cyan]state_changed[/color] %s" % state_name))
	databind.property_changed.connect(func(path, value):
		_log("[color=lime]property_changed[/color] %s = %s" % [path, value]))


func _column(parent: Control, title: String) -> VBoxContainer:
	var col := VBoxContainer.new()
	col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col.add_theme_constant_override("separation", 8)
	parent.add_child(col)
	col.add_child(_label(title))
	return col


func _label(text: String) -> Label:
	var label := Label.new()
	label.text = text
	return label


func _log(line: String) -> void:
	event_log.append_text(line + "\n")


func _process(_delta: float) -> void:
	# Headless-verification hook: RIVEGD_DEMO_SHOT=<path> screenshots and quits.
	frames += 1
	var shot := OS.get_environment("RIVEGD_DEMO_SHOT")
	if shot != "" and frames == 120:
		get_viewport().get_texture().get_image().save_png(shot)
		print("DEMO SHOT SAVED")
		get_tree().quit(0)

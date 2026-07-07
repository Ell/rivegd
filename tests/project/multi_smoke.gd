# Multi-instance independence (the "many enemies, each with its own health
# bar" case): two RiveSprite2D of the same file+artboard must have fully
# independent view models — setting a data-bound property on one must change
# ONLY its render. Guards against shared-VM regressions.
#   godot --path tests/project multi_smoke.tscn   (needs a display)
extends Node2D

var a: Node
var b: Node
var frames := 0
var a0 := PackedColorArray()
var b0 := PackedColorArray()


func fail(msg: String) -> void:
	push_error("MULTI FAIL: " + msg)
	get_tree().quit(1)


func _make(x: int) -> Node:
	var s = ClassDB.instantiate("RiveSprite2D")
	s.set("file", load("res://fixtures/data_binding_test.riv"))
	s.set("artboard", "artboard-1")
	s.set("size", Vector2i(360, 360))
	s.set("position", Vector2(x, 40))
	add_child(s)
	return s


func _sample(img: Image, x0: int) -> PackedColorArray:
	var out := PackedColorArray()
	for y in range(60, 380, 20):
		for x in range(x0, x0 + 360, 20):
			out.append(img.get_pixel(x, y))
	return out


func _ready() -> void:
	a = _make(30)
	b = _make(430)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60:
		var img := get_viewport().get_texture().get_image()
		a0 = _sample(img, 30)
		b0 = _sample(img, 430)
	elif frames == 66:
		# Change ONLY instance A's data binding.
		a.call("set_property", "width", 300.0)
	elif frames == 130:
		var img := get_viewport().get_texture().get_image()
		var a_changed := _sample(img, 30) != a0
		var b_changed := _sample(img, 430) != b0
		if not a_changed:
			fail("instance A did not re-render after set_property")
			return
		if b_changed:
			fail("instance B changed when only A was set — VMs are shared!")
			return
		print("MULTI OK: instances render independently")
		get_tree().quit(0)

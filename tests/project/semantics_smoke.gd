# Accessibility semantics (G-a11y): focus_nodes_list_order.riv authors
# four button semantic nodes. With accessibility_enabled, the rive
# semantics diff must flow render-thread -> mailbox -> RiveControl's
# mirrored node set. (OS-level AccessKit publishing additionally requires
# assistive tech to be active — this asserts our side of the pipeline.)
#   godot --path tests/project semantics_smoke.tscn
extends Control

var control: RiveControl
var frames := 0


func fail(msg: String) -> void:
	push_error("SEMANTICS FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	control = RiveControl.new()
	control.file = load("res://fixtures/semantic/focus_nodes_list_order.riv")
	control.position = Vector2(32, 32)
	control.size = Vector2(400, 400)
	control.accessibility_enabled = true
	add_child(control)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 90:
		var count: int = control.get_semantics_node_count()
		if count != 4:
			fail("expected 4 semantic button nodes, got %d" % count)
			return
		print("SEMANTICS OK: 4 authored buttons mirrored")
		get_tree().quit(0)

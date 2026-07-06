# Phase 0 headless smoke: the extension loads, a .riv imports, metadata
# enumerates. Run:
#   godot --headless --path tests/project --script smoke.gd
extends SceneTree


func _init() -> void:
	var repo_root := ProjectSettings.globalize_path("res://").path_join("../..")
	var riv_path := repo_root.path_join(
			"thirdparty/rive-runtime/tests/unit_tests/assets/bullet_man.riv")

	if not ClassDB.class_exists("RiveFileResource"):
		push_error("SMOKE FAIL: RiveFileResource not registered (extension not loaded)")
		quit(1)
		return

	var file := FileAccess.open(riv_path, FileAccess.READ)
	if file == null:
		push_error("SMOKE FAIL: cannot open fixture: " + riv_path)
		quit(1)
		return

	var res: RiveFileResource = RiveFileResource.new()
	res.set_data(file.get_buffer(file.get_length()))
	if not res.is_valid():
		push_error("SMOKE FAIL: import failed: " + res.get_import_error())
		quit(1)
		return

	var artboards := res.get_artboard_names()
	print("artboards: ", artboards)
	if artboards.is_empty():
		push_error("SMOKE FAIL: no artboards")
		quit(1)
		return

	var machines := res.get_state_machine_names(artboards[0])
	print("state machines of '%s': %s" % [artboards[0], machines])

	# Garbage data must fail cleanly, not crash.
	var bad: RiveFileResource = RiveFileResource.new()
	bad.set_data(PackedByteArray([0xDE, 0xAD, 0xBE, 0xEF]))
	if bad.is_valid():
		push_error("SMOKE FAIL: garbage bytes reported valid")
		quit(1)
		return
	print("garbage rejected with: ", bad.get_import_error())

	print("SMOKE OK")
	quit(0)

# Phase 1 render smoke: draw a .riv through the full Vulkan bridge +
# Texture2DRD path, screenshot the viewport, quit. Exit 0 on success.
#   godot --path tests/project render_smoke.tscn
extends Node2D

var frames := 0


func _ready() -> void:
	var repo_root := ProjectSettings.globalize_path("res://").path_join("../..")
	var riv_path := repo_root.path_join(
			"thirdparty/rive-runtime/tests/unit_tests/assets/bullet_man.riv")

	var res: RiveFileResource = RiveFileResource.new()
	res.set_data(FileAccess.get_file_as_bytes(riv_path))
	if not res.is_valid():
		push_error("RENDER SMOKE FAIL: import failed")
		get_tree().quit(1)
		return

	var sprite: RiveSprite2D = RiveSprite2D.new()
	sprite.file = res
	sprite.size = Vector2i(512, 512)
	sprite.position = Vector2(64, 44)
	add_child(sprite)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 60 or frames == 300:
		var img := get_viewport().get_texture().get_image()
		var out_path := ProjectSettings.globalize_path("res://").path_join(
				"../../out/render_smoke_%d.png" % frames)
		var err := img.save_png(out_path)
		if err != OK:
			push_error("RENDER SMOKE FAIL: could not save screenshot")
			get_tree().quit(1)
			return
		# Content assertion: a frame that is one flat color means nothing
		# rendered (this exact failure shipped once — never again).
		var probe := img.get_pixel(200, 200)
		var flat := true
		for x in range(100, 500, 40):
			for y in range(100, 500, 40):
				if img.get_pixel(x, y) != probe:
					flat = false
		if flat:
			push_error("RENDER SMOKE FAIL: frame is a flat color — no content")
			get_tree().quit(1)
			return
		print("RENDER SMOKE OK -> ", out_path)
		if frames == 300:
			get_tree().quit(0)

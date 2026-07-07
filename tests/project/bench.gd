# Tier 5 benchmark (GOALS G7.3 / success criterion 4): N live artboards,
# measure frame times, compare against a budget.
#   godot --path tests/project bench.tscn
# Env: RIVEGD_BENCH_COUNT (default 50), RIVEGD_BENCH_BUDGET_MS (default 16.0),
#      RIVEGD_BENCH_FIXTURE (res:// path), RIVEGD_BENCH_ARTBOARD,
#      RIVEGD_BENCH_SIZE (square texture px)
extends Node2D

const WARMUP_FRAMES := 90
const MEASURE_FRAMES := 300

var frames := 0
var samples: Array[float] = []


func _ready() -> void:
	var count := int(OS.get_environment("RIVEGD_BENCH_COUNT").to_int())
	if count <= 0:
		count = 50
	var res := load(OS.get_environment("RIVEGD_BENCH_FIXTURE")
		if OS.get_environment("RIVEGD_BENCH_FIXTURE") != ""
		else "res://fixtures/bullet_man.riv")
	if res == null:
		push_error("BENCH FAIL: fixture missing")
		get_tree().quit(1)
		return
	var columns := int(ceil(sqrt(float(count))))
	for i in count:
		var sprite: RiveSprite2D = RiveSprite2D.new()
		sprite.file = res
		var ab := OS.get_environment("RIVEGD_BENCH_ARTBOARD")
		if ab != "":
			sprite.artboard = ab
		var px := int(OS.get_environment("RIVEGD_BENCH_SIZE").to_int())
		sprite.size = Vector2i(px, px) if px > 0 else Vector2i(128, 128)
		sprite.position = Vector2(
				(i % columns) * 130 + 8, (i / columns) * 130 + 8)
		add_child(sprite)
	print("bench: %d artboards" % count)


func _process(delta: float) -> void:
	frames += 1
	if frames <= WARMUP_FRAMES:
		return
	samples.push_back(delta * 1000.0)
	if samples.size() < MEASURE_FRAMES:
		return

	samples.sort()
	var sum := 0.0
	for sample in samples:
		sum += sample
	var avg := sum / samples.size()
	var p95: float = samples[int(samples.size() * 0.95)]
	var p99: float = samples[int(samples.size() * 0.99)]

	var budget := OS.get_environment("RIVEGD_BENCH_BUDGET_MS").to_float()
	if budget <= 0.0:
		budget = 16.0

	var report := {
		"artboards": get_child_count(),
		"frames": samples.size(),
		"avg_ms": snapped(avg, 0.01),
		"p95_ms": snapped(p95, 0.01),
		"p99_ms": snapped(p99, 0.01),
		"budget_ms": budget,
		"gpu": RenderingServer.get_video_adapter_name(),
	}
	var out_path := ProjectSettings.globalize_path("res://").path_join(
			"../../out/bench.json")
	FileAccess.open(out_path, FileAccess.WRITE).store_string(
			JSON.stringify(report, "  "))
	print("BENCH RESULT: ", report)

	if p95 > budget:
		push_error("BENCH FAIL: p95 %.2fms > budget %.2fms" % [p95, budget])
		get_tree().quit(1)
		return
	print("BENCH OK")
	get_tree().quit(0)

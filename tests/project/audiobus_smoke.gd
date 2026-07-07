# Per-node audio bus routing (G5.3): a node with audio_bus set routes ITS
# Rive audio through a dedicated engine into an internal AudioStreamPlayer
# on that bus. sound2.riv fires an AudioEvent on autoplay; the internal
# player's playback must show a real peak.
#   godot --path tests/project audiobus_smoke.tscn
extends Node2D

var sprite: Node
var frames := 0
var peak := 0.0
var playback: Object


func fail(msg: String) -> void:
	push_error("AUDIOBUS FAIL: " + msg)
	get_tree().quit(1)


func _ready() -> void:
	# A dedicated bus, added at runtime.
	AudioServer.add_bus()
	AudioServer.set_bus_name(AudioServer.bus_count - 1, "RiveBus")

	sprite = ClassDB.instantiate("RiveSprite2D")
	sprite.set("file", load("res://fixtures/sound2.riv"))
	sprite.set("audio_bus", "RiveBus")
	add_child(sprite)


func _process(_delta: float) -> void:
	frames += 1
	if frames == 20:
		# Find the internal player and its live playback.
		for child in sprite.get_children(true):
			if child is AudioStreamPlayer:
				if child.bus != &"RiveBus":
					fail("internal player not on RiveBus (bus=%s)" % child.bus)
					return
				playback = child.get_stream_playback()
		if playback == null:
			fail("no internal AudioStreamPlayer with live playback found")
			return
	elif frames > 20 and frames < 200:
		if playback != null:
			peak = max(peak, playback.get_last_peak())
	elif frames == 200:
		if peak <= 0.0:
			fail("no audio flowed through the per-node bus (peak 0)")
			return
		print("AUDIOBUS OK: peak %.4f through RiveBus" % peak)
		get_tree().quit(0)

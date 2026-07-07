#pragma once

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>

namespace rivegd {

// Routes Rive's audio (audio events fired by state machines) into Godot's
// audio server: add to any AudioStreamPlayer and play it. All Rive
// instances share one external miniaudio engine; this stream pulls its
// mixed PCM from the audio thread (GOALS G5.3).
class RiveAudioStream : public godot::AudioStream {
    GDCLASS(RiveAudioStream, godot::AudioStream)

public:
    godot::Ref<godot::AudioStreamPlayback> _instantiate_playback() const override;
    godot::String _get_stream_name() const override { return "RiveAudio"; }
    double _get_length() const override { return 0.0; }
    bool _is_monophonic() const override { return false; }

protected:
    static void _bind_methods() {}
};

class RiveAudioStreamPlayback : public godot::AudioStreamPlayback {
    GDCLASS(RiveAudioStreamPlayback, godot::AudioStreamPlayback)

public:
    void _start(double p_from_pos) override;
    void _stop() override;
    bool _is_playing() const override { return active; }
    int32_t _mix(godot::AudioFrame* p_buffer, float p_rate_scale,
                 int32_t p_frames) override;

    // Peak of the last mixed block — lets tests assert audio actually flowed.
    float get_last_peak() const { return last_peak; }

protected:
    static void _bind_methods();

private:
    bool active = false;
    float last_peak = 0.0f;
};

} // namespace rivegd

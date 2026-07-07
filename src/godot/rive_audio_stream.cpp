#include "godot/rive_audio_stream.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

Ref<AudioStreamPlayback> RiveAudioStream::_instantiate_playback() const {
    Ref<RiveAudioStreamPlayback> playback;
    playback.instantiate();
    playback->instance_id = instance_id;
    return playback;
}

void RiveAudioStream::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_instance_id", "id"),
                         &RiveAudioStream::set_instance_id);
    ClassDB::bind_method(D_METHOD("get_instance_id"),
                         &RiveAudioStream::get_instance_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "instance_id"),
                 "set_instance_id", "get_instance_id");
}

void RiveAudioStreamPlayback::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_last_peak"),
                         &RiveAudioStreamPlayback::get_last_peak);
}

void RiveAudioStreamPlayback::_start(double) { active = true; }

void RiveAudioStreamPlayback::_stop() { active = false; }

int32_t RiveAudioStreamPlayback::_mix(AudioFrame* p_buffer, float,
                                      int32_t p_frames) {
    // AudioFrame is {float left, right} — identical layout to rive's
    // interleaved stereo output, so mix straight into the buffer.
    float* out = reinterpret_cast<float*>(p_buffer);
    int mixed = 0;
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server != nullptr) {
        mixed = instance_id != 0
                    ? server->mix_audio_instance(instance_id, out, p_frames)
                    : server->mix_audio(out, p_frames);
    }
    float peak = 0.0f;
    for (int i = 0; i < mixed * 2; ++i) {
        peak = MAX(peak, ABS(out[i]));
    }
    // Keep the stream alive with silence when Rive has nothing playing.
    for (int i = mixed * 2; i < p_frames * 2; ++i) {
        out[i] = 0.0f;
    }
    last_peak = peak;
    return p_frames;
}

} // namespace rivegd

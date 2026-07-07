#pragma once

#include "godot/rive_instance.h"

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node2d.hpp>

namespace rivegd {

// World-space 2D node that plays a Rive artboard (GOALS G4.1).
class RiveSprite2D : public godot::Node2D {
    GDCLASS(RiveSprite2D, godot::Node2D)

public:
    void set_file(const godot::Ref<RiveFileResource>& p_file);
    godot::Ref<RiveFileResource> get_file() const { return rive.file; }

    void set_artboard(const godot::String& p_artboard);
    godot::String get_artboard() const { return rive.artboard; }

    // Non-empty routes THIS node's Rive audio to that Godot bus via an
    // internal AudioStreamPlayer + dedicated engine (G5.3 per-node
    // routing). Empty (default) mixes into the shared global stream.
    void set_audio_bus(const godot::String& p_bus);
    godot::String get_audio_bus() const { return audio_bus; }

    void set_fit(int p_fit);
    int get_fit() const { return rive.fit; }
    void set_alignment(int p_alignment);
    int get_alignment() const { return alignment_index; }

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return rive.state_machine; }

    void set_size(const godot::Vector2i& p_size);
    godot::Vector2i get_size() const { return size; }

    void set_playing(bool p_playing);
    bool is_playing() const { return playing; }

    void set_pause_when_hidden(bool p_pause);
    bool get_pause_when_hidden() const { return pause_when_hidden; }

    void set_speed_scale(double p_speed_scale);
    double get_speed_scale() const { return speed_scale; }

    void set_bool_input(const godot::String& p_name, bool p_value);
    void set_number_input(const godot::String& p_name, double p_value);
    void fire_trigger(const godot::String& p_name);

    void set_property(const godot::String& p_path, const godot::Variant& p_value);
    void fire_property_trigger(const godot::String& p_path);
    void watch_property(const godot::String& p_path);
    godot::Variant get_property(const godot::String& p_path) const;

    // Targeted callables (G4.3 tier 2): sugar over the rive_event /
    // property_changed signals. Each returns the connected Callable so it
    // can be passed to disconnect() to unsubscribe.
    godot::Callable on_event(const godot::String& p_name,
                             const godot::Callable& p_callable);
    godot::Callable on_property(const godot::String& p_path,
                                const godot::Callable& p_callable);
    void list_append(const godot::String& p_path,
                     const godot::String& p_view_model,
                     const godot::String& p_instance_name);
    void list_remove_at(const godot::String& p_path, int p_index);
    void list_swap(const godot::String& p_path, int p_a, int p_b);
    void list_clear(const godot::String& p_path);
    void list_set_property(const godot::String& p_path, int p_index,
                           const godot::String& p_sub_path,
                           const godot::Variant& p_value);
    void list_read_property(const godot::String& p_path, int p_index,
                            const godot::String& p_sub_path);
    void set_artboard_property(const godot::String& p_path,
                               const godot::String& p_artboard_name);
    void replace_view_model(const godot::String& p_path,
                            const godot::String& p_view_model,
                            const godot::String& p_instance_name);

    godot::PackedStringArray _get_configuration_warnings() const override;
    void _notification(int p_what);
    bool _set(const godot::StringName& p_name, const godot::Variant& p_value);
    bool _get(const godot::StringName& p_name, godot::Variant& r_value) const;
    void _get_property_list(godot::List<godot::PropertyInfo>* p_list) const;
    void _validate_property(godot::PropertyInfo& p_property) const;

    // Signal-forwarding dispatchers for on_event/on_property (bound
    // callables; not meant to be called directly).
    void _dispatch_event(const godot::String& p_event_name,
                         const godot::Dictionary& p_properties,
                         const godot::String& p_want,
                         const godot::Callable& p_callable);
    void _dispatch_property(const godot::String& p_path,
                            const godot::Variant& p_value,
                            const godot::String& p_want,
                            const godot::Callable& p_callable);

protected:
    static void _bind_methods();

private:
    void recreate_instance();

    RiveInstance rive;
    int alignment_index = 4; // Center
    godot::String audio_bus;
    godot::AudioStreamPlayer* audio_player = nullptr;
    void update_audio_player();
    godot::Vector2i size = godot::Vector2i(512, 512);
    bool playing = true;
    bool pause_when_hidden = true;
    double speed_scale = 1.0;
};

} // namespace rivegd

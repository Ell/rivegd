#pragma once

#include "godot/rive_instance.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>

namespace rivegd {

// UI node that plays a Rive artboard inside a Control rect and forwards
// pointer input to the state machine, so Rive listeners (hover/click/drag)
// work with zero code (GOALS G4.1/G4.7).
//
// The render texture tracks the control's size; resizes are debounced so a
// live drag doesn't re-import the file every frame.
class RiveControl : public godot::Control {
    GDCLASS(RiveControl, godot::Control)

public:
    void set_file(const godot::Ref<RiveFileResource>& p_file);
    godot::Ref<RiveFileResource> get_file() const { return rive.file; }

    void set_artboard(const godot::String& p_artboard);
    godot::String get_artboard() const { return rive.artboard; }

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return rive.state_machine; }

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
    void list_append(const godot::String& p_path,
                     const godot::String& p_view_model,
                     const godot::String& p_instance_name);
    void list_remove_at(const godot::String& p_path, int p_index);
    void list_swap(const godot::String& p_path, int p_a, int p_b);
    void list_clear(const godot::String& p_path);
    void list_set_property(const godot::String& p_path, int p_index,
                           const godot::String& p_sub_path,
                           const godot::Variant& p_value);

    godot::Vector2 _get_minimum_size() const override;
    void _gui_input(const godot::Ref<godot::InputEvent>& p_event) override;

    godot::PackedStringArray _get_configuration_warnings() const override;
    void _notification(int p_what);
    bool _set(const godot::StringName& p_name, const godot::Variant& p_value);
    bool _get(const godot::StringName& p_name, godot::Variant& r_value) const;
    void _get_property_list(godot::List<godot::PropertyInfo>* p_list) const;
    void _validate_property(godot::PropertyInfo& p_property) const;

protected:
    static void _bind_methods();

private:
    void recreate_instance();
    godot::Vector2i texture_size() const;

    RiveInstance rive;
    bool playing = true;
    bool pause_when_hidden = true;
    double speed_scale = 1.0;

    // Resize debounce: recreate the texture only after the size has been
    // stable for this long.
    static constexpr double kResizeDebounceSeconds = 0.3;
    double resize_cooldown = 0.0;
    godot::Vector2i live_texture_size;
};

} // namespace rivegd

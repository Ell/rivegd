#pragma once

#include "godot/rive_instance.h"

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

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return rive.state_machine; }

    void set_size(const godot::Vector2i& p_size);
    godot::Vector2i get_size() const { return size; }

    void set_playing(bool p_playing);
    bool is_playing() const { return playing; }

    void set_speed_scale(double p_speed_scale);
    double get_speed_scale() const { return speed_scale; }

    void set_bool_input(const godot::String& p_name, bool p_value);
    void set_number_input(const godot::String& p_name, double p_value);
    void fire_trigger(const godot::String& p_name);

    void set_property(const godot::String& p_path, const godot::Variant& p_value);
    void fire_property_trigger(const godot::String& p_path);

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

    RiveInstance rive;
    godot::Vector2i size = godot::Vector2i(512, 512);
    bool playing = true;
    double speed_scale = 1.0;
};

} // namespace rivegd

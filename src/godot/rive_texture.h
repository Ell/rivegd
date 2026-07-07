#pragma once

#include "godot/rive_instance.h"

#include <godot_cpp/classes/texture2drd.hpp>

namespace rivegd {

// Rive output as a plain Texture2D (GOALS G4.1): usable in any material,
// TextureRect, 3D surface, particle, or shader uniform. Advances itself via
// RenderingServer's frame_pre_draw signal, so it animates anywhere it is
// drawn — no scene-tree node required.
class RiveTexture : public godot::Texture2DRD {
    GDCLASS(RiveTexture, godot::Texture2DRD)

public:
    RiveTexture();
    ~RiveTexture() override;

    void set_file(const godot::Ref<RiveFileResource>& p_file);
    godot::Ref<RiveFileResource> get_file() const { return rive.file; }

    void set_artboard(const godot::String& p_artboard);
    godot::String get_artboard() const { return rive.artboard; }

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return rive.state_machine; }

    void set_render_size(const godot::Vector2i& p_size);
    godot::Vector2i get_render_size() const { return render_size; }

    void set_playing(bool p_playing);
    bool is_playing() const { return playing; }

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
    void on_frame_pre_draw();

    RiveInstance rive;
    godot::Vector2i render_size = godot::Vector2i(512, 512);
    bool playing = true;
    double speed_scale = 1.0;
    uint64_t last_frame_usec = 0;
    bool frame_hook_connected = false;
};

} // namespace rivegd

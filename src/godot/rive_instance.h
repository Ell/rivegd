#pragma once

#include "godot/rive_file_resource.h"

#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace rivegd {

// Main-thread controller shared by RiveSprite2D and RiveControl: owns the
// server instance id, the Texture2DRD binding, the input cache, and the
// dynamic-property plumbing. All Rive state lives on the render thread;
// this class only posts work and polls mailboxes.
class RiveInstance {
public:
    ~RiveInstance() { release(); }

    godot::Ref<RiveFileResource> file;
    godot::String artboard;
    godot::String state_machine;

    // Posts instance creation (and replays cached inputs). No-op without a
    // valid file.
    void create(const godot::Vector2i& p_size);
    void release();
    bool is_live() const { return instance_id != 0; }

    void frame(double p_delta);

    // Polls the RID mailbox; returns true the moment the texture binds.
    bool update_texture_binding();
    godot::Ref<godot::Texture2DRD> get_texture() const { return texture; }

    godot::Array take_events();
    godot::Array take_state_changes();

    void set_bool_input(const godot::String& p_name, bool p_value);
    void set_number_input(const godot::String& p_name, double p_value);
    void fire_trigger(const godot::String& p_name);

    // Data binding: path-addressed view model property writes
    // (bool/int/float/String/Color dispatched by Variant type).
    void set_property(const godot::String& p_path, const godot::Variant& p_value);
    void fire_property_trigger(const godot::String& p_path);

    // Data binding reads: watch a property (change reports flow through
    // take_property_changes; the current value is reported immediately).
    // get_property returns the last value seen on the main thread.
    void watch_property(const godot::String& p_path);
    godot::Variant get_property(const godot::String& p_path) const;
    godot::Array take_property_changes();
    void pointer(int p_phase, const godot::Vector2& p_local,
                 const godot::Vector2& p_node_size);

    // Dynamic inspector support (delegated from the owning node).
    void get_property_list(godot::List<godot::PropertyInfo>* p_list) const;
    bool try_set(const godot::StringName& p_name, const godot::Variant& p_value);
    bool try_get(const godot::StringName& p_name, godot::Variant& r_value) const;
    void validate_property(godot::PropertyInfo& p_property) const;

private:
    void post_input(const godot::String& p_name, const godot::Variant& p_value);

    int64_t instance_id = 0;
    godot::Ref<godot::Texture2DRD> texture;
    bool texture_bound = false;
    void post_property(const godot::String& p_path,
                       const godot::Variant& p_value);
    void post_watch(const godot::String& p_path);

    // Inspector/script-set values, replayed whenever the instance recreates.
    godot::HashMap<godot::String, godot::Variant> input_values;
    godot::HashMap<godot::String, godot::Variant> property_values;
    // Watched paths (replayed on recreate) and last values seen.
    godot::HashMap<godot::String, bool> watched_paths;
    godot::HashMap<godot::String, godot::Variant> property_cache;
};

} // namespace rivegd

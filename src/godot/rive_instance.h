#pragma once

#include "godot/rive_file_resource.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
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
    // Artboard->texture fit (RiveRenderServer::FitMode) and alignment in
    // [-1,1] per axis (used by every mode except FILL/LAYOUT). Set before
    // create(); changing them on a node recreates the instance.
    int fit = 0;
    godot::Vector2 alignment;
    // FIT_LAYOUT content scale: artboard is laid out at node_size/scale
    // and drawn scaled up (Unity "Layout Scale Factor"; constant-physical
    // DPI = DisplayServer.screen_get_dpi() / your reference DPI).
    double layout_scale = 1.0;
    // Create a dedicated AudioEngine for this instance (per-node bus
    // routing): pair with RiveAudioStream.instance_id = get_instance_id().
    bool dedicated_audio = false;

    int64_t get_instance_id() const { return instance_id; }

    // Posts instance creation (and replays cached inputs). No-op without a
    // valid file.
    void create(const godot::Vector2i& p_size);
    void release();
    bool is_live() const { return instance_id != 0; }

    void frame(double p_delta);

    // Live resize (fit = Layout): reflow the artboard immediately (cheap,
    // preserves all state); swap the texture separately once the size has
    // settled — the instance, state machine, and view model survive.
    void resize_artboard(const godot::Vector2& p_logical_size);
    void resize_texture(const godot::Vector2i& p_size);

    // Polls the RID mailboxes; returns true the moment the texture binds.
    bool update_texture_binding();
    // RS-level texture (canvas drawing; valid on every backend).
    godot::RID get_canvas_texture_rid() const { return canvas_texture; }
    // Raw RD texture (Texture2DRD path; empty under the GL backend).
    godot::RID get_rd_texture_rid() const { return rd_texture; }

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

    // Accessibility: enable semantics draining; changes arrive as node
    // payload Dictionaries (texture-space bounds). Replayed on recreate.
    void set_semantics_enabled(bool p_enabled);
    godot::Array take_semantics();
    bool semantics_enabled = false;

    // List properties. Structural ops are render-thread state and do not
    // survive instance rebuilds; watch the list path to observe its size.
    void list_append(const godot::String& p_path,
                     const godot::String& p_view_model,
                     const godot::String& p_instance_name);
    void list_remove_at(const godot::String& p_path, int p_index);
    void list_swap(const godot::String& p_path, int p_a, int p_b);
    void list_clear(const godot::String& p_path);
    void list_set_property(const godot::String& p_path, int p_index,
                           const godot::String& p_sub_path,
                           const godot::Variant& p_value);
    // Async read: value arrives via property_changed at path
    // "<path>[<index>]/<sub_path>" and is cached for get_property.
    void list_read_property(const godot::String& p_path, int p_index,
                            const godot::String& p_sub_path);

    // Artboard properties and nested view-model instance swapping.
    void set_artboard_property(const godot::String& p_path,
                               const godot::String& p_artboard_name);
    void replace_view_model(const godot::String& p_path,
                            const godot::String& p_view_model,
                            const godot::String& p_instance_name);
    void pointer(int p_phase, const godot::Vector2& p_local,
                 const godot::Vector2& p_node_size, int p_pointer_id = 0);
    // Synchronous listener hit query (see RiveRenderServer::hit_test).
    bool hit_test(const godot::Vector2& p_local,
                  const godot::Vector2& p_node_size, bool p_default) const;
    void key(int p_rive_key, int p_modifiers, bool p_pressed, bool p_repeat);
    void text_input(const godot::String& p_text);
    void focus_move(int p_direction);
    void gamepads(const godot::PackedByteArray& p_batch);

    // Dynamic inspector support (delegated from the owning node).
    void get_property_list(godot::List<godot::PropertyInfo>* p_list) const;
    bool try_set(const godot::StringName& p_name, const godot::Variant& p_value);
    bool try_get(const godot::StringName& p_name, godot::Variant& r_value) const;
    void validate_property(godot::PropertyInfo& p_property) const;

private:
    void post_input(const godot::String& p_name, const godot::Variant& p_value);

    int64_t instance_id = 0;
    // Shared queue import (one per resource, refcounted); file_handle is
    // its handle, cached for the queue calls below.
    RiveFileResource::SharedFile* shared_file = nullptr;
    uint64_t file_handle = 0;
    uint64_t artboard_handle = 0;
    uint64_t state_machine_handle = 0;
    godot::RID canvas_texture;
    godot::RID rd_texture;
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

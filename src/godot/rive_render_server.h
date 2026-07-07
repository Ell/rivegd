#pragma once

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>

namespace rive {
class ArtboardInstance;
class File;
class StateMachineInstance;
template <typename T> class rcp;
} // namespace rive

namespace rivegd {

namespace render {
class RenderBridge;
}

// Render-thread owner of all live Rive GPU state.
//
// Threading model (Phase 1): main thread allocates ids and posts work via
// RenderingServer::call_on_render_thread(); every rt_* method and all Rive
// object lifetimes live on the render thread. The only cross-thread state
// is the texture-RID mailbox, guarded by a mutex.
class RiveRenderServer : public godot::Object {
    GDCLASS(RiveRenderServer, godot::Object)

public:
    RiveRenderServer();
    ~RiveRenderServer() override; // out-of-line: VulkanBridge is incomplete here

    static RiveRenderServer* get_singleton();
    static void create_singleton();
    static void free_singleton();

    // Main thread: reserve an instance id.
    int64_t allocate_instance_id();

    // Main thread: RID mailboxes (filled by rt_init_instance).
    godot::RID get_texture_rid(int64_t p_instance_id);        // RD texture
    godot::RID get_canvas_texture_rid(int64_t p_instance_id); // RS texture

    // Main thread: drain events reported since the last call (Array of
    // Dictionaries: { name, seconds_delay, properties }).
    godot::Array take_events(int64_t p_instance_id);

    // Main thread: drain state-change notifications (Array of state names).
    godot::Array take_state_changes(int64_t p_instance_id);

    // Main thread: drain watched view-model property changes (Array of
    // { path, value } Dictionaries).
    godot::Array take_property_changes(int64_t p_instance_id);

    enum PointerPhase { POINTER_MOVE = 0, POINTER_DOWN, POINTER_UP, POINTER_EXIT };

    // Render thread only.
    void rt_init_instance(int64_t p_instance_id,
                          const godot::PackedByteArray& p_data,
                          const godot::String& p_artboard,
                          const godot::String& p_state_machine,
                          const godot::Vector2i& p_size);
    void rt_frame(int64_t p_instance_id, double p_delta); // advance only
    // Renders every instance that needs it in one batch (one submission).
    // Posted once per engine frame from the frame_pre_draw hook.
    void rt_flush_all();
    void rt_free_instance(int64_t p_instance_id);
    void rt_set_bool(int64_t p_instance_id, const godot::String& p_name,
                     bool p_value);
    void rt_set_number(int64_t p_instance_id, const godot::String& p_name,
                       double p_value);
    void rt_fire_trigger(int64_t p_instance_id, const godot::String& p_name);
    // p_local is in node-local pixels; p_node_size the node's drawn size —
    // mapped through the same contain-fit transform used for drawing.
    void rt_pointer(int64_t p_instance_id, int p_phase,
                    const godot::Vector2& p_local,
                    const godot::Vector2& p_node_size);

    // Data binding (view model) writes; p_path is Rive's slash-delimited
    // property path (e.g. "stats/health").
    void rt_set_vm_bool(int64_t p_instance_id, const godot::String& p_path,
                        bool p_value);
    void rt_set_vm_number(int64_t p_instance_id, const godot::String& p_path,
                          double p_value);
    void rt_set_vm_string(int64_t p_instance_id, const godot::String& p_path,
                          const godot::String& p_value);
    void rt_set_vm_color(int64_t p_instance_id, const godot::String& p_path,
                         const godot::Color& p_value);
    void rt_fire_vm_trigger(int64_t p_instance_id, const godot::String& p_path);

    // Start reporting changes of a view-model property through
    // take_property_changes (its current value is reported immediately).
    void rt_watch_vm_property(int64_t p_instance_id,
                              const godot::String& p_path);

    // Image property: p_png_bytes is an encoded image (PNG/JPEG/WebP),
    // decoded on the render thread through the context factory.
    void rt_set_vm_image(int64_t p_instance_id, const godot::String& p_path,
                         const godot::PackedByteArray& p_png_bytes);
    // Artboard property: binds the named artboard from the same file.
    void rt_set_vm_artboard(int64_t p_instance_id, const godot::String& p_path,
                            const godot::String& p_artboard_name);
    // Replaces a nested view model instance (empty instance name = fresh
    // instance of the named view model).
    void rt_replace_view_model(int64_t p_instance_id,
                               const godot::String& p_path,
                               const godot::String& p_view_model,
                               const godot::String& p_instance_name);

    // List properties (path-addressed; items are view-model instances).
    // List state lives on the render thread and does not survive instance
    // rebuilds (no replay for structural ops).
    void rt_list_append(int64_t p_instance_id, const godot::String& p_path,
                        const godot::String& p_view_model,
                        const godot::String& p_instance_name);
    void rt_list_remove_at(int64_t p_instance_id, const godot::String& p_path,
                           int p_index);
    void rt_list_swap(int64_t p_instance_id, const godot::String& p_path,
                      int p_a, int p_b);
    void rt_list_clear(int64_t p_instance_id, const godot::String& p_path);
    // Sets a property on the list item at p_index (Variant-dispatched like
    // rt_set_vm_*).
    void rt_list_set(int64_t p_instance_id, const godot::String& p_path,
                     int p_index, const godot::String& p_sub_path,
                     const godot::Variant& p_value);

    // Blocking render for editor thumbnails: safe to call from any
    // non-render thread (the editor's preview thread); posts a one-shot
    // render to the render thread and waits (with timeout).
    godot::Ref<godot::Image> render_thumbnail(
        const godot::PackedByteArray& p_data, const godot::Vector2i& p_size);
    void rt_render_thumbnail(const godot::PackedByteArray& p_data,
                             const godot::Vector2i& p_size);

protected:
    static void _bind_methods();

private:
    struct Instance;

    void on_frame_pre_draw(); // main thread: posts rt_flush_all
    void rt_render_instance(int64_t p_instance_id, Instance* p_instance);

    bool rt_ensure_bridge();

    static RiveRenderServer* singleton;

    std::unique_ptr<render::RenderBridge> bridge; // render thread only
    godot::HashMap<int64_t, Instance*> instances; // render thread only
    bool bridge_failed = false;                   // render thread only
    bool frame_hook_connected = false;            // main thread only

    std::mutex mailbox_mutex;
    godot::HashMap<int64_t, godot::RID> texture_mailbox;
    godot::HashMap<int64_t, godot::RID> canvas_texture_mailbox;
    godot::HashMap<int64_t, godot::Array> event_mailbox;
    godot::HashMap<int64_t, godot::Array> state_mailbox;
    godot::HashMap<int64_t, godot::Array> property_mailbox;

    std::atomic<int64_t> next_instance_id{1};

    std::mutex thumbnail_mutex;
    std::condition_variable thumbnail_done;
    godot::Ref<godot::Image> thumbnail_result;
    bool thumbnail_ready = false;
};

} // namespace rivegd

#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

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
class VulkanBridge;
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

    // Main thread: RID mailbox (filled by rt_init_instance).
    godot::RID get_texture_rid(int64_t p_instance_id);

    // Main thread: drain events reported since the last call (Array of
    // Dictionaries: { name, seconds_delay, properties }).
    godot::Array take_events(int64_t p_instance_id);

    enum PointerPhase { POINTER_MOVE = 0, POINTER_DOWN, POINTER_UP, POINTER_EXIT };

    // Render thread only.
    void rt_init_instance(int64_t p_instance_id,
                          const godot::PackedByteArray& p_data,
                          const godot::String& p_artboard,
                          const godot::String& p_state_machine,
                          const godot::Vector2i& p_size);
    void rt_frame(int64_t p_instance_id, double p_delta);
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

protected:
    static void _bind_methods();

private:
    struct Instance;

    bool rt_ensure_bridge();

    static RiveRenderServer* singleton;

    std::unique_ptr<render::VulkanBridge> bridge; // render thread only
    godot::HashMap<int64_t, Instance*> instances; // render thread only
    bool bridge_failed = false;                   // render thread only

    std::mutex mailbox_mutex;
    godot::HashMap<int64_t, godot::RID> texture_mailbox;
    godot::HashMap<int64_t, godot::Array> event_mailbox;

    std::atomic<int64_t> next_instance_id{1};
};

} // namespace rivegd

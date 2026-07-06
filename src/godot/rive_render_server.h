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

    // Render thread only.
    void rt_init_instance(int64_t p_instance_id,
                          const godot::PackedByteArray& p_data,
                          const godot::String& p_artboard,
                          const godot::String& p_state_machine,
                          const godot::Vector2i& p_size);
    void rt_frame(int64_t p_instance_id, double p_delta);
    void rt_free_instance(int64_t p_instance_id);

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

    std::atomic<int64_t> next_instance_id{1};
};

} // namespace rivegd

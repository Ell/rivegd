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
class AudioEngine;
class CommandQueue;
class CommandServer;
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

    // The command queue: all instance mutations flow through its FIFO (M1);
    // the server pumps it on the render thread each frame.
    rive::CommandQueue* queue();

    // Ensures a pump+flush runs this frame. frame_pre_draw normally drives
    // it, but that signal never fires headless — writers call this after
    // queueing (deduped by an atomic flag cleared in rt_flush_all).
    void request_pump();
    // Immediate pump, bypassing the frame_pre_draw deferral — for teardown
    // paths that must run even when no further frame will be drawn (quit).
    void request_pump_now();

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
    // Artboard->texture fit modes (mirrors rive::Fit).
    enum FitMode {
        FIT_CONTAIN = 0,
        FIT_COVER,
        FIT_FILL,
        FIT_WIDTH,
        FIT_HEIGHT,
        FIT_NONE,
        FIT_SCALE_DOWN,
        FIT_LAYOUT,
    };

    // Render thread only (invoked through CommandQueue::runOnce closures).
    // Handles were minted by queue lifecycle calls on the main thread.
    void rt_init_instance(int64_t p_instance_id, uint64_t p_file_handle,
                          uint64_t p_artboard_handle,
                          uint64_t p_state_machine_handle,
                          const godot::Vector2i& p_size, int p_fit,
                          const godot::Vector2& p_alignment,
                          bool p_dedicated_audio, float p_layout_scale);
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
                    const godot::Vector2& p_node_size, int p_pointer_id);

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

    // Keyboard/focus input (GOALS G4.7). Key codes use rive's GLFW-style
    // values; RiveControl maps Godot keycodes before posting.
    void rt_key(int64_t p_instance_id, int p_rive_key, int p_modifiers,
                bool p_pressed, bool p_repeat);
    void rt_text_input(int64_t p_instance_id, const godot::String& p_text);
    // 0 = next, 1 = previous, 2 = left, 3 = right, 4 = up, 5 = down.
    void rt_focus_move(int64_t p_instance_id, int p_direction);
    // Gamepad batch in rive's wire format (version 2, little-endian).
    void rt_gamepads(int64_t p_instance_id, const godot::PackedByteArray& p_batch);

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
    // Synchronous, main-thread-safe listener hit query (const geometry
    // walk; guarded by flush_mutex). Returns true when an interactive
    // listener is under the point. p_default when the instance is not
    // live yet.
    bool hit_test(int64_t p_instance_id, const godot::Vector2& p_local,
                  const godot::Vector2& p_node_size, bool p_default);

    // Accessibility (G-a11y): enable per-instance semantics draining; each
    // non-empty diff lands in the semantics mailbox as node payloads with
    // TEXTURE-space bounds (the instance fit transform is pre-applied).
    void rt_set_semantics_enabled(int64_t p_instance_id, bool p_enabled);
    godot::Array take_semantics(int64_t p_instance_id);

    // Live resize (fit = Layout): reflow now, swap the texture at the
    // debounce — the instance and its state survive.
    void rt_resize_artboard(int64_t p_instance_id,
                            const godot::Vector2& p_logical_size);
    void rt_resize_texture(int64_t p_instance_id,
                           const godot::Vector2i& p_size);

    // Reads a list item's scalar and posts it to the property mailbox under
    // the synthetic path "<path>[<index>]/<sub_path>" (surfaced through
    // property_changed / get_property like any watched value).
    void rt_list_get(int64_t p_instance_id, const godot::String& p_path,
                     int p_index, const godot::String& p_sub_path);

    // Audio-thread entry: mixes Rive's audio into interleaved stereo
    // floats. Safe once the engine exists (created lazily on the render
    // thread; guarded by an atomic pointer swap).
    int mix_audio(float* p_buffer, int p_frames);
    // Per-instance mix (per-node bus routing, G5.3): pulls the dedicated
    // engine created when the instance was initialized with dedicated
    // audio. Audio-thread safe (short map lock; engine internally locked).
    int mix_audio_instance(int64_t p_instance_id, float* p_buffer,
                           int p_frames);

    // Blocking render for editor thumbnails: safe to call from any
    // non-render thread (the editor's preview thread); posts a one-shot
    // self-contained render (own File import, no Instance) and waits.
    godot::Ref<godot::Image> render_thumbnail(
        const godot::PackedByteArray& p_data, const godot::Vector2i& p_size);
    void rt_render_thumbnail(const godot::PackedByteArray& p_data,
                             const godot::Vector2i& p_size);

    void ensure_frame_hook(); // main/preview thread

protected:
    static void _bind_methods();

private:
    struct Instance;

    void on_frame_pre_draw(); // main thread: posts rt_flush_all
    void rt_render_instance(int64_t p_instance_id, Instance* p_instance);
    void rt_drain_reported_events(int64_t p_instance_id, Instance* p_instance);
    void rt_drain_semantics(int64_t p_instance_id, Instance* p_instance);
    bool rt_create_target(int64_t p_instance_id, Instance* p_instance,
                          bool p_has_bridge);

    bool rt_ensure_bridge();

    static RiveRenderServer* singleton;

    std::unique_ptr<render::RenderBridge> bridge; // render thread only

    // CommandQueue migration (docs/commandqueue-migration.md). M0: the
    // queue/server exist and are pumped each frame; instance state still
    // flows through the legacy rt_* path.
    rive::rcp<rive::CommandQueue>* command_queue_storage = nullptr; // main
    std::unique_ptr<rive::CommandServer> command_server; // render thread
    godot::HashMap<int64_t, Instance*> instances; // render thread only
    bool bridge_failed = false;                   // render thread only
    bool frame_hook_connected = false;            // main thread only
    // True when frame_pre_draw actually fires (not headless): pumps defer
    // to one flush per drawn frame. Main thread only.
    bool pump_deferred = false;

    std::mutex mailbox_mutex;
    // Serializes the render-thread pump (rt_flush_all: every rive mutation
    // funnels through it post-M1) against synchronous main-thread reads
    // (hit_test). Coarse by design — reads are rare (opt-in hit testing).
    std::mutex flush_mutex;
    godot::HashMap<int64_t, godot::Array> semantics_mailbox;
    // Textures swapped out by rt_resize_texture, freed one flush later so
    // in-flight scene draws never sample a freed RID (render thread only).
    struct RetiredRids {
        godot::RID rd_texture;
        godot::RID rs_texture;
    };
    godot::LocalVector<RetiredRids> retired_rids;
    godot::HashMap<int64_t, godot::RID> texture_mailbox;
    godot::HashMap<int64_t, godot::RID> canvas_texture_mailbox;
    godot::HashMap<int64_t, godot::Array> event_mailbox;
    godot::HashMap<int64_t, godot::Array> state_mailbox;
    godot::HashMap<int64_t, godot::Array> property_mailbox;

    std::atomic<int64_t> next_instance_id{1};
    std::atomic<bool> pump_pending{false};

    // Created on the render thread at first instance init; read from the
    // audio thread via mix_audio. rive::AudioEngine is internally locked.
    std::atomic<rive::AudioEngine*> audio_engine_raw{nullptr};
    // Dedicated per-instance engines (rcp keeps them alive across the
    // audio thread's short lock).
    std::mutex audio_mutex;
    godot::HashMap<int64_t, rive::rcp<rive::AudioEngine>> instance_engines;

    std::mutex thumbnail_mutex;
    std::condition_variable thumbnail_done;
    godot::Ref<godot::Image> thumbnail_result;
    bool thumbnail_ready = false;
};

} // namespace rivegd

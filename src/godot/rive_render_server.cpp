#include "godot/rive_render_server.h"

#include "render/gl/gl_bridge.hpp"
#include "render/render_bridge.hpp"
#ifdef RIVE_VULKAN
#include "render/vulkan/vulkan_bridge.hpp"
#endif

#include "rive/artboard.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_color_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_enum_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_artboard_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_asset_image_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_list_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_trigger_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "utils/no_op_factory.hpp"

#include <godot_cpp/templates/local_vector.hpp>

#include <chrono>
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/event.hpp"
#include "rive/event_report.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/semantic/semantic_manager.hpp"
#include "rive/semantic/semantic_snapshot.hpp"
#include "rive/file.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer/rive_renderer.hpp"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rivegd {

RiveRenderServer* RiveRenderServer::singleton = nullptr;

// Keeps the engine rcp alive for the server's lifetime.
static rive::rcp<rive::AudioEngine>& audio_engine_holder() {
    static rive::rcp<rive::AudioEngine> engine;
    return engine;
}

struct RiveRenderServer::Instance {
    // Owned by the CommandServer; resolved once at init from the handles
    // below and stable until the queue's delete commands run (which our
    // release path orders after rt_free_instance).
    rive::File* file = nullptr;
    rive::ArtboardInstance* artboard = nullptr;
    rive::StateMachineInstance* state_machine = nullptr;
    rive::FileHandle file_handle = RIVE_NULL_HANDLE;
    rive::ArtboardHandle artboard_handle = RIVE_NULL_HANDLE;
    rive::StateMachineHandle state_machine_handle = RIVE_NULL_HANDLE;
    rive::rcp<rive::ViewModelInstanceRuntime> view_model;
    rive::rcp<rive::gpu::RenderTarget> target;
    RID rd_texture; // RD-path only
    RID rs_texture; // RS-level texture, valid on every backend
    Vector2i size;
    int fit = 0;              // RiveRenderServer::FitMode
    float align_x = 0.0f;     // [-1, 1]
    float align_y = 0.0f;
    float layout_scale = 1.0f;
    bool valid = false;

    // Sleep bookkeeping (GOALS G4.6): when the machine reports it settled
    // and nothing external arrived, skip the GPU frame — the target keeps
    // its last contents.
    bool needs_render = true;
    bool settled = false;
    bool semantics_enabled = false;
    // A live-bound GPU image updates outside rive's knowledge: the
    // instance must re-render every frame to re-sample it (never settles).
    bool live_image_bound = false;
    // Reports already delivered from the current report generation (rive
    // clears reports on advance; between advances they accumulate).
    size_t events_delivered = 0;

    // Decoded RenderImages assigned to image properties (kept alive here;
    // keyed by path so reassignment releases the old one).
    godot::HashMap<godot::String, rive::rcp<rive::RenderImage>> bound_images;

    struct WatchedProperty {
        String path;
        rive::ViewModelInstanceValueRuntime* value = nullptr; // owned by VM
    };
    LocalVector<WatchedProperty> watched;
};

// Reads a watched property's current value as a Variant. Dispatch is by
// DataType — rive builds without RTTI, so no dynamic_cast.
static Variant read_vm_property(rive::ViewModelInstanceValueRuntime* value) {
    switch (value->dataType()) {
        case rive::DataType::boolean:
            return static_cast<rive::ViewModelInstanceBooleanRuntime*>(value)
                ->value();
        case rive::DataType::number:
            return static_cast<rive::ViewModelInstanceNumberRuntime*>(value)
                ->value();
        case rive::DataType::string:
            return String::utf8(
                static_cast<rive::ViewModelInstanceStringRuntime*>(value)
                    ->value()
                    .c_str());
        case rive::DataType::enumType:
            return String::utf8(
                static_cast<rive::ViewModelInstanceEnumRuntime*>(value)
                    ->value()
                    .c_str());
        case rive::DataType::color: {
            const int argb =
                static_cast<rive::ViewModelInstanceColorRuntime*>(value)
                    ->value();
            const uint32_t c = uint32_t(argb);
            return Color(float((c >> 16) & 0xFF) / 255.0f,
                         float((c >> 8) & 0xFF) / 255.0f,
                         float(c & 0xFF) / 255.0f,
                         float((c >> 24) & 0xFF) / 255.0f);
        }
        case rive::DataType::trigger:
            // Triggers carry no value; reported as fired.
            return true;
        case rive::DataType::list:
            // Lists report their size.
            return static_cast<int64_t>(
                static_cast<rive::ViewModelInstanceListRuntime*>(value)
                    ->size());
        default:
            return Variant();
    }
}

RiveRenderServer::RiveRenderServer() {
    command_queue_storage =
        new rive::rcp<rive::CommandQueue>(rive::make_rcp<rive::CommandQueue>());
}

RiveRenderServer::~RiveRenderServer() {
    // Quit-time sweep: instances released during teardown post rt_free to
    // the render thread, but a quitting app never services it — free the
    // GPU textures here (extension deinit, single-threaded, servers still
    // alive). Rive objects go down with the CommandServer below.
    if (RenderingServer::get_singleton() != nullptr) {
        RenderingServer* rs = RenderingServer::get_singleton();
        RenderingDevice* rd = rs->get_rendering_device();
        if (bridge != nullptr) {
            bridge->wait_idle();
        }
        for (const KeyValue<int64_t, Instance*>& entry : instances) {
            Instance* instance = entry.value;
            if (instance->rs_texture.is_valid()) {
                rs->free_rid(instance->rs_texture);
            }
            if (instance->rd_texture.is_valid() && rd != nullptr) {
                rd->free_rid(instance->rd_texture);
            }
            memdelete(instance);
        }
        instances.clear();
    }
    // Free any textures still parked in the resize graveyard (a quit right
    // after a resize would otherwise leak them past the last flush).
    if (!retired_rids.is_empty() &&
        RenderingServer::get_singleton() != nullptr) {
        RenderingServer* rs = RenderingServer::get_singleton();
        RenderingDevice* rd = rs->get_rendering_device();
        for (const RetiredRids& retired : retired_rids) {
            if (retired.rs_texture.is_valid()) {
                rs->free_rid(retired.rs_texture);
            }
            if (retired.rd_texture.is_valid() && rd != nullptr) {
                rd->free_rid(retired.rd_texture);
            }
        }
        retired_rids.clear();
    }
    // The server must die before the queue reference drops.
    command_server.reset();
    delete command_queue_storage;
}

RiveRenderServer* RiveRenderServer::get_singleton() { return singleton; }

void RiveRenderServer::create_singleton() {
    if (singleton == nullptr) {
        singleton = memnew(RiveRenderServer);
        // frame_pre_draw is connected lazily (allocate_instance_id):
        // RenderingServer does not exist yet at extension-init time.
    }
}

void RiveRenderServer::free_singleton() {
    if (singleton != nullptr) {
        if (singleton->frame_hook_connected &&
            RenderingServer::get_singleton() != nullptr) {
            RenderingServer::get_singleton()->disconnect(
                "frame_pre_draw",
                callable_mp(singleton, &RiveRenderServer::on_frame_pre_draw));
        }
        memdelete(singleton);
        singleton = nullptr;
    }
}

void RiveRenderServer::on_frame_pre_draw() {
    // One flush per rendered frame: every runOnce posted since the last
    // draw (all instances' rt_frame, inputs, writes) lands in a single
    // pump, so the render loop sees ALL dirty instances at once and the
    // chunked submit stays within the fence ring. (Scheduling a flush per
    // mutation made flush count scale with instance count: 200 animated
    // instances = ~200 single-render submissions/frame, bimodal fence
    // stalls, 134ms spikes.)
    if (pump_pending.load()) {
        RenderingServer::get_singleton()->call_on_render_thread(
            callable_mp(this, &RiveRenderServer::rt_flush_all));
    }
}

void RiveRenderServer::request_pump_now() {
    bool expected = false;
    if (pump_pending.compare_exchange_strong(expected, true)) {
        RenderingServer::get_singleton()->call_on_render_thread(
            callable_mp(this, &RiveRenderServer::rt_flush_all));
    }
}

void RiveRenderServer::request_pump() {
    if (pump_deferred) {
        // Rendering: mark and wait for frame_pre_draw to run the single
        // per-frame flush.
        pump_pending.store(true);
        return;
    }
    // Headless (frame_pre_draw never fires): pump immediately, deduped.
    bool expected = false;
    if (pump_pending.compare_exchange_strong(expected, true)) {
        RenderingServer::get_singleton()->call_on_render_thread(
            callable_mp(this, &RiveRenderServer::rt_flush_all));
    }
}

void RiveRenderServer::_bind_methods() {
    // rt_* methods are invoked through call_on_render_thread callables.
    ClassDB::bind_method(D_METHOD("rt_init_instance", "instance_id", "data",
                                  "artboard", "state_machine", "size"),
                         &RiveRenderServer::rt_init_instance);
    ClassDB::bind_method(D_METHOD("rt_frame", "instance_id", "delta"),
                         &RiveRenderServer::rt_frame);
    ClassDB::bind_method(D_METHOD("rt_free_instance", "instance_id"),
                         &RiveRenderServer::rt_free_instance);
}

void RiveRenderServer::ensure_frame_hook() {
    if (!frame_hook_connected) {
        frame_hook_connected = true;
        // Deferring pumps to frame_pre_draw only works when draws happen:
        // under --headless the signal is connected but NEVER fires, so
        // request_pump must keep the immediate path there.
        pump_deferred =
            DisplayServer::get_singleton() != nullptr &&
            DisplayServer::get_singleton()->get_name() != "headless";
        RenderingServer::get_singleton()->connect(
            "frame_pre_draw",
            callable_mp(this, &RiveRenderServer::on_frame_pre_draw));
    }
}

int64_t RiveRenderServer::allocate_instance_id() {
    ensure_frame_hook();
    return next_instance_id.fetch_add(1);
}

rive::CommandQueue* RiveRenderServer::queue() {
    return command_queue_storage->get();
}

RID RiveRenderServer::get_texture_rid(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    const RID* rid = texture_mailbox.getptr(p_instance_id);
    return rid != nullptr ? *rid : RID();
}

RID RiveRenderServer::get_canvas_texture_rid(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    const RID* rid = canvas_texture_mailbox.getptr(p_instance_id);
    return rid != nullptr ? *rid : RID();
}

Array RiveRenderServer::take_events(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    Array* events = event_mailbox.getptr(p_instance_id);
    if (events == nullptr || events->is_empty()) {
        return Array();
    }
    Array out = *events;
    *events = Array();
    return out;
}

Array RiveRenderServer::take_state_changes(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    Array* states = state_mailbox.getptr(p_instance_id);
    if (states == nullptr || states->is_empty()) {
        return Array();
    }
    Array out = *states;
    *states = Array();
    return out;
}

Array RiveRenderServer::take_property_changes(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    Array* changes = property_mailbox.getptr(p_instance_id);
    if (changes == nullptr || changes->is_empty()) {
        return Array();
    }
    Array out = *changes;
    *changes = Array();
    return out;
}

// Contain-fit transform shared by drawing and pointer mapping.
struct ContainFit {
    float scale, tx, ty;
};
static ContainFit contain_fit(float artboard_w, float artboard_h,
                              float target_w, float target_h) {
    const float scale = MIN(target_w / artboard_w, target_h / artboard_h);
    return {scale, (target_w - artboard_w * scale) * 0.5f,
            (target_h - artboard_h * scale) * 0.5f};
}

// Generalized artboard->texture transform (GOALS G4.x fit modes; mirrors
// rive's Fit). LAYOUT is handled upstream (the artboard itself is resized
// to the texture, so its transform is identity).
struct FitTransform {
    float sx = 1.0f, sy = 1.0f, tx = 0.0f, ty = 0.0f;
};
static FitTransform compute_fit(int fit, float align_x, float align_y,
                                float ab_w, float ab_h, float tex_w,
                                float tex_h, float layout_scale = 1.0f) {
    FitTransform out;
    switch (fit) {
        case RiveRenderServer::FIT_FILL:
            out.sx = tex_w / ab_w;
            out.sy = tex_h / ab_h;
            return out; // fills exactly; alignment moot
        case RiveRenderServer::FIT_COVER:
            out.sx = out.sy = MAX(tex_w / ab_w, tex_h / ab_h);
            break;
        case RiveRenderServer::FIT_WIDTH:
            out.sx = out.sy = tex_w / ab_w;
            break;
        case RiveRenderServer::FIT_HEIGHT:
            out.sx = out.sy = tex_h / ab_h;
            break;
        case RiveRenderServer::FIT_NONE:
            out.sx = out.sy = 1.0f;
            break;
        case RiveRenderServer::FIT_SCALE_DOWN:
            out.sx = out.sy =
                MIN(1.0f, MIN(tex_w / ab_w, tex_h / ab_h));
            break;
        case RiveRenderServer::FIT_LAYOUT:
            // Artboard tracks node_size/layout_scale upstream; computing
            // the transform from the ACTUAL sizes (rather than assuming
            // they match) keeps content mapped correctly during live
            // resizes, when the artboard has reflowed but the texture has
            // not been recreated yet.
            out.sx = tex_w / ab_w;
            out.sy = tex_h / ab_h;
            return out;
        case RiveRenderServer::FIT_CONTAIN:
        default:
            out.sx = out.sy = MIN(tex_w / ab_w, tex_h / ab_h);
            break;
    }
    // align_* in [-1, 1]: -1 = left/top, 0 = center, 1 = right/bottom.
    out.tx = (tex_w - ab_w * out.sx) * (align_x + 1.0f) * 0.5f;
    out.ty = (tex_h - ab_h * out.sy) * (align_y + 1.0f) * 0.5f;
    return out;
}

bool RiveRenderServer::rt_ensure_bridge() {
    if (bridge != nullptr) {
        return true;
    }
    if (bridge_failed) {
        return false;
    }

    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs->get_rendering_device();
    if (rd == nullptr) {
        // Compatibility renderer (or headless). GL shares Godot's context;
        // we are on the thread that owns it (render thread).
        const String driver = rs->get_current_rendering_driver_name();
        if (driver.begins_with("opengl")) {
            std::string error;
            bridge = render::GLBridge::create(&error);
            if (bridge == nullptr) {
                bridge_failed = true;
                ERR_PRINT(String("rivegd: GL bridge init failed: ") +
                          String::utf8(error.c_str()));
                return false;
            }
            return true;
        }
        bridge_failed = true;
        // Under --headless the display server is "headless" and the driver
        // name still reflects the project setting — don't warn about that.
        if (DisplayServer::get_singleton() != nullptr &&
            DisplayServer::get_singleton()->get_name() == "headless") {
            print_verbose("rivegd: headless — running logic-only.");
        } else {
            ERR_PRINT("rivegd: no RenderingDevice and driver '" + driver +
                      "' is unsupported; Rive runs logic-only.");
        }
        return false;
    }
#ifdef RIVE_VULKAN
    render::GodotVulkanHandles handles;
    handles.instance = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_TOPMOST_OBJECT, RID(), 0);
    handles.physical_device = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, RID(), 0);
    handles.device = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    handles.queue = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
    handles.queue_family = static_cast<uint32_t>(rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_QUEUE_FAMILY, RID(), 0));

    std::string error;
    bridge = render::VulkanBridge::create(handles, &error);
    if (bridge == nullptr) {
        bridge_failed = true;
        ERR_PRINT(String("rivegd: Vulkan bridge init failed: ") +
                  String::utf8(error.c_str()));
        return false;
    }
    return true;
#else
    // No Vulkan on this platform (web): an RD device is unexpected.
    bridge_failed = true;
    ERR_PRINT("rivegd: RenderingDevice present but Vulkan not compiled in");
    return false;
#endif
}

void RiveRenderServer::rt_init_instance(int64_t p_instance_id,
                                        uint64_t p_file_handle,
                                        uint64_t p_artboard_handle,
                                        uint64_t p_state_machine_handle,
                                        const Vector2i& p_size, int p_fit,
                                        const Vector2& p_alignment,
                                        bool p_dedicated_audio,
                                        float p_layout_scale) {
    // The queue's loadFile/instantiate* commands ran earlier in this pump
    // (FIFO); the CommandServer owns the objects — we resolve and hold raw
    // pointers, releasing via queue deletes ordered after rt_free_instance.
    const bool has_bridge = rt_ensure_bridge();

    Instance* instance = memnew(Instance);
    instance->size = Vector2i(MAX(1, p_size.x), MAX(1, p_size.y));
    instance->fit = p_fit;
    instance->align_x = p_alignment.x;
    instance->align_y = p_alignment.y;
    instance->layout_scale = MAX(0.01f, p_layout_scale);
    instance->file_handle = reinterpret_cast<rive::FileHandle>(p_file_handle);
    instance->artboard_handle =
        reinterpret_cast<rive::ArtboardHandle>(p_artboard_handle);
    instance->state_machine_handle =
        reinterpret_cast<rive::StateMachineHandle>(p_state_machine_handle);

    instance->file = command_server->getFile(instance->file_handle);
    instance->artboard =
        command_server->getArtboardInstance(instance->artboard_handle);
    instance->state_machine = command_server->getStateMachineInstance(
        instance->state_machine_handle);
    if (instance->file == nullptr || instance->artboard == nullptr) {
        ERR_PRINT("rivegd: file/artboard failed to load (bad data or name)");
        memdelete(instance);
        return;
    }

    // FIT_LAYOUT: the artboard itself resizes to the texture, so its Rive
    // layout (Yoga) reflows — same mechanism as the queue's setArtboardSize.
    if (instance->fit == FIT_LAYOUT) {
        instance->artboard->width(float(instance->size.x) /
                                  instance->layout_scale);
        instance->artboard->height(float(instance->size.y) /
                                   instance->layout_scale);
    }

    // Data binding: create the instance through the ViewModelRuntime factory
    // (which wires up the property runtimes and dirty-tracking) and bind its
    // underlying ViewModelInstance. Constructing a ViewModelInstanceRuntime
    // by hand around a raw createDefaultViewModelInstance() skips that setup,
    // so writes reach get_property's cache but never re-drive the render.
    if (instance->artboard->viewModelId() != -1) { // silent when no VM at all
        rive::ViewModelRuntime* view_model_runtime =
            instance->file->defaultArtboardViewModel(instance->artboard);
        if (view_model_runtime != nullptr) {
            rive::rcp<rive::ViewModelInstanceRuntime> vmi_runtime =
                view_model_runtime->createDefaultInstance();
            if (vmi_runtime == nullptr) {
                vmi_runtime = view_model_runtime->createInstance();
            }
            if (vmi_runtime != nullptr) {
                rive::rcp<rive::ViewModelInstance> vmi = vmi_runtime->instance();
                if (instance->state_machine != nullptr) {
                    instance->state_machine->bindViewModelInstance(vmi);
                } else {
                    instance->artboard->bindViewModelInstance(vmi);
                }
                instance->view_model = vmi_runtime;
            }
        }
    }

    // Create the shared texture and hand its native handle(s) to Rive.
    if (!rt_create_target(p_instance_id, instance, has_bridge)) {
        memdelete(instance);
        return;
    }

    // Route this artboard's audio: a dedicated engine when requested
    // (per-node bus routing via RiveAudioStream.instance_id), otherwise
    // the shared external engine (global RiveAudioStream mix).
    if (p_dedicated_audio) {
        rive::rcp<rive::AudioEngine> engine = rive::AudioEngine::Make(
            2, uint32_t(AudioServer::get_singleton()->get_mix_rate()));
        instance->artboard->audioEngine(engine);
        std::lock_guard<std::mutex> audio_lock(audio_mutex);
        instance_engines[p_instance_id] = engine;
    } else {
        if (audio_engine_raw.load() == nullptr) {
            rive::rcp<rive::AudioEngine> engine = rive::AudioEngine::Make(
                2, uint32_t(AudioServer::get_singleton()->get_mix_rate()));
            audio_engine_holder() = engine;
            audio_engine_raw.store(engine.get());
        }
        instance->artboard->audioEngine(audio_engine_holder());
    }

    instance->valid = true;
    instances[p_instance_id] = instance;

    {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        texture_mailbox[p_instance_id] = instance->rd_texture;
        canvas_texture_mailbox[p_instance_id] = instance->rs_texture;
    }
}

// Allocates the instance's GPU texture + render target at instance->size
// and posts the fresh RIDs to the texture mailboxes. Reused by resize
// (rt_resize_texture) — RD path: RD texture (VkImage/view) surfaced via an
// RS wrapper (texture_rd_create); GL path: an RS texture rendered into
// directly. Logic-only instances get no texture.
bool RiveRenderServer::rt_create_target(int64_t p_instance_id,
                                        Instance* instance, bool has_bridge) {
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = has_bridge ? rs->get_rendering_device() : nullptr;
    uint64_t native_a = 0;
    uint64_t native_b = 0;
    if (!has_bridge) {
        // Logic-only: no texture, no render target.
    } else if (rd != nullptr) {
        Ref<RDTextureFormat> format;
        format.instantiate();
        format->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
        format->set_width(instance->size.x);
        format->set_height(instance->size.y);
        format->set_usage_bits(
            RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
            RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
            RenderingDevice::TEXTURE_USAGE_STORAGE_BIT |
            RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
            RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT);
        Ref<RDTextureView> view;
        view.instantiate();
        instance->rd_texture = rd->texture_create(format, view);
        if (!instance->rd_texture.is_valid()) {
            ERR_PRINT("rivegd: RD texture_create failed");
            return false;
        }
        instance->rs_texture = rs->texture_rd_create(instance->rd_texture);
        native_a = rd->get_driver_resource(
            RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE,
            instance->rd_texture, 0);
        native_b = rd->get_driver_resource(
            RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW,
            instance->rd_texture, 0);
    } else {
        Ref<Image> placeholder = Image::create_empty(
            instance->size.x, instance->size.y, false, Image::FORMAT_RGBA8);
        instance->rs_texture = rs->texture_2d_create(placeholder);
        if (!instance->rs_texture.is_valid()) {
            ERR_PRINT("rivegd: RS texture_2d_create failed");
            return false;
        }
        native_a = rs->texture_get_native_handle(instance->rs_texture);
    }
    if (has_bridge) {
        instance->target = bridge->wrap_render_target(
            instance->size.x, instance->size.y, native_a, native_b);
    }
    {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        texture_mailbox[p_instance_id] = instance->rd_texture;
        canvas_texture_mailbox[p_instance_id] = instance->rs_texture;
    }
    return true;
}

// Live artboard reflow during a resize (FIT_LAYOUT): cheap, preserves all
// state; the texture is recreated separately once the size settles.
void RiveRenderServer::rt_resize_artboard(int64_t p_instance_id,
                                          const Vector2& p_logical_size) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || !(*found)->valid) {
        return;
    }
    Instance* instance = *found;
    if (instance->fit != FIT_LAYOUT || instance->artboard == nullptr) {
        return;
    }
    instance->artboard->width(
        MAX(1.0f, p_logical_size.x / instance->layout_scale));
    instance->artboard->height(
        MAX(1.0f, p_logical_size.y / instance->layout_scale));
    instance->settled = false;
    instance->needs_render = true;
}

// Texture-only resize: swaps the GPU target without touching the file,
// artboard, state machine, or view model — SM state survives (unlike the
// historical full instance recreation). Old RIDs are retired at the START
// of a later flush so in-flight scene draws never sample a freed texture.
void RiveRenderServer::rt_resize_texture(int64_t p_instance_id,
                                         const Vector2i& p_size) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || !(*found)->valid) {
        return;
    }
    Instance* instance = *found;
    const Vector2i size = Vector2i(MAX(1, p_size.x), MAX(1, p_size.y));
    if (size == instance->size && instance->target != nullptr) {
        return;
    }
    instance->size = size;
    if (bridge == nullptr) {
        return; // logic-only: size is bookkeeping only
    }
    // Retire (do not free yet) the old resources.
    retired_rids.push_back({instance->rd_texture, instance->rs_texture});
    instance->rd_texture = RID();
    instance->rs_texture = RID();
    instance->target = nullptr;
    if (!rt_create_target(p_instance_id, instance, true)) {
        instance->valid = false;
        return;
    }
    if (instance->fit == FIT_LAYOUT && instance->artboard != nullptr) {
        instance->artboard->width(float(size.x) / instance->layout_scale);
        instance->artboard->height(float(size.y) / instance->layout_scale);
    }
    instance->settled = false;
    instance->needs_render = true;
}

void RiveRenderServer::rt_drain_reported_events(int64_t p_instance_id,
                                                Instance* instance) {
    const size_t event_count = instance->state_machine->reportedEventCount();
    if (event_count <= instance->events_delivered) {
        return;
    }
    Array batch;
    for (size_t i = instance->events_delivered; i < event_count; ++i) {
        const rive::EventReport report =
            instance->state_machine->reportedEventAt(i);
        rive::Event* event = report.event();
        if (event == nullptr) {
            continue;
        }
        Dictionary entry;
        entry["name"] = String::utf8(event->name().c_str());
        entry["seconds_delay"] = report.secondsDelay();
        Dictionary properties;
        for (rive::Component* child : event->children()) {
            if (auto* b = child->as<rive::CustomPropertyBoolean>()) {
                properties[String::utf8(b->name().c_str())] =
                    b->propertyValue();
            } else if (auto* n = child->as<rive::CustomPropertyNumber>()) {
                properties[String::utf8(n->name().c_str())] =
                    n->propertyValue();
            } else if (auto* s = child->as<rive::CustomPropertyString>()) {
                properties[String::utf8(s->name().c_str())] =
                    String::utf8(s->propertyValue().c_str());
            }
        }
        entry["properties"] = properties;
        batch.push_back(entry);
    }
    instance->events_delivered = event_count;
    if (!batch.is_empty()) {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        Array& events = event_mailbox[p_instance_id];
        events.append_array(batch);
    }
}

void RiveRenderServer::rt_frame(int64_t p_instance_id, double p_delta) {
    Instance** found = instances.getptr(p_instance_id);
    // No bridge is fine: logic-only instances still advance (G2.4);
    // rendering is guarded separately in rt_flush_all.
    if (found == nullptr || !(*found)->valid) {
        return;
    }
    Instance* instance = *found;

    if (instance->state_machine != nullptr) {
        // Pointer-fired events live on the instance only until the next
        // advance clears them — drain BEFORE advancing (rt_pointer also
        // drains immediately, so this is a safety net for reports raised
        // between frames by other paths).
        rt_drain_reported_events(p_instance_id, instance);

        const bool advancing =
            instance->state_machine->advanceAndApply(static_cast<float>(p_delta));
        instance->events_delivered = 0; // advance started a new generation
        if (advancing) {
            instance->settled = false;
            instance->needs_render = true;
        } else if (instance->live_image_bound) {
            // Live-bound image: its contents change externally every
            // frame — keep rendering so rive re-samples it.
            instance->needs_render = true;
        } else if (!instance->settled) {
            // Render one final frame in the settled pose, then sleep.
            instance->settled = true;
            instance->needs_render = true;
        }

        // Timeline events raised during this advance.
        rt_drain_reported_events(p_instance_id, instance);
        rt_drain_semantics(p_instance_id, instance);

        // State transitions -> state_changed signal (animation states carry
        // the meaningful names; entry/exit/any states are skipped).
        const size_t state_count = instance->state_machine->stateChangedCount();
        if (state_count > 0) {
            Array names;
            for (size_t i = 0; i < state_count; ++i) {
                const rive::LayerState* state =
                    instance->state_machine->stateChangedByIndex(i);
                if (state != nullptr && state->is<rive::AnimationState>()) {
                    const auto* animation_state =
                        state->as<rive::AnimationState>();
                    if (animation_state->animation() != nullptr) {
                        names.push_back(String::utf8(
                            animation_state->animation()->name().c_str()));
                    }
                }
            }
            if (!names.is_empty()) {
                std::lock_guard<std::mutex> lock(mailbox_mutex);
                Array& states = state_mailbox[p_instance_id];
                states.append_array(names);
            }
        }
    } else {
        instance->needs_render =
            instance->artboard->advance(static_cast<float>(p_delta)) ||
            !instance->settled;
        instance->settled = !instance->needs_render;
    }

    // Watched view-model properties -> property_changed signal.
    if (!instance->watched.is_empty()) {
        Array changes;
        for (const Instance::WatchedProperty& watched : instance->watched) {
            if (watched.value->flushChanges()) {
                Dictionary change;
                change["path"] = watched.path;
                change["value"] = read_vm_property(watched.value);
                changes.push_back(change);
            }
        }
        if (!changes.is_empty()) {
            std::lock_guard<std::mutex> lock(mailbox_mutex);
            Array& mailbox = property_mailbox[p_instance_id];
            mailbox.append_array(changes);
        }
    }
    // Rendering happens in rt_flush_all — one batch for all instances.
}

void RiveRenderServer::rt_render_instance(int64_t p_instance_id,
                                          Instance* instance) {
    bridge->begin_frame(instance->size.x, instance->size.y, 0x00000000);

    rive::RiveRenderer renderer(bridge->render_context());
    renderer.save();
    const FitTransform fit = compute_fit(
        instance->fit, instance->align_x, instance->align_y,
        instance->artboard->width(), instance->artboard->height(),
        float(instance->size.x), float(instance->size.y),
        instance->layout_scale);
    renderer.transform(rive::Mat2D(fit.sx, 0, 0, fit.sy, fit.tx, fit.ty));
    instance->artboard->draw(&renderer);
    renderer.restore();

    std::string error;
    if (!bridge->flush_target(instance->target.get(), &error)) {
        ERR_PRINT(String("rivegd: flush failed: ") + String::utf8(error.c_str()));
        instance->valid = false;
    }
}

void RiveRenderServer::rt_flush_all() {
    std::lock_guard<std::mutex> flush_lock(flush_mutex);
    pump_pending.store(false);
    // Pump the CommandServer. Its Factory is fixed at construction and
    // files import through it — the GPU bridge MUST be resolved first, or
    // everything silently imports with no-op render paths and draws
    // nothing. NoOpFactory is only correct when no bridge can ever exist
    // (headless), which is exactly what rt_ensure_bridge determines.
    if (command_server == nullptr) {
        rt_ensure_bridge();
        static rive::NoOpFactory pump_factory;
        rive::Factory* factory =
            bridge != nullptr ? bridge->factory() : &pump_factory;
        command_server = std::make_unique<rive::CommandServer>(
            *command_queue_storage, factory);
    }
    // Free textures retired by rt_resize_texture at least one flush ago —
    // the scene has re-drawn with the replacement RIDs by now.
    if (!retired_rids.is_empty()) {
        RenderingServer* rs = RenderingServer::get_singleton();
        RenderingDevice* rd = rs->get_rendering_device();
        for (const RetiredRids& retired : retired_rids) {
            if (retired.rs_texture.is_valid()) {
                rs->free_rid(retired.rs_texture);
            }
            if (retired.rd_texture.is_valid() && rd != nullptr) {
                rd->free_rid(retired.rd_texture);
            }
        }
        retired_rids.clear();
    }

    command_server->processCommands();

    if (bridge == nullptr || instances.is_empty()) {
        return;
    }
    bool any = false;
    for (const KeyValue<int64_t, Instance*>& entry : instances) {
        if (entry.value->valid && entry.value->needs_render) {
            any = true;
            break;
        }
    }
    if (!any) {
        return; // everything is sleeping
    }

    // Submitting in chunks keeps the GPU fed while the CPU records the
    // next chunk (a single giant batch measurably regresses frame time on
    // fast GPUs; per-instance submits waste submissions on slow ones).
    // The chunk size ADAPTS to the dirty count: total submissions must fit
    // the bridge's fence ring (16 slots) or the CPU stalls on fences.
    // Measured at 500 animated 128px artboards: K=4 -> 32.2ms avg with
    // 134ms p95 fence-thrash spikes; K=32 -> 21.1ms with p95 22ms.
    static int forced_chunk_size = [] {
        const String env = OS::get_singleton()->get_environment(
            "RIVEGD_BATCH_SIZE");
        const int parsed = env.to_int();
        return parsed > 0 ? parsed : 0;
    }();
    int dirty = 0;
    for (const KeyValue<int64_t, Instance*>& entry : instances) {
        if (entry.value->valid && entry.value->needs_render &&
            entry.value->target != nullptr) {
            dirty++;
        }
    }
    const int chunk_size =
        forced_chunk_size > 0
            ? forced_chunk_size
            : CLAMP((dirty + 14) / 15, 4, 64); // <=15 submissions + slack

    std::string error;
    int in_chunk = 0;
    if (!bridge->begin_batch(&error)) {
        ERR_PRINT(String("rivegd: begin_batch failed: ") +
                  String::utf8(error.c_str()));
        return;
    }
    for (const KeyValue<int64_t, Instance*>& entry : instances) {
        Instance* instance = entry.value;
        if (!instance->valid || !instance->needs_render ||
            instance->target == nullptr) {
            continue;
        }
        instance->needs_render = false;
        rt_render_instance(entry.key, instance);
        if (++in_chunk >= chunk_size) {
            in_chunk = 0;
            if (!bridge->end_batch(&error) || !bridge->begin_batch(&error)) {
                ERR_PRINT(String("rivegd: batch split failed: ") +
                          String::utf8(error.c_str()));
                return;
            }
        }
    }
    if (!bridge->end_batch(&error)) {
        ERR_PRINT(String("rivegd: end_batch failed: ") +
                  String::utf8(error.c_str()));
    }
}

void RiveRenderServer::rt_set_bool(int64_t p_instance_id,
                                   const String& p_name, bool p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::SMIBool* input =
        (*found)->state_machine->getBool(p_name.utf8().get_data());
    if (input != nullptr) {
        input->value(p_value);
    }
}

void RiveRenderServer::rt_set_number(int64_t p_instance_id,
                                     const String& p_name, double p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::SMINumber* input =
        (*found)->state_machine->getNumber(p_name.utf8().get_data());
    if (input != nullptr) {
        input->value(static_cast<float>(p_value));
    }
}

void RiveRenderServer::rt_fire_trigger(int64_t p_instance_id,
                                       const String& p_name) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::SMITrigger* input =
        (*found)->state_machine->getTrigger(p_name.utf8().get_data());
    if (input != nullptr) {
        input->fire();
    }
}

void RiveRenderServer::rt_pointer(int64_t p_instance_id, int p_phase,
                                  const Vector2& p_local,
                                  const Vector2& p_node_size,
                                  int p_pointer_id, float p_timestamp) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    Instance* instance = *found;

    // Node-local pixels -> texture pixels -> artboard coordinates (inverse
    // of the contain-fit draw transform).
    const float texture_x =
        p_local.x * (float(instance->size.x) / MAX(1.0f, p_node_size.x));
    const float texture_y =
        p_local.y * (float(instance->size.y) / MAX(1.0f, p_node_size.y));
    const FitTransform fit = compute_fit(
        instance->fit, instance->align_x, instance->align_y,
        instance->artboard->width(), instance->artboard->height(),
        float(instance->size.x), float(instance->size.y),
        instance->layout_scale);
    const rive::Vec2D position((texture_x - fit.tx) / fit.sx,
                               (texture_y - fit.ty) / fit.sy);

    switch (p_phase) {
        case POINTER_MOVE:
            // Real timestamps: scroll/drag physics integrates velocity
            // from move-event time deltas — a constant 0 yields no motion.
            instance->state_machine->pointerMove(position, p_timestamp,
                                                 p_pointer_id);
            break;
        case POINTER_DOWN:
            instance->state_machine->pointerDown(position, p_pointer_id);
            break;
        case POINTER_UP:
            instance->state_machine->pointerUp(position, p_pointer_id);
            break;
        case POINTER_EXIT:
            instance->state_machine->pointerExit(position, p_pointer_id);
            break;
    }
    // Listener-fired events are reported immediately and would be cleared
    // by the next advance before rt_frame's drain saw them.
    rt_drain_reported_events(p_instance_id, instance);
}

void RiveRenderServer::rt_set_vm_bool(int64_t p_instance_id,
                                      const String& p_path, bool p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    auto* property =
        (*found)->view_model->propertyBoolean(p_path.utf8().get_data());
    if (property != nullptr) {
        property->value(p_value);
    }
}

void RiveRenderServer::rt_set_vm_number(int64_t p_instance_id,
                                        const String& p_path, double p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    auto* property =
        (*found)->view_model->propertyNumber(p_path.utf8().get_data());
    if (property != nullptr) {
        property->value(static_cast<float>(p_value));
    }
}

void RiveRenderServer::rt_set_vm_string(int64_t p_instance_id,
                                        const String& p_path,
                                        const String& p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    const std::string path = p_path.utf8().get_data();
    if (auto* str = (*found)->view_model->propertyString(path)) {
        str->value(p_value.utf8().get_data());
        return;
    }
    // Enums are string-valued from the Godot side.
    if (auto* en = (*found)->view_model->propertyEnum(path)) {
        en->value(p_value.utf8().get_data());
    }
}

void RiveRenderServer::rt_set_vm_color(int64_t p_instance_id,
                                       const String& p_path,
                                       const Color& p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    auto* property =
        (*found)->view_model->propertyColor(p_path.utf8().get_data());
    if (property != nullptr) {
        property->argb(int(p_value.a * 255.0f), int(p_value.r * 255.0f),
                       int(p_value.g * 255.0f), int(p_value.b * 255.0f));
    }
}

void RiveRenderServer::rt_fire_vm_trigger(int64_t p_instance_id,
                                          const String& p_path) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    auto* property =
        (*found)->view_model->propertyTrigger(p_path.utf8().get_data());
    if (property != nullptr) {
        property->trigger();
    }
}

// Sets a scalar property on an arbitrary VM instance, dispatched by the
// Variant's type (shared by top-level and list-item writes).
static void set_vm_value(rive::ViewModelInstanceRuntime* vm,
                         const String& p_path, const Variant& p_value) {
    if (vm == nullptr) {
        return;
    }
    const std::string path = p_path.utf8().get_data();
    switch (p_value.get_type()) {
        case Variant::BOOL:
            if (auto* b = vm->propertyBoolean(path)) {
                b->value(bool(p_value));
            }
            break;
        case Variant::INT:
        case Variant::FLOAT:
            if (auto* n = vm->propertyNumber(path)) {
                n->value(float(double(p_value)));
            }
            break;
        case Variant::STRING:
        case Variant::STRING_NAME:
            if (auto* str = vm->propertyString(path)) {
                str->value(String(p_value).utf8().get_data());
            } else if (auto* e = vm->propertyEnum(path)) {
                e->value(String(p_value).utf8().get_data());
            }
            break;
        case Variant::COLOR: {
            const Color color = p_value;
            if (auto* c = vm->propertyColor(path)) {
                c->argb(int(color.a * 255.0f), int(color.r * 255.0f),
                        int(color.g * 255.0f), int(color.b * 255.0f));
            }
        } break;
        default:
            break;
    }
}

// Reads a scalar property off an arbitrary VM instance as a Variant.
static Variant read_vm_value(rive::ViewModelInstanceRuntime* vm,
                             const String& p_sub_path) {
    if (vm == nullptr) {
        return Variant();
    }
    const std::string path = p_sub_path.utf8().get_data();
    if (auto* v = vm->propertyBoolean(path)) return read_vm_property(v);
    if (auto* v = vm->propertyNumber(path)) return read_vm_property(v);
    if (auto* v = vm->propertyString(path)) return read_vm_property(v);
    if (auto* v = vm->propertyEnum(path)) return read_vm_property(v);
    return Variant();
}

static rive::ViewModelInstanceListRuntime* resolve_list(
    RiveRenderServer* /*unused*/, rive::ViewModelInstanceRuntime* vm,
    const String& p_path) {
    return vm != nullptr ? vm->propertyList(p_path.utf8().get_data()) : nullptr;
}

void RiveRenderServer::rt_watch_vm_property(int64_t p_instance_id,
                                            const String& p_path) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    if ((*found)->view_model == nullptr) {
        ERR_PRINT("rivegd: cannot watch '" + p_path +
                  "': this artboard has no view model bound");
        return;
    }
    Instance* instance = *found;
    for (const Instance::WatchedProperty& watched : instance->watched) {
        if (watched.path == p_path) {
            return; // already watched
        }
    }

    const std::string path = p_path.utf8().get_data();
    rive::ViewModelInstanceValueRuntime* value =
        instance->view_model->propertyBoolean(path);
    if (value == nullptr) {
        value = instance->view_model->propertyNumber(path);
    }
    if (value == nullptr) {
        value = instance->view_model->propertyString(path);
    }
    if (value == nullptr) {
        value = instance->view_model->propertyEnum(path);
    }
    if (value == nullptr) {
        value = instance->view_model->propertyColor(path);
    }
    if (value == nullptr) {
        value = instance->view_model->propertyList(path);
    }
    bool is_trigger = false;
    if (value == nullptr) {
        value = instance->view_model->propertyTrigger(path);
        is_trigger = value != nullptr;
    }
    if (value == nullptr) {
        ERR_PRINT("rivegd: cannot watch view-model property '" + p_path +
                  "' (not found or unsupported type)");
        return;
    }
    instance->watched.push_back({p_path, value});

    if (is_trigger) {
        // Triggers have no baseline value; they only report when fired.
        value->clearChanges();
        return;
    }
    // Report the current value immediately so get_property has a baseline.
    Dictionary change;
    change["path"] = p_path;
    change["value"] = read_vm_property(value);
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    property_mailbox[p_instance_id].push_back(change);
}

void RiveRenderServer::rt_key(int64_t p_instance_id, int p_rive_key,
                              int p_modifiers, bool p_pressed,
                              bool p_repeat) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::FocusManager* focus = (*found)->state_machine->focusManager();
    if (focus != nullptr) {
        focus->keyInput(static_cast<rive::Key>(p_rive_key),
                        static_cast<rive::KeyModifiers>(p_modifiers),
                        p_pressed, p_repeat);
    }
}

void RiveRenderServer::rt_text_input(int64_t p_instance_id,
                                     const String& p_text) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::FocusManager* focus = (*found)->state_machine->focusManager();
    if (focus != nullptr) {
        focus->textInput(p_text.utf8().get_data());
    }
}

void RiveRenderServer::rt_focus_move(int64_t p_instance_id, int p_direction) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    rive::StateMachineInstance* sm = (*found)->state_machine;
    rive::FocusManager* focus = sm->focusManager();
    switch (p_direction) {
        case 0: sm->focusNext(); break;
        case 1: sm->focusPrevious(); break;
        case 2: if (focus) focus->focusLeft(); break;
        case 3: if (focus) focus->focusRight(); break;
        case 4: if (focus) focus->focusUp(); break;
        case 5: if (focus) focus->focusDown(); break;
    }
}

void RiveRenderServer::rt_gamepads(int64_t p_instance_id,
                                   const PackedByteArray& p_batch) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->state_machine == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    if (!(*found)->state_machine->submitGamepadsFromBuffer(p_batch.ptr(),
                                                           p_batch.size())) {
        ERR_PRINT("rivegd: malformed gamepad batch rejected");
    }
}

void RiveRenderServer::rt_set_vm_image_live(int64_t p_instance_id,
                                            const String& p_path,
                                            const RID& p_rs_texture) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr ||
        bridge == nullptr) {
        return;
    }
    Instance* instance = *found;
    auto* property =
        instance->view_model->propertyImage(p_path.utf8().get_data());
    if (property == nullptr) {
        ERR_PRINT("rivegd: image property not found: " + p_path);
        return;
    }
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs->get_rendering_device();
    const RID rd_texture =
        rd != nullptr ? rs->texture_get_rd_texture(p_rs_texture) : RID();
    if (!rd_texture.is_valid()) {
        ERR_PRINT("rivegd: live image binding needs an RD-backed texture "
                  "(Vulkan renderers); use an Image for '" + p_path + "'");
        return;
    }
    Ref<RDTextureFormat> format = rd->texture_get_format(rd_texture);
    // Godot DataFormat -> VkFormat (values are Vulkan-ABI stable).
    uint32_t vk_format = 0;
    switch (format->get_format()) {
        case RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM:
            vk_format = 37; // VK_FORMAT_R8G8B8A8_UNORM
            break;
        case RenderingDevice::DATA_FORMAT_R8G8B8A8_SRGB:
            vk_format = 43; // VK_FORMAT_R8G8B8A8_SRGB
            break;
        case RenderingDevice::DATA_FORMAT_B8G8R8A8_UNORM:
            vk_format = 44; // VK_FORMAT_B8G8R8A8_UNORM
            break;
        case RenderingDevice::DATA_FORMAT_B8G8R8A8_SRGB:
            vk_format = 50; // VK_FORMAT_B8G8R8A8_SRGB
            break;
        case RenderingDevice::DATA_FORMAT_R16G16B16A16_SFLOAT:
            vk_format = 97; // VK_FORMAT_R16G16B16A16_SFLOAT
            break;
        default:
            ERR_PRINT(vformat(
                "rivegd: unsupported texture format %d for live image '%s'",
                int(format->get_format()), p_path));
            return;
    }
    const uint64_t vk_image = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, rd_texture, 0);
    rive::rcp<rive::RenderImage> image = bridge->adopt_texture(
        vk_image, format->get_width(), format->get_height(), vk_format);
    if (image == nullptr) {
        ERR_PRINT("rivegd: adopt_texture failed for '" + p_path + "'");
        return;
    }
    property->value(image.get());
    instance->bound_images[p_path] = image;
    instance->live_image_bound = true;
    instance->settled = false;
    instance->needs_render = true;
}

void RiveRenderServer::rt_set_vm_image(int64_t p_instance_id,
                                       const String& p_path,
                                       const PackedByteArray& p_png_bytes) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr ||
        bridge == nullptr) {
        return;
    }
    Instance* instance = *found;
    instance->settled = false;
    instance->needs_render = true;
    auto* property =
        instance->view_model->propertyImage(p_path.utf8().get_data());
    if (property == nullptr) {
        ERR_PRINT("rivegd: image property not found: " + p_path);
        return;
    }
    rive::rcp<rive::RenderImage> image = bridge->factory()->decodeImage(
        rive::Span<const uint8_t>(p_png_bytes.ptr(), p_png_bytes.size()));
    if (image == nullptr) {
        ERR_PRINT("rivegd: could not decode image for '" + p_path + "'");
        return;
    }
    property->value(image.get());
    instance->bound_images[p_path] = image;
}

void RiveRenderServer::rt_set_vm_artboard(int64_t p_instance_id,
                                          const String& p_path,
                                          const String& p_artboard_name) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    Instance* instance = *found;
    instance->settled = false;
    instance->needs_render = true;
    auto* property =
        instance->view_model->propertyArtboard(p_path.utf8().get_data());
    if (property == nullptr) {
        ERR_PRINT("rivegd: artboard property not found: " + p_path);
        return;
    }
    rive::rcp<rive::BindableArtboard> bindable =
        p_artboard_name.is_empty()
            ? instance->file->bindableArtboardDefault()
            : instance->file->bindableArtboardNamed(
                  p_artboard_name.utf8().get_data());
    if (bindable == nullptr) {
        ERR_PRINT("rivegd: bindable artboard not found: " + p_artboard_name);
        return;
    }
    property->value(bindable);
}

void RiveRenderServer::rt_replace_view_model(int64_t p_instance_id,
                                             const String& p_path,
                                             const String& p_view_model,
                                             const String& p_instance_name) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || (*found)->view_model == nullptr) {
        return;
    }
    Instance* instance = *found;
    instance->settled = false;
    instance->needs_render = true;
    rive::ViewModelRuntime* view_model =
        instance->file->viewModelByName(p_view_model.utf8().get_data());
    if (view_model == nullptr) {
        ERR_PRINT("rivegd: view model not found: " + p_view_model);
        return;
    }
    rive::rcp<rive::ViewModelInstanceRuntime> replacement =
        p_instance_name.is_empty()
            ? view_model->createInstance()
            : view_model->createInstanceFromName(
                  p_instance_name.utf8().get_data());
    if (replacement == nullptr ||
        !instance->view_model->replaceViewModel(p_path.utf8().get_data(),
                                                replacement.get())) {
        ERR_PRINT("rivegd: could not replace nested view model at '" + p_path +
                  "'");
    }
}

void RiveRenderServer::rt_list_append(int64_t p_instance_id,
                                      const String& p_path,
                                      const String& p_view_model,
                                      const String& p_instance_name) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    Instance* instance = *found;
    instance->settled = false;
    instance->needs_render = true;
    auto* list = resolve_list(this, instance->view_model.get(), p_path);
    if (list == nullptr) {
        ERR_PRINT("rivegd: list property not found: " + p_path);
        return;
    }
    rive::ViewModelRuntime* view_model =
        instance->file->viewModelByName(p_view_model.utf8().get_data());
    if (view_model == nullptr) {
        ERR_PRINT("rivegd: view model not found: " + p_view_model);
        return;
    }
    rive::rcp<rive::ViewModelInstanceRuntime> item =
        p_instance_name.is_empty()
            ? view_model->createInstance()
            : view_model->createInstanceFromName(
                  p_instance_name.utf8().get_data());
    if (item == nullptr) {
        ERR_PRINT("rivegd: could not create instance of " + p_view_model);
        return;
    }
    list->addInstance(item.get());
}

void RiveRenderServer::rt_list_remove_at(int64_t p_instance_id,
                                         const String& p_path, int p_index) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    if (auto* list = resolve_list(this, (*found)->view_model.get(), p_path)) {
        list->removeInstanceAt(p_index);
    }
}

void RiveRenderServer::rt_list_swap(int64_t p_instance_id,
                                    const String& p_path, int p_a, int p_b) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    if (auto* list = resolve_list(this, (*found)->view_model.get(), p_path)) {
        if (p_a >= 0 && p_b >= 0 && size_t(p_a) < list->size() &&
            size_t(p_b) < list->size()) {
            list->swap(uint32_t(p_a), uint32_t(p_b));
        }
    }
}

void RiveRenderServer::rt_list_clear(int64_t p_instance_id,
                                     const String& p_path) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    if (auto* list = resolve_list(this, (*found)->view_model.get(), p_path)) {
        list->removeAllInstances();
    }
}

void RiveRenderServer::rt_list_set(int64_t p_instance_id, const String& p_path,
                                   int p_index, const String& p_sub_path,
                                   const Variant& p_value) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    (*found)->settled = false;
    (*found)->needs_render = true;
    auto* list = resolve_list(this, (*found)->view_model.get(), p_path);
    if (list == nullptr || p_index < 0 || size_t(p_index) >= list->size()) {
        return;
    }
    rive::rcp<rive::ViewModelInstanceRuntime> item = list->instanceAt(p_index);
    set_vm_value(item.get(), p_sub_path, p_value);
}


static const char* semantic_label(const rive::SemanticsDiffNode& node) {
    return node.label.c_str();
}

void RiveRenderServer::rt_set_semantics_enabled(int64_t p_instance_id,
                                                bool p_enabled) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    (*found)->semantics_enabled = p_enabled;
    if (p_enabled && (*found)->state_machine != nullptr) {
        // Builds the semantic tree (idempotent) — the manager is null
        // until this runs.
        (*found)->state_machine->enableSemantics();
        if (auto* manager = (*found)->state_machine->semanticManager()) {
            manager->markDirty(); // full re-emit for late enablers
        }
    }
}

void RiveRenderServer::rt_drain_semantics(int64_t p_instance_id,
                                          Instance* p_instance) {
    if (!p_instance->semantics_enabled ||
        p_instance->state_machine == nullptr) {
        return;
    }
    auto* manager = p_instance->state_machine->semanticManager();
    if (manager == nullptr) {
        return;
    }
    // drainDiff rebuilds if dirty and returns the delta (empty when
    // nothing changed).
    rive::SemanticsDiff diff = manager->drainDiff();
    if (diff.empty()) {
        return;
    }
    const FitTransform fit = compute_fit(
        p_instance->fit, p_instance->align_x, p_instance->align_y,
        p_instance->artboard->width(), p_instance->artboard->height(),
        float(p_instance->size.x), float(p_instance->size.y),
        p_instance->layout_scale);
    Array changes;
    auto push_node = [&](const rive::SemanticsDiffNode& node,
                         const char* op) {
        Dictionary d;
        d["op"] = op;
        d["id"] = int64_t(node.id);
        d["parent"] = int64_t(node.parentId);
        d["role"] = int(node.role);
        d["label"] = String::utf8(semantic_label(node));
        d["value"] = String::utf8(node.value.c_str());
        d["bounds"] = Rect2(fit.sx * node.minX + fit.tx,
                            fit.sy * node.minY + fit.ty,
                            fit.sx * (node.maxX - node.minX),
                            fit.sy * (node.maxY - node.minY));
        changes.push_back(d);
    };
    for (const uint32_t removed : diff.removed) {
        Dictionary d;
        d["op"] = "removed";
        d["id"] = int64_t(removed);
        changes.push_back(d);
    }
    for (const auto& node : diff.added) {
        push_node(node, "added");
    }
    for (const auto& node : diff.moved) {
        push_node(node, "moved");
    }
    for (const auto& node : diff.updatedSemantic) {
        push_node(node, "updated");
    }
    for (const auto& update : diff.updatedGeometry) {
        Dictionary d;
        d["op"] = "geometry";
        d["id"] = int64_t(update.id);
        d["bounds"] = Rect2(fit.sx * update.minX + fit.tx,
                            fit.sy * update.minY + fit.ty,
                            fit.sx * (update.maxX - update.minX),
                            fit.sy * (update.maxY - update.minY));
        changes.push_back(d);
    }
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    Array& box = semantics_mailbox[p_instance_id];
    for (int i = 0; i < changes.size(); ++i) {
        box.push_back(changes[i]);
    }
}

Array RiveRenderServer::take_semantics(int64_t p_instance_id) {
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    Array* found = semantics_mailbox.getptr(p_instance_id);
    if (found == nullptr || found->is_empty()) {
        return Array();
    }
    Array out = *found;
    *found = Array();
    return out;
}

bool RiveRenderServer::hit_test(int64_t p_instance_id,
                                const Vector2& p_local,
                                const Vector2& p_node_size, bool p_default) {
    std::lock_guard<std::mutex> flush_lock(flush_mutex);
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || !(*found)->valid ||
        (*found)->state_machine == nullptr) {
        return p_default;
    }
    Instance* instance = *found;
    const float texture_x =
        p_local.x * (float(instance->size.x) / MAX(1.0f, p_node_size.x));
    const float texture_y =
        p_local.y * (float(instance->size.y) / MAX(1.0f, p_node_size.y));
    const FitTransform fit = compute_fit(
        instance->fit, instance->align_x, instance->align_y,
        instance->artboard->width(), instance->artboard->height(),
        float(instance->size.x), float(instance->size.y),
        instance->layout_scale);
    const rive::Vec2D position((texture_x - fit.tx) / fit.sx,
                               (texture_y - fit.ty) / fit.sy);
    return instance->state_machine->hitTest(position);
}

void RiveRenderServer::rt_list_get(int64_t p_instance_id, const String& p_path,
                                   int p_index, const String& p_sub_path) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    auto* list = resolve_list(this, (*found)->view_model.get(), p_path);
    if (list == nullptr || p_index < 0 || size_t(p_index) >= list->size()) {
        return;
    }
    rive::rcp<rive::ViewModelInstanceRuntime> item = list->instanceAt(p_index);
    Dictionary change;
    change["path"] = p_path + String("[") + itos(p_index) + "]/" + p_sub_path;
    change["value"] = read_vm_value(item.get(), p_sub_path);
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    property_mailbox[p_instance_id].push_back(change);
}

int RiveRenderServer::mix_audio_instance(int64_t p_instance_id,
                                         float* p_buffer, int p_frames) {
    rive::rcp<rive::AudioEngine> engine;
    {
        std::lock_guard<std::mutex> audio_lock(audio_mutex);
        rive::rcp<rive::AudioEngine>* found =
            instance_engines.getptr(p_instance_id);
        if (found == nullptr) {
            return 0;
        }
        engine = *found;
    }
    uint64_t frames_read = 0;
    if (!engine->readAudioFrames(p_buffer, uint64_t(p_frames),
                                 &frames_read)) {
        return 0;
    }
    return int(frames_read);
}

int RiveRenderServer::mix_audio(float* p_buffer, int p_frames) {
    rive::AudioEngine* engine = audio_engine_raw.load();
    if (engine == nullptr) {
        return 0;
    }
    uint64_t frames_read = 0;
    if (!engine->readAudioFrames(p_buffer, uint64_t(p_frames), &frames_read)) {
        return 0;
    }
    return int(frames_read);
}

Ref<Image> RiveRenderServer::render_thumbnail(const PackedByteArray& p_data,
                                              const Vector2i& p_size) {
    {
        std::lock_guard<std::mutex> lock(thumbnail_mutex);
        thumbnail_ready = false;
        thumbnail_result.unref();
    }
    ensure_frame_hook();
    queue()->runOnce([this, p_data, p_size](rive::CommandServer*) {
        rt_render_thumbnail(p_data, p_size);
    });
    request_pump();
    std::unique_lock<std::mutex> lock(thumbnail_mutex);
    if (!thumbnail_done.wait_for(lock, std::chrono::seconds(4),
                                 [this] { return thumbnail_ready; })) {
        return Ref<Image>();
    }
    return thumbnail_result;
}

void RiveRenderServer::rt_render_thumbnail(const PackedByteArray& p_data,
                                           const Vector2i& p_size) {
    Ref<Image> result;
    // Self-contained one-shot render: own File import against the context
    // factory, temporary RD texture, no Instance bookkeeping.
    do {
        if (!rt_ensure_bridge()) {
            break;
        }
        rive::ImportResult import_result;
        rive::rcp<rive::File> file = rive::File::import(
            rive::Span<const uint8_t>(p_data.ptr(), p_data.size()),
            bridge->factory(), &import_result);
        if (file == nullptr) {
            break;
        }
        std::unique_ptr<rive::ArtboardInstance> artboard =
            file->artboardDefault();
        if (artboard == nullptr) {
            break;
        }
        std::unique_ptr<rive::StateMachineInstance> machine =
            artboard->stateMachineCount() > 0 ? artboard->stateMachineAt(0)
                                              : nullptr;
        if (machine != nullptr) {
            machine->advanceAndApply(0.0f);
        } else {
            artboard->advance(0.0f);
        }

        RenderingServer* rs = RenderingServer::get_singleton();
        RenderingDevice* rd = rs->get_rendering_device();
        RID rd_texture;
        RID rs_texture;
        uint64_t native_a = 0;
        uint64_t native_b = 0;
        const Vector2i size(MAX(1, p_size.x), MAX(1, p_size.y));
        if (rd != nullptr) {
            Ref<RDTextureFormat> format;
            format.instantiate();
            format->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
            format->set_width(size.x);
            format->set_height(size.y);
            format->set_usage_bits(
                RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
                RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                RenderingDevice::TEXTURE_USAGE_STORAGE_BIT |
                RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
                RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT);
            Ref<RDTextureView> view;
            view.instantiate();
            rd_texture = rd->texture_create(format, view);
            if (!rd_texture.is_valid()) {
                break;
            }
            rs_texture = rs->texture_rd_create(rd_texture);
            native_a = rd->get_driver_resource(
                RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, rd_texture, 0);
            native_b = rd->get_driver_resource(
                RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW,
                rd_texture, 0);
        } else {
            Ref<Image> placeholder =
                Image::create_empty(size.x, size.y, false, Image::FORMAT_RGBA8);
            rs_texture = rs->texture_2d_create(placeholder);
            if (!rs_texture.is_valid()) {
                break;
            }
            native_a = rs->texture_get_native_handle(rs_texture);
        }
        rive::rcp<rive::gpu::RenderTarget> target =
            bridge->wrap_render_target(size.x, size.y, native_a, native_b);

        std::string error;
        if (bridge->begin_batch(&error)) {
            bridge->begin_frame(size.x, size.y, 0x00000000);
            rive::RiveRenderer renderer(bridge->render_context());
            renderer.save();
            const ContainFit fit =
                contain_fit(artboard->width(), artboard->height(),
                            float(size.x), float(size.y));
            renderer.transform(
                rive::Mat2D(fit.scale, 0, 0, fit.scale, fit.tx, fit.ty));
            artboard->draw(&renderer);
            renderer.restore();
            bridge->flush_target(target.get(), &error);
            bridge->end_batch(&error);
            bridge->wait_idle();
            result = rs->texture_2d_get(rs_texture);
        }
        if (rs_texture.is_valid()) {
            rs->free_rid(rs_texture);
        }
        if (rd_texture.is_valid() && rd != nullptr) {
            rd->free_rid(rd_texture);
        }
    } while (false);
    {
        std::lock_guard<std::mutex> lock(thumbnail_mutex);
        thumbnail_result = result;
        thumbnail_ready = true;
    }
    thumbnail_done.notify_all();
}

void RiveRenderServer::rt_free_instance(int64_t p_instance_id) {
    {
        std::lock_guard<std::mutex> audio_lock(audio_mutex);
        instance_engines.erase(p_instance_id);
    }
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    Instance* instance = *found;
    instances.erase(p_instance_id);
    // The rive objects themselves are CommandServer-owned; the controller
    // queues deleteStateMachine/deleteArtboard/deleteFile right after this
    // closure, so they outlive our last use below.
    {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        texture_mailbox.erase(p_instance_id);
        canvas_texture_mailbox.erase(p_instance_id);
        event_mailbox.erase(p_instance_id);
        state_mailbox.erase(p_instance_id);
        property_mailbox.erase(p_instance_id);
    }
    if (bridge != nullptr) {
        // GPU may still be reading the target from the last flush.
        bridge->wait_idle();
    }
    if (instance->rs_texture.is_valid()) {
        RenderingServer::get_singleton()->free_rid(instance->rs_texture);
    }
    if (instance->rd_texture.is_valid()) {
        RenderingDevice* rd =
            RenderingServer::get_singleton()->get_rendering_device();
        if (rd != nullptr) {
            rd->free_rid(instance->rd_texture);
        }
    }
    memdelete(instance);
}

} // namespace rivegd

#include "godot/rive_render_server.h"

#include "render/gl/gl_bridge.hpp"
#include "render/render_bridge.hpp"
#include "render/vulkan/vulkan_bridge.hpp"

#include "rive/artboard.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_boolean_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_color_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_number_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_string_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_trigger_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_value_runtime.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"

#include <godot_cpp/templates/local_vector.hpp>
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/event.hpp"
#include "rive/event_report.hpp"
#include "rive/file.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer/rive_renderer.hpp"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rivegd {

RiveRenderServer* RiveRenderServer::singleton = nullptr;

struct RiveRenderServer::Instance {
    rive::rcp<rive::File> file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    rive::rcp<rive::ViewModelInstanceRuntime> view_model;
    rive::rcp<rive::gpu::RenderTarget> target;
    RID rd_texture; // RD-path only
    RID rs_texture; // RS-level texture, valid on every backend
    Vector2i size;
    bool valid = false;

    // Sleep bookkeeping (GOALS G4.6): when the machine reports it settled
    // and nothing external arrived, skip the GPU frame — the target keeps
    // its last contents.
    bool needs_render = true;
    bool settled = false;

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
        default:
            return Variant();
    }
}

RiveRenderServer::RiveRenderServer() = default;
RiveRenderServer::~RiveRenderServer() = default;

RiveRenderServer* RiveRenderServer::get_singleton() { return singleton; }

void RiveRenderServer::create_singleton() {
    if (singleton == nullptr) {
        singleton = memnew(RiveRenderServer);
    }
}

void RiveRenderServer::free_singleton() {
    if (singleton != nullptr) {
        memdelete(singleton);
        singleton = nullptr;
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

int64_t RiveRenderServer::allocate_instance_id() {
    return next_instance_id.fetch_add(1);
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
        ERR_PRINT("rivegd: no RenderingDevice and driver '" + driver +
                  "' is unsupported; Rive rendering is unavailable.");
        return false;
    }
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
}

void RiveRenderServer::rt_init_instance(int64_t p_instance_id,
                                        const PackedByteArray& p_data,
                                        const String& p_artboard,
                                        const String& p_state_machine,
                                        const Vector2i& p_size) {
    if (!rt_ensure_bridge()) {
        return;
    }

    Instance* instance = memnew(Instance);
    instance->size = Vector2i(MAX(1, p_size.x), MAX(1, p_size.y));

    // Import against the render context's factory: the GPU resources the
    // file creates belong to this context.
    rive::ImportResult result;
    instance->file =
        rive::File::import(rive::Span<const uint8_t>(p_data.ptr(), p_data.size()),
                           bridge->factory(), &result);
    if (instance->file == nullptr) {
        ERR_PRINT("rivegd: .riv import failed on render context");
        memdelete(instance);
        return;
    }

    instance->artboard = p_artboard.is_empty()
        ? instance->file->artboardDefault()
        : instance->file->artboardNamed(p_artboard.utf8().get_data());
    if (instance->artboard == nullptr) {
        ERR_PRINT("rivegd: artboard not found: " + p_artboard);
        memdelete(instance);
        return;
    }

    if (!p_state_machine.is_empty()) {
        instance->state_machine = instance->artboard->stateMachineNamed(
            p_state_machine.utf8().get_data());
    } else if (instance->artboard->stateMachineCount() > 0) {
        instance->state_machine = instance->artboard->stateMachineAt(0);
    }

    // Data binding: bind the artboard's default view model instance when it
    // declares one; otherwise fall back to a fresh instance of the
    // artboard's view model (no-op when the artboard has no view model).
    rive::rcp<rive::ViewModelInstance> vmi =
        instance->file->createDefaultViewModelInstance(instance->artboard.get());
    if (vmi == nullptr) {
        vmi = instance->file->createViewModelInstance(instance->artboard.get());
    }
    if (vmi != nullptr) {
        if (instance->state_machine != nullptr) {
            instance->state_machine->bindViewModelInstance(vmi);
        } else {
            instance->artboard->bindViewModelInstance(vmi);
        }
        instance->view_model = rive::rcp<rive::ViewModelInstanceRuntime>(
            new rive::ViewModelInstanceRuntime(vmi));
    }

    // Create the shared texture and hand its native handle(s) to Rive.
    // RD path: RD texture (VkImage/view), surfaced to the scene through an
    // RS wrapper (texture_rd_create). GL path: an RS texture whose GL id we
    // render into directly.
    RenderingServer* rs = RenderingServer::get_singleton();
    RenderingDevice* rd = rs->get_rendering_device();
    uint64_t native_a = 0;
    uint64_t native_b = 0;
    if (rd != nullptr) {
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
            memdelete(instance);
            return;
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
            memdelete(instance);
            return;
        }
        native_a = rs->texture_get_native_handle(instance->rs_texture);
    }
    instance->target = bridge->wrap_render_target(
        instance->size.x, instance->size.y, native_a, native_b);

    instance->valid = true;
    instances[p_instance_id] = instance;

    {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        texture_mailbox[p_instance_id] = instance->rd_texture;
        canvas_texture_mailbox[p_instance_id] = instance->rs_texture;
    }
}

void RiveRenderServer::rt_frame(int64_t p_instance_id, double p_delta) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || !(*found)->valid || bridge == nullptr) {
        return;
    }
    Instance* instance = *found;

    if (instance->state_machine != nullptr) {
        const bool advancing =
            instance->state_machine->advanceAndApply(static_cast<float>(p_delta));
        if (advancing) {
            instance->settled = false;
            instance->needs_render = true;
        } else if (!instance->settled) {
            // Render one final frame in the settled pose, then sleep.
            instance->settled = true;
            instance->needs_render = true;
        }

        // Marshal reported events to the main-thread mailbox.
        const size_t event_count = instance->state_machine->reportedEventCount();
        if (event_count > 0) {
            Array batch;
            for (size_t i = 0; i < event_count; ++i) {
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
                    } else if (auto* n =
                                   child->as<rive::CustomPropertyNumber>()) {
                        properties[String::utf8(n->name().c_str())] =
                            n->propertyValue();
                    } else if (auto* s =
                                   child->as<rive::CustomPropertyString>()) {
                        properties[String::utf8(s->name().c_str())] =
                            String::utf8(s->propertyValue().c_str());
                    }
                }
                entry["properties"] = properties;
                batch.push_back(entry);
            }
            if (!batch.is_empty()) {
                std::lock_guard<std::mutex> lock(mailbox_mutex);
                Array& events = event_mailbox[p_instance_id];
                events.append_array(batch);
            }
        }

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

    if (!instance->needs_render) {
        return; // sleeping: settled machine, no external changes
    }
    instance->needs_render = false;

    bridge->begin_frame(instance->size.x, instance->size.y, 0x00000000);

    rive::RiveRenderer renderer(bridge->render_context());
    renderer.save();
    const ContainFit fit =
        contain_fit(instance->artboard->width(), instance->artboard->height(),
                    float(instance->size.x), float(instance->size.y));
    renderer.transform(rive::Mat2D(fit.scale, 0, 0, fit.scale, fit.tx, fit.ty));
    instance->artboard->draw(&renderer);
    renderer.restore();

    std::string error;
    if (!bridge->flush_to(instance->target.get(), &error)) {
        ERR_PRINT(String("rivegd: flush failed: ") + String::utf8(error.c_str()));
        instance->valid = false;
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
                                  const Vector2& p_node_size) {
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
    const ContainFit fit =
        contain_fit(instance->artboard->width(), instance->artboard->height(),
                    float(instance->size.x), float(instance->size.y));
    const rive::Vec2D position((texture_x - fit.tx) / fit.scale,
                               (texture_y - fit.ty) / fit.scale);

    switch (p_phase) {
        case POINTER_MOVE:
            instance->state_machine->pointerMove(position);
            break;
        case POINTER_DOWN:
            instance->state_machine->pointerDown(position);
            break;
        case POINTER_UP:
            instance->state_machine->pointerUp(position);
            break;
        case POINTER_EXIT:
            instance->state_machine->pointerExit(position);
            break;
    }
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
    auto* property =
        (*found)->view_model->propertyString(p_path.utf8().get_data());
    if (property != nullptr) {
        property->value(p_value.utf8().get_data());
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
        ERR_PRINT("rivegd: cannot watch view-model property '" + p_path +
                  "' (not found or unsupported type)");
        return;
    }
    instance->watched.push_back({p_path, value});

    // Report the current value immediately so get_property has a baseline.
    Dictionary change;
    change["path"] = p_path;
    change["value"] = read_vm_property(value);
    std::lock_guard<std::mutex> lock(mailbox_mutex);
    property_mailbox[p_instance_id].push_back(change);
}

void RiveRenderServer::rt_free_instance(int64_t p_instance_id) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr) {
        return;
    }
    Instance* instance = *found;
    instances.erase(p_instance_id);
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

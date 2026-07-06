#include "godot/rive_render_server.h"

#include "render/vulkan/vulkan_bridge.hpp"

#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/renderer/rive_renderer.hpp"

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
    rive::rcp<rive::gpu::RenderTargetVulkanImpl> target;
    RID rd_texture;
    Vector2i size;
    bool valid = false;
};

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

bool RiveRenderServer::rt_ensure_bridge() {
    if (bridge != nullptr) {
        return true;
    }
    if (bridge_failed) {
        return false;
    }

    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    if (rd == nullptr) {
        bridge_failed = true;
        ERR_PRINT("rivegd: no RenderingDevice (Compatibility renderer or "
                  "headless); Rive rendering is unavailable.");
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

    // Create the shared texture through Godot's RD so the scene side can
    // sample it via Texture2DRD; hand its VkImage to Rive as render target.
    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    Ref<RDTextureFormat> format;
    format.instantiate();
    format->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
    format->set_width(instance->size.x);
    format->set_height(instance->size.y);
    format->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
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

    uint64_t vk_image = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE, instance->rd_texture, 0);
    uint64_t vk_image_view = rd->get_driver_resource(
        RenderingDevice::DRIVER_RESOURCE_VULKAN_IMAGE_VIEW,
        instance->rd_texture, 0);
    instance->target = bridge->wrap_render_target(
        instance->size.x, instance->size.y, vk_image, vk_image_view);

    instance->valid = true;
    instances[p_instance_id] = instance;

    {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        texture_mailbox[p_instance_id] = instance->rd_texture;
    }
}

void RiveRenderServer::rt_frame(int64_t p_instance_id, double p_delta) {
    Instance** found = instances.getptr(p_instance_id);
    if (found == nullptr || !(*found)->valid || bridge == nullptr) {
        return;
    }
    Instance* instance = *found;

    if (instance->state_machine != nullptr) {
        instance->state_machine->advanceAndApply(static_cast<float>(p_delta));
    } else {
        instance->artboard->advance(static_cast<float>(p_delta));
    }

    bridge->begin_frame(instance->size.x, instance->size.y, 0x00000000);

    rive::RiveRenderer renderer(bridge->render_context());
    renderer.save();
    // Fit::contain, centered.
    const float sx = float(instance->size.x) / instance->artboard->width();
    const float sy = float(instance->size.y) / instance->artboard->height();
    const float scale = MIN(sx, sy);
    const float tx =
        (float(instance->size.x) - instance->artboard->width() * scale) * 0.5f;
    const float ty =
        (float(instance->size.y) - instance->artboard->height() * scale) * 0.5f;
    renderer.transform(rive::Mat2D(scale, 0, 0, scale, tx, ty));
    instance->artboard->draw(&renderer);
    renderer.restore();

    std::string error;
    if (!bridge->flush_to(instance->target.get(), &error)) {
        ERR_PRINT(String("rivegd: flush failed: ") + String::utf8(error.c_str()));
        instance->valid = false;
    }
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
    }
    if (bridge != nullptr) {
        // GPU may still be reading the target from the last flush.
        bridge->wait_idle();
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

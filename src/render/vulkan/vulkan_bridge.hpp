#pragma once

// render/ is Godot-free: raw graphics handles in, rive objects out.
// (tools/check_layering.sh enforces this.)

#include <cstdint>
#include <memory>
#include <string>

#include <vulkan/vulkan.h>

#include "rive/renderer/render_context.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

namespace rivegd::render {

// Native handles extracted from Godot's RenderingDevice
// (get_driver_resource). Plain integers so the Godot layer never includes
// vulkan headers and this layer never includes Godot headers.
struct GodotVulkanHandles {
    uint64_t instance = 0;        // VkInstance
    uint64_t physical_device = 0; // VkPhysicalDevice
    uint64_t device = 0;          // VkDevice
    uint64_t queue = 0;           // VkQueue (Godot's graphics queue)
    uint32_t queue_family = 0;
};

// Owns the Rive Vulkan RenderContext bootstrapped onto Godot's device, plus
// a small ring of command buffers/fences for flush submission.
//
// Threading: create() and all frame methods must run on the thread that owns
// the queue (Godot's render thread).
class VulkanBridge {
public:
    static std::unique_ptr<VulkanBridge> create(const GodotVulkanHandles& handles,
                                                std::string* out_error);
    ~VulkanBridge();

    rive::gpu::RenderContext* render_context() const { return m_context.get(); }
    // RenderContext doubles as the rive::Factory that must import files
    // whose GPU resources this context owns.
    rive::Factory* factory() const { return m_context.get(); }

    // Wraps an externally created image (e.g. Godot RD texture) as a Rive
    // render target. The image must be RGBA8_UNORM with usage flags
    // matching `usage_flags` and remain alive as long as the target.
    rive::rcp<rive::gpu::RenderTargetVulkanImpl> wrap_render_target(
        uint32_t width,
        uint32_t height,
        uint64_t vk_image,
        uint64_t vk_image_view);

    static constexpr VkFormat kTargetFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkImageUsageFlags kTargetUsageFlags =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Frame sequence: begin_frame() -> draw with a RiveRenderer -> flush_to().
    void begin_frame(uint32_t width, uint32_t height, uint32_t clear_color_argb);

    // Records this frame's draws into a command buffer, transitions the
    // target to SHADER_READ_ONLY_OPTIMAL (so Godot can sample it), and
    // submits on Godot's queue. Returns false on device error.
    bool flush_to(rive::gpu::RenderTargetVulkanImpl* target,
                  std::string* out_error);

    // Blocks until all in-flight flushes complete (teardown path).
    void wait_idle();

private:
    VulkanBridge() = default;
    bool init(const GodotVulkanHandles& handles, std::string* out_error);
    uint64_t update_safe_frame_number();

    void* m_vulkan_lib = nullptr;
    PFN_vkGetInstanceProcAddr m_get_instance_proc = nullptr;

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;

    // Device-level entry points (loaded via vkGetDeviceProcAddr).
    struct DeviceFns {
        PFN_vkCreateCommandPool CreateCommandPool = nullptr;
        PFN_vkDestroyCommandPool DestroyCommandPool = nullptr;
        PFN_vkAllocateCommandBuffers AllocateCommandBuffers = nullptr;
        PFN_vkBeginCommandBuffer BeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer EndCommandBuffer = nullptr;
        PFN_vkResetCommandBuffer ResetCommandBuffer = nullptr;
        PFN_vkCreateFence CreateFence = nullptr;
        PFN_vkDestroyFence DestroyFence = nullptr;
        PFN_vkGetFenceStatus GetFenceStatus = nullptr;
        PFN_vkWaitForFences WaitForFences = nullptr;
        PFN_vkResetFences ResetFences = nullptr;
        PFN_vkQueueSubmit QueueSubmit = nullptr;
        PFN_vkDeviceWaitIdle DeviceWaitIdle = nullptr;
    } m_fns;

    static constexpr int kFramesInFlight = 4;
    struct FrameSlot {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        uint64_t frame_number = 0;
        bool submitted = false;
    };
    FrameSlot m_slots[kFramesInFlight];

    uint64_t m_current_frame_number = 0;
    uint64_t m_safe_frame_number = 0;

    std::unique_ptr<rive::gpu::RenderContext> m_context;
};

} // namespace rivegd::render

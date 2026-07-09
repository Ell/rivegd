#include "render/vulkan/vulkan_bridge.hpp"

#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

#ifdef _WIN32
#include <windows.h>
#define rivegd_dlopen(name) reinterpret_cast<void*>(LoadLibraryA(name))
#define rivegd_dlsym(lib, sym) \
    reinterpret_cast<void*>( \
        GetProcAddress(reinterpret_cast<HMODULE>(lib), sym))
#define rivegd_dlclose(lib) FreeLibrary(reinterpret_cast<HMODULE>(lib))
#else
#include <dlfcn.h>
#define rivegd_dlopen(name) dlopen(name, RTLD_NOW | RTLD_LOCAL)
#define rivegd_dlsym(lib, sym) dlsym(lib, sym)
#define rivegd_dlclose(lib) dlclose(lib)
#endif

namespace rivegd::render {

std::unique_ptr<VulkanBridge> VulkanBridge::create(
    const GodotVulkanHandles& handles,
    std::string* out_error) {
    std::unique_ptr<VulkanBridge> bridge(new VulkanBridge());
    if (!bridge->init(handles, out_error)) {
        return nullptr;
    }
    return bridge;
}

bool VulkanBridge::init(const GodotVulkanHandles& handles,
                        std::string* out_error) {
    auto fail = [&](const char* msg) {
        if (out_error != nullptr) {
            *out_error = msg;
        }
        return false;
    };

    if (handles.instance == 0 || handles.physical_device == 0 ||
        handles.device == 0 || handles.queue == 0) {
        return fail("missing Vulkan driver handles (is Godot running a "
                    "RenderingDevice-based rendering method?)");
    }

#ifdef _WIN32
    m_vulkan_lib = rivegd_dlopen("vulkan-1.dll");
#else
    m_vulkan_lib = rivegd_dlopen("libvulkan.so.1");
#endif
    if (m_vulkan_lib == nullptr) {
#ifndef _WIN32
        m_vulkan_lib = rivegd_dlopen("libvulkan.so");
#endif
    }
    if (m_vulkan_lib == nullptr) {
        return fail("could not dlopen libvulkan");
    }
    m_get_instance_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        rivegd_dlsym(m_vulkan_lib, "vkGetInstanceProcAddr"));
    if (m_get_instance_proc == nullptr) {
        return fail("libvulkan has no vkGetInstanceProcAddr");
    }

    VkInstance instance = reinterpret_cast<VkInstance>(handles.instance);
    VkPhysicalDevice physical_device =
        reinterpret_cast<VkPhysicalDevice>(handles.physical_device);
    m_device = reinterpret_cast<VkDevice>(handles.device);
    m_queue = reinterpret_cast<VkQueue>(handles.queue);

    auto instance_fn = [&](const char* name) {
        return m_get_instance_proc(instance, name);
    };

    // Feature flags for Rive's context. Everything optional defaults to
    // false, which lands in atomic mode — correct on any conformant driver.
    auto get_features = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
        instance_fn("vkGetPhysicalDeviceFeatures"));
    auto get_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        instance_fn("vkGetPhysicalDeviceProperties"));
    if (get_features == nullptr || get_properties == nullptr) {
        return fail("failed to load Vulkan physical-device entry points");
    }
    VkPhysicalDeviceFeatures features{};
    get_features(physical_device, &features);
    VkPhysicalDeviceProperties properties{};
    get_properties(physical_device, &properties);

    rive::gpu::VulkanFeatures rive_features{};
    rive_features.apiVersion = properties.apiVersion;
    rive_features.independentBlend = features.independentBlend == VK_TRUE;
    rive_features.fillModeNonSolid = features.fillModeNonSolid == VK_TRUE;
    rive_features.fragmentStoresAndAtomics =
        features.fragmentStoresAndAtomics == VK_TRUE;
    rive_features.shaderClipDistance = features.shaderClipDistance == VK_TRUE;

    m_context = rive::gpu::RenderContextVulkanImpl::MakeContext(
        instance, physical_device, m_device, rive_features,
        m_get_instance_proc);
    if (m_context == nullptr) {
        return fail("RenderContextVulkanImpl::MakeContext failed");
    }

    auto get_device_proc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        instance_fn("vkGetDeviceProcAddr"));
    if (get_device_proc == nullptr) {
        return fail("failed to load vkGetDeviceProcAddr");
    }
    auto device_fn = [&](const char* name) {
        return get_device_proc(m_device, name);
    };
#define RIVEGD_LOAD_FN(NAME)                                                   \
    m_fns.NAME = reinterpret_cast<PFN_vk##NAME>(device_fn("vk" #NAME));        \
    if (m_fns.NAME == nullptr) {                                               \
        return fail("failed to load vk" #NAME);                                \
    }
    RIVEGD_LOAD_FN(CreateCommandPool)
    RIVEGD_LOAD_FN(DestroyCommandPool)
    RIVEGD_LOAD_FN(AllocateCommandBuffers)
    RIVEGD_LOAD_FN(BeginCommandBuffer)
    RIVEGD_LOAD_FN(EndCommandBuffer)
    RIVEGD_LOAD_FN(ResetCommandBuffer)
    RIVEGD_LOAD_FN(CreateFence)
    RIVEGD_LOAD_FN(DestroyFence)
    RIVEGD_LOAD_FN(GetFenceStatus)
    RIVEGD_LOAD_FN(WaitForFences)
    RIVEGD_LOAD_FN(ResetFences)
    RIVEGD_LOAD_FN(QueueSubmit)
    RIVEGD_LOAD_FN(DeviceWaitIdle)
#undef RIVEGD_LOAD_FN

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = handles.queue_family;
    if (m_fns.CreateCommandPool(m_device, &pool_info, nullptr,
                                &m_command_pool) != VK_SUCCESS) {
        return fail("vkCreateCommandPool failed");
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    for (FrameSlot& slot : m_slots) {
        if (m_fns.AllocateCommandBuffers(m_device, &alloc_info,
                                         &slot.command_buffer) != VK_SUCCESS ||
            m_fns.CreateFence(m_device, &fence_info, nullptr, &slot.fence) !=
                VK_SUCCESS) {
            return fail("failed to allocate frame command buffers/fences");
        }
    }
    return true;
}

VulkanBridge::~VulkanBridge() {
    if (m_device != VK_NULL_HANDLE && m_fns.DeviceWaitIdle != nullptr) {
        m_fns.DeviceWaitIdle(m_device);
    }
    // The context owns GPU resources on Godot's device; destroy it while the
    // device is still alive (extension teardown happens before device
    // teardown).
    if (m_context != nullptr) {
        m_context->releaseResources();
        m_context.reset();
    }
    for (FrameSlot& slot : m_slots) {
        if (slot.fence != VK_NULL_HANDLE && m_fns.DestroyFence != nullptr) {
            m_fns.DestroyFence(m_device, slot.fence, nullptr);
        }
    }
    if (m_command_pool != VK_NULL_HANDLE &&
        m_fns.DestroyCommandPool != nullptr) {
        m_fns.DestroyCommandPool(m_device, m_command_pool, nullptr);
    }
    if (m_vulkan_lib != nullptr) {
        rivegd_dlclose(m_vulkan_lib);
    }
}

rive::rcp<rive::RenderImage> VulkanBridge::adopt_texture(uint64_t p_image,
                                                         uint32_t p_width,
                                                         uint32_t p_height,
                                                         uint32_t p_format) {
    auto* impl = m_context->static_impl_cast<
        rive::gpu::RenderContextVulkanImpl>();
    rive::rcp<rive::gpu::Texture> texture = impl->adoptImageTexture(
        reinterpret_cast<VkImage>(p_image), p_width, p_height,
        static_cast<VkFormat>(p_format));
    if (texture == nullptr) {
        return nullptr;
    }
    return rive::make_rcp<rive::RiveRenderImage>(std::move(texture));
}

rive::rcp<rive::gpu::RenderTarget> VulkanBridge::wrap_render_target(
    uint32_t width,
    uint32_t height,
    uint64_t native_handle_a,
    uint64_t native_handle_b) {
    const uint64_t vk_image = native_handle_a;
    const uint64_t vk_image_view = native_handle_b;
    auto* impl =
        m_context->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
    rive::rcp<rive::gpu::RenderTargetVulkanImpl> target =
        impl->makeRenderTarget(width, height, kTargetFormat,
                               kTargetUsageFlags);
    // Godot created the image; a fresh RD texture starts life undefined
    // until Godot's tracker transitions it. Declare it undefined so Rive's
    // first access performs a full transition.
    target->setTargetImageView(
        reinterpret_cast<VkImageView>(vk_image_view),
        reinterpret_cast<VkImage>(vk_image),
        rive::gpu::vkutil::ImageAccess{
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_NONE,
            VK_IMAGE_LAYOUT_UNDEFINED,
        });
    return target;
}

void VulkanBridge::begin_frame(uint32_t width,
                               uint32_t height,
                               uint32_t clear_color_argb) {
    rive::gpu::RenderContext::FrameDescriptor descriptor{};
    descriptor.renderTargetWidth = width;
    descriptor.renderTargetHeight = height;
    descriptor.loadAction = rive::gpu::LoadAction::clear;
    descriptor.clearColor = clear_color_argb;
    m_context->beginFrame(descriptor);
}

uint64_t VulkanBridge::update_safe_frame_number() {
    for (FrameSlot& slot : m_slots) {
        if (slot.submitted &&
            m_fns.GetFenceStatus(m_device, slot.fence) == VK_SUCCESS) {
            if (slot.frame_number > m_safe_frame_number) {
                m_safe_frame_number = slot.frame_number;
            }
        }
    }
    return m_safe_frame_number;
}

bool VulkanBridge::begin_batch(std::string* out_error) {
    auto fail = [&](const char* msg) {
        if (out_error != nullptr) {
            *out_error = msg;
        }
        return false;
    };
    if (m_active_slot != nullptr) {
        return fail("begin_batch called twice");
    }

    m_current_frame_number++;
    FrameSlot& slot = m_slots[m_current_frame_number % kFramesInFlight];
    if (slot.submitted) {
        // Ring wrapped: this slot's previous submission must be complete.
        if (m_fns.WaitForFences(m_device, 1, &slot.fence, VK_TRUE,
                                UINT64_MAX) != VK_SUCCESS) {
            return fail("vkWaitForFences failed");
        }
        if (slot.frame_number > m_safe_frame_number) {
            m_safe_frame_number = slot.frame_number;
        }
        m_fns.ResetFences(m_device, 1, &slot.fence);
        m_fns.ResetCommandBuffer(slot.command_buffer, 0);
    }
    slot.frame_number = m_current_frame_number;

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (m_fns.BeginCommandBuffer(slot.command_buffer, &begin_info) !=
        VK_SUCCESS) {
        return fail("vkBeginCommandBuffer failed");
    }
    m_active_slot = &slot;
    return true;
}

bool VulkanBridge::flush_target(rive::gpu::RenderTarget* target_base,
                                uint64_t /*native_texture*/,
                                std::string* out_error) {
    // Targets handed to this bridge were created by wrap_render_target.
    auto* target =
        static_cast<rive::gpu::RenderTargetVulkanImpl*>(target_base);
    if (m_active_slot == nullptr) {
        if (out_error != nullptr) {
            *out_error = "flush_target outside a batch";
        }
        return false;
    }

    rive::gpu::RenderContext::FlushResources resources{};
    resources.renderTarget = target;
    resources.externalCommandBuffer = m_active_slot->command_buffer;
    resources.currentFrameNumber = m_current_frame_number;
    resources.safeFrameNumber = update_safe_frame_number();
    m_context->flush(resources);

    // Leave the image where Godot's sampler expects it. Rive records the
    // matching barrier because the target tracks its last access.
    target->accessTargetImage(
        m_active_slot->command_buffer,
        rive::gpu::vkutil::ImageAccess{
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });
    return true;
}

bool VulkanBridge::end_batch(std::string* out_error) {
    auto fail = [&](const char* msg) {
        if (out_error != nullptr) {
            *out_error = msg;
        }
        return false;
    };
    if (m_active_slot == nullptr) {
        return fail("end_batch outside a batch");
    }
    FrameSlot& slot = *m_active_slot;
    m_active_slot = nullptr;

    if (m_fns.EndCommandBuffer(slot.command_buffer) != VK_SUCCESS) {
        return fail("vkEndCommandBuffer failed");
    }
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &slot.command_buffer;
    if (m_fns.QueueSubmit(m_queue, 1, &submit_info, slot.fence) !=
        VK_SUCCESS) {
        return fail("vkQueueSubmit failed");
    }
    slot.submitted = true;
    return true;
}

void VulkanBridge::wait_idle() {
    if (m_device != VK_NULL_HANDLE && m_fns.DeviceWaitIdle != nullptr) {
        m_fns.DeviceWaitIdle(m_device);
    }
}

} // namespace rivegd::render

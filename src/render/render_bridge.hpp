#pragma once

// Backend-agnostic interface between the Godot layer and a Rive GPU
// render context. One implementation per graphics API (GOALS G1.3):
// Vulkan today; Metal, D3D12, and GL/GLES to follow. render/ is Godot-free.

#include <cstdint>
#include <memory>
#include <string>

#include "rive/factory.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_target.hpp"

namespace rivegd::render {

class RenderBridge {
public:
    virtual ~RenderBridge() = default;

    // The context doubles as the rive::Factory that must import files whose
    // GPU resources it owns.
    virtual rive::gpu::RenderContext* render_context() const = 0;
    rive::Factory* factory() const { return render_context(); }

    // Wraps a texture created by the host as a Rive render target. The two
    // native handles are backend-defined (Vulkan: VkImage + VkImageView;
    // GL: texture id + 0; ...). Must stay alive as long as the target.
    virtual rive::rcp<rive::gpu::RenderTarget> wrap_render_target(
        uint32_t width,
        uint32_t height,
        uint64_t native_handle_a,
        uint64_t native_handle_b) = 0;

    // Frame sequence (one batch per engine frame, N targets per batch):
    //   begin_batch()
    //     { begin_frame -> draw with a RiveRenderer -> flush_target } x N
    //   end_batch()  // single submission
    // All frame methods run on the thread that owns the graphics queue.
    virtual bool begin_batch(std::string* out_error) = 0;
    virtual void begin_frame(uint32_t width,
                             uint32_t height,
                             uint32_t clear_color_argb) = 0;
    virtual bool flush_target(rive::gpu::RenderTarget* target,
                              std::string* out_error) = 0;
    virtual bool end_batch(std::string* out_error) = 0;

    // Blocks until in-flight GPU work completes (teardown path).
    virtual void wait_idle() = 0;
};

} // namespace rivegd::render

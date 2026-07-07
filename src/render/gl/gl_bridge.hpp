#pragma once

// GL/GLES backend for Godot's Compatibility renderer (GOALS G1.3) — shares
// Godot's GL context, so every frame method must run on the thread that owns
// it (Godot's render thread). render/ is Godot-free.

#include "render/render_bridge.hpp"

namespace rivegd::render {

class GLBridge final : public RenderBridge {
public:
    // Requires Godot's GL context to be current on the calling thread.
    static std::unique_ptr<GLBridge> create(std::string* out_error);
    ~GLBridge() override;

    rive::gpu::RenderContext* render_context() const override {
        return m_context.get();
    }

    // native_handle_a = GL texture id (RGBA8), native_handle_b unused.
    rive::rcp<rive::gpu::RenderTarget> wrap_render_target(
        uint32_t width,
        uint32_t height,
        uint64_t native_handle_a,
        uint64_t native_handle_b) override;

    bool begin_batch(std::string* out_error) override;
    void begin_frame(uint32_t width, uint32_t height,
                     uint32_t clear_color_argb) override;
    bool flush_target(rive::gpu::RenderTarget* target,
                      std::string* out_error) override;
    bool end_batch(std::string* out_error) override;
    void wait_idle() override;

private:
    GLBridge() = default;

    void* m_gl_lib = nullptr;
    std::unique_ptr<rive::gpu::RenderContext> m_context;
};

} // namespace rivegd::render

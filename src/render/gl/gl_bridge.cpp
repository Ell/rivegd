#include "render/gl/gl_bridge.hpp"

#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/texture.hpp"

#include "glad_custom.h" // gladLoadCustomLoader (in rive_pls_renderer)

#include <dlfcn.h>

namespace rivegd::render {

// GL proc resolution: try GLX then EGL, falling back to direct dlsym — this
// covers Godot's X11, Wayland, and ANGLE paths on desktop Linux.
static void* s_gl_lib = nullptr;
static void* (*s_glx_get_proc)(const unsigned char*) = nullptr;
static void* (*s_egl_get_proc)(const char*) = nullptr;

static GLADapiproc gl_loader(const char* name) {
    if (s_glx_get_proc != nullptr) {
        if (void* p = s_glx_get_proc(
                reinterpret_cast<const unsigned char*>(name))) {
            return reinterpret_cast<GLADapiproc>(p);
        }
    }
    if (s_egl_get_proc != nullptr) {
        if (void* p = s_egl_get_proc(name)) {
            return reinterpret_cast<GLADapiproc>(p);
        }
    }
    return s_gl_lib != nullptr
               ? reinterpret_cast<GLADapiproc>(dlsym(s_gl_lib, name))
               : nullptr;
}

std::unique_ptr<GLBridge> GLBridge::create(std::string* out_error) {
    auto fail = [&](const char* msg) {
        if (out_error != nullptr) {
            *out_error = msg;
        }
        return nullptr;
    };

    if (s_gl_lib == nullptr) {
        s_gl_lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
        if (s_gl_lib != nullptr) {
            s_glx_get_proc = reinterpret_cast<void* (*)(const unsigned char*)>(
                dlsym(s_gl_lib, "glXGetProcAddressARB"));
        }
        if (void* egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL)) {
            s_egl_get_proc = reinterpret_cast<void* (*)(const char*)>(
                dlsym(egl, "eglGetProcAddress"));
        }
    }
    if (s_gl_lib == nullptr && s_egl_get_proc == nullptr) {
        return fail("could not load libGL/libEGL");
    }

    if (!gladLoadCustomLoader(&gl_loader)) {
        return fail("gladLoadCustomLoader failed (no current GL context?)");
    }

    std::unique_ptr<GLBridge> bridge(new GLBridge());
    bridge->m_context = rive::gpu::RenderContextGLImpl::MakeContext();
    if (bridge->m_context == nullptr) {
        return fail("RenderContextGLImpl::MakeContext failed");
    }
    return bridge;
}

GLBridge::~GLBridge() {
    if (m_context != nullptr) {
        m_context->releaseResources();
    }
}

rive::rcp<rive::gpu::RenderTarget> GLBridge::wrap_render_target(
    uint32_t width,
    uint32_t height,
    uint64_t native_handle_a,
    uint64_t /*native_handle_b*/) {
    auto target =
        rive::make_rcp<rive::gpu::TextureRenderTargetGL>(width, height);
    target->setTargetTexture(static_cast<unsigned int>(native_handle_a));
    return target;
}

void GLBridge::begin_frame(uint32_t width, uint32_t height,
                           uint32_t clear_color_argb) {
    // Godot mutated GL state since our last flush; drop all cached state.
    m_context->static_impl_cast<rive::gpu::RenderContextGLImpl>()
        ->invalidateGLState();

    rive::gpu::RenderContext::FrameDescriptor descriptor{};
    descriptor.renderTargetWidth = width;
    descriptor.renderTargetHeight = height;
    descriptor.loadAction = rive::gpu::LoadAction::clear;
    descriptor.clearColor = clear_color_argb;
    m_context->beginFrame(descriptor);
}

bool GLBridge::begin_batch(std::string*) { return true; }

bool GLBridge::end_batch(std::string*) { return true; }

bool GLBridge::flush_target(rive::gpu::RenderTarget* target,
                            std::string* out_error) {
    rive::gpu::RenderContext::FlushResources resources{};
    resources.renderTarget = target;
    m_context->flush(resources);

    // Leave GL in a state Godot's renderer can cope with: unbind everything
    // Rive bound internally (mirrors rive's own GL interop example).
    m_context->static_impl_cast<rive::gpu::RenderContextGLImpl>()
        ->unbindGLInternalResources();
    return true;
}

void GLBridge::wait_idle() {
    if (auto finish =
            reinterpret_cast<void (*)()>(gl_loader("glFinish"))) {
        finish();
    }
}

} // namespace rivegd::render

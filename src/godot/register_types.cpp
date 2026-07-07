#include "godot/register_types.h"

#include "godot/editor/rive_editor_plugin.h"
#include "godot/rive_audio_stream.h"
#include "godot/rive_control.h"
#include "godot/rive_file_loader.h"
#include "godot/rive_file_resource.h"
#include "godot/rive_render_server.h"
#include "godot/rive_sprite_2d.h"
#include "godot/rive_texture.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static Ref<rivegd::RiveFileLoader> riv_loader;

void initialize_rivegd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        GDREGISTER_INTERNAL_CLASS(rivegd::RiveInspectorPlugin);
        GDREGISTER_INTERNAL_CLASS(rivegd::RiveDropOverlay);
        GDREGISTER_INTERNAL_CLASS(rivegd::RivePreviewGenerator);
        GDREGISTER_INTERNAL_CLASS(rivegd::RiveEditorPlugin);
        EditorPlugins::add_by_type<rivegd::RiveEditorPlugin>();
        return;
    }
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(rivegd::RiveFileResource);
    GDREGISTER_CLASS(rivegd::RiveFileLoader);
    GDREGISTER_INTERNAL_CLASS(rivegd::RiveRenderServer);
    GDREGISTER_CLASS(rivegd::RiveSprite2D);
    GDREGISTER_CLASS(rivegd::RiveControl);
    GDREGISTER_CLASS(rivegd::RiveTexture);
    GDREGISTER_CLASS(rivegd::RiveAudioStream);
    GDREGISTER_CLASS(rivegd::RiveAudioStreamPlayback);
    rivegd::RiveRenderServer::create_singleton();

    riv_loader.instantiate();
    ResourceLoader::get_singleton()->add_resource_format_loader(riv_loader);
}

void uninitialize_rivegd_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
        EditorPlugins::remove_by_type<rivegd::RiveEditorPlugin>();
        return;
    }
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ResourceLoader::get_singleton()->remove_resource_format_loader(riv_loader);
    riv_loader.unref();
    rivegd::RiveRenderServer::free_singleton();
}

extern "C" {
GDExtensionBool GDE_EXPORT
rivegd_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                    const GDExtensionClassLibraryPtr p_library,
                    GDExtensionInitialization* r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address,
                                                   p_library,
                                                   r_initialization);
    init_obj.register_initializer(initialize_rivegd_module);
    init_obj.register_terminator(uninitialize_rivegd_module);
    init_obj.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}

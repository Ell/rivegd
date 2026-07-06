#include "godot/register_types.h"

#include "godot/rive_file_resource.h"
#include "godot/rive_render_server.h"
#include "godot/rive_sprite_2d.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_rivegd_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(rivegd::RiveFileResource);
    GDREGISTER_INTERNAL_CLASS(rivegd::RiveRenderServer);
    GDREGISTER_CLASS(rivegd::RiveSprite2D);
    rivegd::RiveRenderServer::create_singleton();
}

void uninitialize_rivegd_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
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

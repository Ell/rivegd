#include <gdextension_interface.h>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
using namespace godot;

class NullResource : public Resource {
    GDCLASS(NullResource, Resource)
protected:
    static void _bind_methods() {}
};

static void init_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(NullResource);
}
static void uninit_module(ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT null_ext_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(init_module);
    init_obj.register_terminator(uninit_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}

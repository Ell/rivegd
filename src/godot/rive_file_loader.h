#pragma once

#include <godot_cpp/classes/resource_format_loader.hpp>

namespace rivegd {

// Loads .riv files directly as RiveFileResource: `load("res://ui/menu.riv")`
// just works, in editor and exported games alike (GOALS G3.1).
class RiveFileLoader : public godot::ResourceFormatLoader {
    GDCLASS(RiveFileLoader, godot::ResourceFormatLoader)

public:
    godot::PackedStringArray _get_recognized_extensions() const override;
    bool _handles_type(const godot::StringName& p_type) const override;
    godot::String _get_resource_type(const godot::String& p_path) const override;
    godot::Variant _load(const godot::String& p_path,
                         const godot::String& p_original_path,
                         bool p_use_sub_threads,
                         int32_t p_cache_mode) const override;

protected:
    static void _bind_methods() {}
};

} // namespace rivegd

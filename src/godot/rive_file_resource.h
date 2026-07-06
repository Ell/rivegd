#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

#include <memory>

namespace rivegd::core {
class RivFile;
}

namespace rivegd {

// A .riv file held as raw bytes plus parsed metadata. The bytes are the
// serialized property; parsing happens on set_data (see
// docs/implementation-strategy.md §5 for why decode stays lazy/context-free).
class RiveFileResource : public godot::Resource {
    GDCLASS(RiveFileResource, godot::Resource)

public:
    RiveFileResource();
    ~RiveFileResource() override;

    void set_data(const godot::PackedByteArray& p_data);
    godot::PackedByteArray get_data() const { return data; }

    bool is_valid() const { return riv_file != nullptr; }
    godot::String get_import_error() const { return import_error; }

    godot::PackedStringArray get_artboard_names() const;
    godot::PackedStringArray get_state_machine_names(
        const godot::String& p_artboard) const;
    godot::PackedStringArray get_animation_names(
        const godot::String& p_artboard) const;

protected:
    static void _bind_methods();

private:
    godot::PackedByteArray data;
    godot::String import_error;
    std::unique_ptr<core::RivFile> riv_file;
};

} // namespace rivegd

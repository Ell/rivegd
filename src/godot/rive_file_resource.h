#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/templates/local_vector.hpp>

namespace rive {
class CommandQueue;
}
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
    // Fallback fonts (G5.1, global): consulted in registration order when
    // an authored font misses a glyph. Accepts a FontFile or raw
    // TTF/OTF bytes. Returns false when the data fails to decode.
    static bool add_fallback_font(const godot::Variant& p_font);
    static void clear_fallback_fonts();

    // Assets the file references: [{name, unique_name, unique_filename,
    // type, resolved}] — unresolved entries are out-of-band (GOALS G3.6).
    godot::Array get_asset_descriptions() const;

    // One CommandQueue import shared by every instance of this resource
    // ("Caching Rive Files"). acquire mints on first use (registering
    // out-of-band assets once); release deletes queue objects at refcount
    // zero. set_data() detaches the current SharedFile so hot reload mints
    // a fresh import while surviving instances wind down the old one.
    // Deletion rule: freed when refs==0 AND retired. The CURRENT
    // generation is never freed at refs==0 (the resource still points at
    // it for reuse); set_data()/destruction retires it.
    struct SharedFile {
        rive::CommandQueue* queue = nullptr;
        uint64_t file_handle = 0;
        int refs = 0;
        bool retired = false;
        struct OobHandle {
            int type = 0; // 0 image, 1 font, 2 audio
            uint64_t handle = 0;
        };
        godot::LocalVector<OobHandle> oob_handles;
    };
    SharedFile* acquire_shared_file(rive::CommandQueue* p_queue);
    static void unref_shared_file(SharedFile* p_shared);
    godot::PackedStringArray get_state_machine_names(
        const godot::String& p_artboard) const;
    godot::PackedStringArray get_animation_names(
        const godot::String& p_artboard) const;
    godot::Array get_input_descriptions(
        const godot::String& p_artboard,
        const godot::String& p_state_machine) const;
    godot::Array get_property_descriptions(const godot::String& p_artboard) const;
    godot::Vector2 get_artboard_size(const godot::String& p_artboard) const;

protected:
    static void _bind_methods();

private:
    godot::PackedByteArray data;
    godot::String import_error;
    std::unique_ptr<core::RivFile> riv_file;
    SharedFile* shared_file = nullptr; // current generation (owned refs)
};

} // namespace rivegd

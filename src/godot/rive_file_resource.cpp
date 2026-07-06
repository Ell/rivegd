#include "godot/rive_file_resource.h"

#include "core/riv_file.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

RiveFileResource::RiveFileResource() = default;
RiveFileResource::~RiveFileResource() = default;

void RiveFileResource::set_data(const PackedByteArray& p_data) {
    data = p_data;
    riv_file.reset();
    import_error = String();

    if (!data.is_empty()) {
        std::string error;
        riv_file = core::RivFile::import(data.ptr(), data.size(), &error);
        if (riv_file == nullptr) {
            import_error = String::utf8(error.c_str());
        }
    }
    emit_changed();
}

PackedStringArray RiveFileResource::get_artboard_names() const {
    PackedStringArray names;
    if (riv_file != nullptr) {
        for (const core::ArtboardMeta& meta : riv_file->artboards()) {
            names.push_back(String::utf8(meta.name.c_str()));
        }
    }
    return names;
}

PackedStringArray RiveFileResource::get_state_machine_names(
    const String& p_artboard) const {
    PackedStringArray names;
    if (riv_file != nullptr) {
        const core::ArtboardMeta* meta =
            riv_file->find_artboard(p_artboard.utf8().get_data());
        if (meta != nullptr) {
            for (const std::string& name : meta->state_machines) {
                names.push_back(String::utf8(name.c_str()));
            }
        }
    }
    return names;
}

PackedStringArray RiveFileResource::get_animation_names(
    const String& p_artboard) const {
    PackedStringArray names;
    if (riv_file != nullptr) {
        const core::ArtboardMeta* meta =
            riv_file->find_artboard(p_artboard.utf8().get_data());
        if (meta != nullptr) {
            for (const std::string& name : meta->animations) {
                names.push_back(String::utf8(name.c_str()));
            }
        }
    }
    return names;
}

void RiveFileResource::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_data", "data"),
                         &RiveFileResource::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &RiveFileResource::get_data);
    ClassDB::bind_method(D_METHOD("is_valid"), &RiveFileResource::is_valid);
    ClassDB::bind_method(D_METHOD("get_import_error"),
                         &RiveFileResource::get_import_error);
    ClassDB::bind_method(D_METHOD("get_artboard_names"),
                         &RiveFileResource::get_artboard_names);
    ClassDB::bind_method(D_METHOD("get_state_machine_names", "artboard"),
                         &RiveFileResource::get_state_machine_names);
    ClassDB::bind_method(D_METHOD("get_animation_names", "artboard"),
                         &RiveFileResource::get_animation_names);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data",
                              PROPERTY_HINT_NONE, "",
                              PROPERTY_USAGE_STORAGE),
                 "set_data", "get_data");
}

} // namespace rivegd

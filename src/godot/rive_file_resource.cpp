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
            for (const core::StateMachineMeta& sm : meta->state_machines) {
                names.push_back(String::utf8(sm.name.c_str()));
            }
        }
    }
    return names;
}

// Array of { name: String, type: "bool"|"number"|"trigger", default: Variant }.
Array RiveFileResource::get_input_descriptions(
    const String& p_artboard, const String& p_state_machine) const {
    Array out;
    if (riv_file == nullptr) {
        return out;
    }
    const core::ArtboardMeta* artboard_meta =
        riv_file->find_artboard(p_artboard.utf8().get_data());
    if (artboard_meta == nullptr && !riv_file->artboards().empty() &&
        p_artboard.is_empty()) {
        artboard_meta = &riv_file->artboards()[0];
    }
    if (artboard_meta == nullptr) {
        return out;
    }
    const core::StateMachineMeta* sm_meta =
        artboard_meta->find_state_machine(p_state_machine.utf8().get_data());
    if (sm_meta == nullptr && !artboard_meta->state_machines.empty() &&
        p_state_machine.is_empty()) {
        sm_meta = &artboard_meta->state_machines[0];
    }
    if (sm_meta == nullptr) {
        return out;
    }
    for (const core::InputMeta& input : sm_meta->inputs) {
        Dictionary description;
        description["name"] = String::utf8(input.name.c_str());
        switch (input.type) {
            case core::InputMeta::Type::boolean:
                description["type"] = "bool";
                description["default"] = input.default_bool;
                break;
            case core::InputMeta::Type::number:
                description["type"] = "number";
                description["default"] = input.default_number;
                break;
            case core::InputMeta::Type::trigger:
                description["type"] = "trigger";
                break;
        }
        out.push_back(description);
    }
    return out;
}

// Array of { name: String, type: "number"|"string"|"boolean"|"color"|... }.
Array RiveFileResource::get_property_descriptions(
    const String& p_artboard) const {
    Array out;
    if (riv_file == nullptr) {
        return out;
    }
    const core::ArtboardMeta* meta =
        riv_file->find_artboard(p_artboard.utf8().get_data());
    if (meta == nullptr && !riv_file->artboards().empty() &&
        p_artboard.is_empty()) {
        meta = &riv_file->artboards()[0];
    }
    if (meta == nullptr) {
        return out;
    }
    for (const core::VmPropertyMeta& property : meta->view_model_properties) {
        Dictionary description;
        description["name"] = String::utf8(property.name.c_str());
        description["type"] = String::utf8(property.type.c_str());
        out.push_back(description);
    }
    return out;
}

Vector2 RiveFileResource::get_artboard_size(const String& p_artboard) const {
    if (riv_file == nullptr) {
        return Vector2();
    }
    const core::ArtboardMeta* meta =
        riv_file->find_artboard(p_artboard.utf8().get_data());
    if (meta == nullptr && !riv_file->artboards().empty() &&
        p_artboard.is_empty()) {
        meta = &riv_file->artboards()[0];
    }
    return meta != nullptr ? Vector2(meta->width, meta->height) : Vector2();
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
    ClassDB::bind_method(
        D_METHOD("get_input_descriptions", "artboard", "state_machine"),
        &RiveFileResource::get_input_descriptions);
    ClassDB::bind_method(D_METHOD("get_artboard_size", "artboard"),
                         &RiveFileResource::get_artboard_size);
    ClassDB::bind_method(D_METHOD("get_property_descriptions", "artboard"),
                         &RiveFileResource::get_property_descriptions);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data",
                              PROPERTY_HINT_NONE, "",
                              PROPERTY_USAGE_STORAGE),
                 "set_data", "get_data");
}

} // namespace rivegd

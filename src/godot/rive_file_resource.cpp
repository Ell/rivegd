#include "godot/rive_file_resource.h"

#include "core/riv_file.hpp"

#include "rive/command_queue.hpp"

#include <godot_cpp/classes/file_access.hpp>

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

// Queue-object cleanup + free, once a SharedFile is unreachable.
static void destroy_shared_file(RiveFileResource::SharedFile* p_shared) {
    rive::CommandQueue* queue = p_shared->queue;
    queue->deleteFile(
        reinterpret_cast<rive::FileHandle>(p_shared->file_handle));
    for (const RiveFileResource::SharedFile::OobHandle& oob :
         p_shared->oob_handles) {
        switch (oob.type) {
            case 0:
                queue->deleteImage(
                    reinterpret_cast<rive::RenderImageHandle>(oob.handle));
                break;
            case 1:
                queue->deleteFont(
                    reinterpret_cast<rive::FontHandle>(oob.handle));
                break;
            case 2:
                queue->deleteAudio(
                    reinterpret_cast<rive::AudioSourceHandle>(oob.handle));
                break;
        }
    }
    delete p_shared;
}

static void retire_shared_file(RiveFileResource::SharedFile* p_shared) {
    p_shared->retired = true;
    if (p_shared->refs == 0) {
        destroy_shared_file(p_shared);
    }
}

RiveFileResource::RiveFileResource() = default;

RiveFileResource::~RiveFileResource() {
    if (shared_file != nullptr) {
        retire_shared_file(shared_file);
        shared_file = nullptr;
    }
}

void RiveFileResource::set_data(const PackedByteArray& p_data) {
    data = p_data;
    riv_file.reset();
    import_error = String();
    // Retire the cached queue import: instances rebuilt by the changed
    // signal mint a fresh one; survivors wind the old one down to zero.
    if (shared_file != nullptr) {
        retire_shared_file(shared_file);
        shared_file = nullptr;
    }

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

RiveFileResource::SharedFile* RiveFileResource::acquire_shared_file(
    rive::CommandQueue* p_queue) {
    if (shared_file == nullptr) {
        SharedFile* shared = new SharedFile();
        shared->queue = p_queue;
        // Out-of-band assets (G3.6): register referenced assets found by
        // rive's sibling-file convention BEFORE loadFile (one FIFO).
        const String base_dir = get_path().get_base_dir();
        if (!base_dir.is_empty() && riv_file != nullptr) {
            for (const core::AssetMeta& asset : riv_file->assets()) {
                if (asset.resolved) {
                    continue;
                }
                const String sibling = base_dir.path_join(
                    String::utf8(asset.unique_filename.c_str()));
                if (!FileAccess::file_exists(sibling)) {
                    continue;
                }
                const PackedByteArray asset_bytes =
                    FileAccess::get_file_as_bytes(sibling);
                std::vector<uint8_t> raw(
                    asset_bytes.ptr(), asset_bytes.ptr() + asset_bytes.size());
                if (asset.type == "image") {
                    auto handle = p_queue->decodeImage(std::move(raw));
                    p_queue->addGlobalImageAsset(asset.unique_name, handle);
                    shared->oob_handles.push_back(
                        {0, reinterpret_cast<uint64_t>(handle)});
                } else if (asset.type == "font") {
                    auto handle = p_queue->decodeFont(std::move(raw));
                    p_queue->addGlobalFontAsset(asset.unique_name, handle);
                    shared->oob_handles.push_back(
                        {1, reinterpret_cast<uint64_t>(handle)});
                } else if (asset.type == "audio") {
                    auto handle = p_queue->decodeAudio(std::move(raw));
                    p_queue->addGlobalAudioAsset(asset.unique_name, handle);
                    shared->oob_handles.push_back(
                        {2, reinterpret_cast<uint64_t>(handle)});
                }
            }
        }
        shared->file_handle = reinterpret_cast<uint64_t>(p_queue->loadFile(
            std::vector<uint8_t>(data.ptr(), data.ptr() + data.size())));
        shared_file = shared;
    }
    shared_file->refs++;
    return shared_file;
}

void RiveFileResource::unref_shared_file(SharedFile* p_shared) {
    if (p_shared == nullptr) {
        return;
    }
    p_shared->refs--;
    if (p_shared->refs == 0 && p_shared->retired) {
        destroy_shared_file(p_shared);
    }
}

Array RiveFileResource::get_asset_descriptions() const {
    Array out;
    if (riv_file == nullptr) {
        return out;
    }
    for (const core::AssetMeta& asset : riv_file->assets()) {
        Dictionary d;
        d["name"] = String::utf8(asset.name.c_str());
        d["unique_name"] = String::utf8(asset.unique_name.c_str());
        d["unique_filename"] = String::utf8(asset.unique_filename.c_str());
        d["type"] = String::utf8(asset.type.c_str());
        d["resolved"] = asset.resolved;
        out.push_back(d);
    }
    return out;
}

PackedStringArray RiveFileResource::get_state_machine_names(
    const String& p_artboard) const {
    PackedStringArray names;
    if (riv_file != nullptr) {
        const core::ArtboardMeta* meta =
            riv_file->find_artboard(p_artboard.utf8().get_data());
        if (meta == nullptr && !riv_file->artboards().empty() &&
            p_artboard.is_empty()) {
            meta = &riv_file->artboards()[0];
        }
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
        if (!property.enum_values.empty()) {
            PackedStringArray values;
            for (const std::string& value : property.enum_values) {
                values.push_back(String::utf8(value.c_str()));
            }
            description["enum_values"] = values;
        }
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
    ClassDB::bind_method(D_METHOD("get_asset_descriptions"),
                         &RiveFileResource::get_asset_descriptions);
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

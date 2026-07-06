#include "godot/rive_file_loader.h"

#include "godot/rive_file_resource.h"

#include <godot_cpp/classes/file_access.hpp>

using namespace godot;

namespace rivegd {

PackedStringArray RiveFileLoader::_get_recognized_extensions() const {
    PackedStringArray extensions;
    extensions.push_back("riv");
    return extensions;
}

bool RiveFileLoader::_handles_type(const StringName& p_type) const {
    return p_type == StringName("RiveFileResource") ||
           p_type == StringName("Resource");
}

String RiveFileLoader::_get_resource_type(const String& p_path) const {
    return p_path.get_extension().to_lower() == "riv" ? "RiveFileResource"
                                                      : "";
}

Variant RiveFileLoader::_load(const String& p_path,
                              const String& p_original_path,
                              bool p_use_sub_threads,
                              int32_t p_cache_mode) const {
    PackedByteArray bytes = FileAccess::get_file_as_bytes(p_path);
    if (bytes.is_empty() && FileAccess::get_open_error() != OK) {
        return Variant(static_cast<int64_t>(ERR_CANT_OPEN));
    }
    Ref<RiveFileResource> resource;
    resource.instantiate();
    resource->set_data(bytes);
    if (!resource->is_valid()) {
        ERR_PRINT("rivegd: failed to load '" + p_path +
                  "': " + resource->get_import_error());
        return Variant(static_cast<int64_t>(ERR_FILE_CORRUPT));
    }
    return resource;
}

} // namespace rivegd

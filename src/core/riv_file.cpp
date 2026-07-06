#include "core/riv_file.hpp"

#include "rive/artboard.hpp"
#include "utils/no_op_factory.hpp"

namespace rivegd::core {

static rive::NoOpFactory& metadata_factory() {
    static rive::NoOpFactory factory;
    return factory;
}

static const char* import_result_to_string(rive::ImportResult result) {
    switch (result) {
        case rive::ImportResult::success:
            return "success";
        case rive::ImportResult::unsupportedVersion:
            return "unsupported .riv version";
        case rive::ImportResult::malformed:
            return "malformed .riv data";
    }
    return "unknown import error";
}

std::unique_ptr<RivFile> RivFile::import(const uint8_t* data,
                                         size_t size,
                                         std::string* out_error) {
    rive::ImportResult result = rive::ImportResult::malformed;
    rive::rcp<rive::File> file =
        rive::File::import(rive::Span<const uint8_t>(data, size),
                           &metadata_factory(),
                           &result);
    if (file == nullptr || result != rive::ImportResult::success) {
        if (out_error != nullptr) {
            *out_error = import_result_to_string(result);
        }
        return nullptr;
    }

    std::unique_ptr<RivFile> out(new RivFile());
    out->m_file = std::move(file);

    const size_t artboard_count = out->m_file->artboardCount();
    out->m_artboards.reserve(artboard_count);
    for (size_t i = 0; i < artboard_count; ++i) {
        ArtboardMeta meta;
        meta.name = out->m_file->artboardNameAt(i);

        std::unique_ptr<rive::ArtboardInstance> instance =
            out->m_file->artboardAt(i);
        if (instance != nullptr) {
            const size_t sm_count = instance->stateMachineCount();
            meta.state_machines.reserve(sm_count);
            for (size_t j = 0; j < sm_count; ++j) {
                meta.state_machines.push_back(instance->stateMachineNameAt(j));
            }
            const size_t anim_count = instance->animationCount();
            meta.animations.reserve(anim_count);
            for (size_t j = 0; j < anim_count; ++j) {
                meta.animations.push_back(instance->animationNameAt(j));
            }
        }
        out->m_artboards.push_back(std::move(meta));
    }
    return out;
}

RivFile::~RivFile() = default;

const ArtboardMeta* RivFile::find_artboard(const std::string& name) const {
    for (const ArtboardMeta& meta : m_artboards) {
        if (meta.name == name) {
            return &meta;
        }
    }
    return nullptr;
}

} // namespace rivegd::core

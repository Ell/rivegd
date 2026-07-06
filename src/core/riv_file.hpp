#pragma once

// core/ is Godot-free: it may depend on rive-runtime and the C++ standard
// library only. Enforced by tools/check_layering.sh.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rive/file.hpp"

namespace rivegd::core {

struct ArtboardMeta {
    std::string name;
    std::vector<std::string> state_machines;
    std::vector<std::string> animations;
};

// An imported .riv file plus enumerated metadata.
//
// Phase 0: imports against a metadata-only NoOpFactory (no GPU resources).
// Once the render layer lands, import() gains a Factory parameter and the
// no-op path remains for headless/server builds (GOALS G2.4).
class RivFile {
public:
    static std::unique_ptr<RivFile> import(const uint8_t* data,
                                           size_t size,
                                           std::string* out_error = nullptr);
    ~RivFile();

    const std::vector<ArtboardMeta>& artboards() const { return m_artboards; }
    const ArtboardMeta* find_artboard(const std::string& name) const;

    rive::File* raw() const { return m_file.get(); }

private:
    RivFile() = default;

    rive::rcp<rive::File> m_file;
    std::vector<ArtboardMeta> m_artboards;
};

} // namespace rivegd::core

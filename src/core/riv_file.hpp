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

struct InputMeta {
    enum class Type { boolean, number, trigger };
    std::string name;
    Type type = Type::trigger;
    bool default_bool = false;
    float default_number = 0.0f;
};

struct VmPropertyMeta {
    std::string name;
    std::string type; // "number" | "string" | "boolean" | "color" | "trigger" | "enum"
    std::vector<std::string> enum_values; // filled for type == "enum"
};

struct StateMachineMeta {
    std::string name;
    std::vector<InputMeta> inputs;
};

struct ArtboardMeta {
    std::string name;
    float width = 0.0f;
    float height = 0.0f;
    std::vector<StateMachineMeta> state_machines;
    std::vector<std::string> animations;
    // Scalar properties of the artboard's default view model.
    std::vector<VmPropertyMeta> view_model_properties;

    const StateMachineMeta* find_state_machine(const std::string& sm_name) const;
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

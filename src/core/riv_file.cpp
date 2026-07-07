#include "core/riv_file.hpp"

#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/data_bind/data_values/data_type.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_enum_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_instance_runtime.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
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
            meta.width = instance->width();
            meta.height = instance->height();

            const size_t sm_count = instance->stateMachineCount();
            meta.state_machines.reserve(sm_count);
            for (size_t j = 0; j < sm_count; ++j) {
                StateMachineMeta sm_meta;
                sm_meta.name = instance->stateMachineNameAt(j);

                // Instance the machine (cheap, no-op factory) to enumerate
                // inputs and their defaults through the public API.
                std::unique_ptr<rive::StateMachineInstance> sm =
                    instance->stateMachineAt(j);
                if (sm != nullptr) {
                    const size_t input_count = sm->inputCount();
                    sm_meta.inputs.reserve(input_count);
                    for (size_t k = 0; k < input_count; ++k) {
                        const rive::SMIInput* input = sm->input(k);
                        if (input == nullptr) {
                            continue;
                        }
                        InputMeta input_meta;
                        input_meta.name = input->name();
                        if (const rive::SMIBool* b =
                                sm->getBool(input_meta.name)) {
                            input_meta.type = InputMeta::Type::boolean;
                            input_meta.default_bool = b->value();
                        } else if (const rive::SMINumber* n =
                                       sm->getNumber(input_meta.name)) {
                            input_meta.type = InputMeta::Type::number;
                            input_meta.default_number = n->value();
                        } else {
                            input_meta.type = InputMeta::Type::trigger;
                        }
                        sm_meta.inputs.push_back(std::move(input_meta));
                    }
                }
                meta.state_machines.push_back(std::move(sm_meta));
            }
            rive::ViewModelRuntime* view_model =
                out->m_file->defaultArtboardViewModel(instance.get());
            if (view_model != nullptr) {
                // Temp instance for probing enum option lists.
                rive::rcp<rive::ViewModelInstanceRuntime> probe =
                    view_model->createInstance();
                for (const rive::PropertyData& property :
                     view_model->properties()) {
                    const char* type = nullptr;
                    switch (property.type) {
                        case rive::DataType::number:  type = "number";  break;
                        case rive::DataType::string:  type = "string";  break;
                        case rive::DataType::boolean: type = "boolean"; break;
                        case rive::DataType::color:   type = "color";   break;
                        case rive::DataType::trigger: type = "trigger"; break;
                        case rive::DataType::enumType: type = "enum";   break;
                        case rive::DataType::list: type = "list"; break;
                        case rive::DataType::viewModel:
                            type = "viewModel";
                            break;
                        default: break; // images/artboards: script-only
                    }
                    if (type != nullptr) {
                        VmPropertyMeta property_meta;
                        property_meta.name = property.name;
                        property_meta.type = type;
                        if (property.type == rive::DataType::enumType &&
                            probe != nullptr) {
                            if (auto* e =
                                    probe->propertyEnum(property.name)) {
                                property_meta.enum_values = e->values();
                            }
                        }
                        meta.view_model_properties.push_back(
                            std::move(property_meta));
                    }
                }
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

const StateMachineMeta* ArtboardMeta::find_state_machine(
    const std::string& sm_name) const {
    for (const StateMachineMeta& sm : state_machines) {
        if (sm.name == sm_name) {
            return &sm;
        }
    }
    return nullptr;
}

const ArtboardMeta* RivFile::find_artboard(const std::string& name) const {
    for (const ArtboardMeta& meta : m_artboards) {
        if (meta.name == name) {
            return &meta;
        }
    }
    return nullptr;
}

} // namespace rivegd::core

// Tier 0 unit tests: core/ against real .riv fixtures, no Godot, no GPU.
// Fixtures come from the rive-runtime submodule's own test assets (MIT).
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "core/riv_file.hpp"

#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"

#include <cstdio>
#include <vector>

static std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> bytes;
    FILE* fp = std::fopen(path, "rb");
    REQUIRE(fp != nullptr);
    std::fseek(fp, 0, SEEK_END);
    long size = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    bytes.resize(static_cast<size_t>(size));
    REQUIRE(std::fread(bytes.data(), 1, bytes.size(), fp) == bytes.size());
    std::fclose(fp);
    return bytes;
}

#define FIXTURE(name) ("thirdparty/rive-runtime/tests/unit_tests/assets/" name)

TEST_CASE("garbage bytes fail cleanly with an error message") {
    std::vector<uint8_t> garbage = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    std::string error;
    auto file =
        rivegd::core::RivFile::import(garbage.data(), garbage.size(), &error);
    REQUIRE(file == nullptr);
    REQUIRE(!error.empty());
}

TEST_CASE("empty data fails cleanly") {
    std::string error;
    auto file = rivegd::core::RivFile::import(nullptr, 0, &error);
    REQUIRE(file == nullptr);
}

TEST_CASE("entry.riv imports and enumerates metadata") {
    auto bytes = read_file(FIXTURE("entry.riv"));
    std::string error;
    auto file =
        rivegd::core::RivFile::import(bytes.data(), bytes.size(), &error);
    INFO("import error: " << error);
    REQUIRE(file != nullptr);
    REQUIRE(file->artboards().size() >= 1);
    // Names must be non-empty and findable through the lookup path.
    for (const auto& artboard : file->artboards()) {
        REQUIRE(!artboard.name.empty());
        REQUIRE(file->find_artboard(artboard.name) != nullptr);
    }
}

TEST_CASE("bullet_man.riv has state machines") {
    auto bytes = read_file(FIXTURE("bullet_man.riv"));
    auto file = rivegd::core::RivFile::import(bytes.data(), bytes.size());
    REQUIRE(file != nullptr);

    bool any_state_machine = false;
    for (const auto& artboard : file->artboards()) {
        if (!artboard.state_machines.empty()) {
            any_state_machine = true;
        }
    }
    REQUIRE(any_state_machine);
}

TEST_CASE("light_switch.riv exposes its 'On' bool input in metadata") {
    auto bytes = read_file(FIXTURE("light_switch.riv"));
    auto file = rivegd::core::RivFile::import(bytes.data(), bytes.size());
    REQUIRE(file != nullptr);
    REQUIRE(!file->artboards().empty());

    const auto& artboard = file->artboards()[0];
    REQUIRE(artboard.width > 0.0f);
    REQUIRE(!artboard.state_machines.empty());

    bool found_on = false;
    for (const auto& sm : artboard.state_machines) {
        for (const auto& input : sm.inputs) {
            if (input.name == "On" &&
                input.type == rivegd::core::InputMeta::Type::boolean) {
                found_on = true;
            }
        }
    }
    REQUIRE(found_on);
}

TEST_CASE("data_binding_test.riv exposes view model properties in metadata") {
    auto bytes = read_file(FIXTURE("data_binding_test.riv"));
    auto file = rivegd::core::RivFile::import(bytes.data(), bytes.size());
    REQUIRE(file != nullptr);

    const auto* artboard = file->find_artboard("artboard-1");
    REQUIRE(artboard != nullptr);
    bool found_width = false;
    for (const auto& property : artboard->view_model_properties) {
        if (property.name == "width" && property.type == "number") {
            found_width = true;
        }
    }
    REQUIRE(found_width);
}

TEST_CASE("enum view model properties carry their option lists") {
    auto bytes = read_file(FIXTURE("data_binding_test.riv"));
    auto file = rivegd::core::RivFile::import(bytes.data(), bytes.size());
    REQUIRE(file != nullptr);

    const auto* artboard = file->find_artboard("artboard-2");
    REQUIRE(artboard != nullptr);
    bool found_enum = false;
    bool found_trigger = false;
    for (const auto& property : artboard->view_model_properties) {
        if (property.name == "state" && property.type == "enum") {
            found_enum = true;
            REQUIRE(property.enum_values ==
                    std::vector<std::string>{"state-red", "state-green",
                                             "state-blue"});
        }
        if (property.name == "trigger-prop" && property.type == "trigger") {
            found_trigger = true;
        }
    }
    REQUIRE(found_enum);
    REQUIRE(found_trigger);
}

TEST_CASE("headless advance: instance a state machine and run it") {
    auto bytes = read_file(FIXTURE("bullet_man.riv"));
    auto file = rivegd::core::RivFile::import(bytes.data(), bytes.size());
    REQUIRE(file != nullptr);

    auto artboard = file->raw()->artboardDefault();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->stateMachineCount() >= 1);

    auto machine = artboard->stateMachineAt(0);
    REQUIRE(machine != nullptr);

    // One minute of 60fps advances: must not crash, must keep reporting.
    for (int i = 0; i < 3600; ++i) {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    SUCCEED("state machine advanced 3600 frames headless");
}

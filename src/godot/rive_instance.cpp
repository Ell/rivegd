#include "godot/rive_instance.h"

#include "godot/rive_render_server.h"

#include "rive/command_queue.hpp"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

#include <tuple>
#include <godot_cpp/classes/texture2d.hpp>

using namespace godot;

namespace rivegd {

#define RIVEGD_POST(METHOD, ...)                                              \
    do {                                                                       \
        RiveRenderServer* server = RiveRenderServer::get_singleton();          \
        if (server == nullptr || instance_id == 0) {                           \
            return;                                                            \
        }                                                                      \
        const int64_t rivegd_id = instance_id;                                 \
        server->request_pump();                                                \
        server->queue()->runOnce(                                              \
            [server, rivegd_id, args = std::make_tuple(__VA_ARGS__)](          \
                rive::CommandServer*) {                                        \
                std::apply(                                                    \
                    [server, rivegd_id](auto&&... a) {                         \
                        server->METHOD(rivegd_id,                              \
                                       std::forward<decltype(a)>(a)...);       \
                    },                                                         \
                    args);                                                     \
            });                                                                \
    } while (0)

static const char* kInputPrefix = "inputs/";
static const char* kPropertyPrefix = "data_binding/";

void RiveInstance::create(const Vector2i& p_size) {
    release();
    if (file.is_null() || !file->is_valid()) {
        return;
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr) {
        return;
    }
    instance_id = server->allocate_instance_id();

    // M1: lifecycle through CommandQueue handles. The queue is one FIFO, so
    // loadFile -> instantiate -> our init closure -> replayed values keep
    // their order; the server pumps it on the render thread each frame.
    rive::CommandQueue* queue = server->queue();
    const PackedByteArray bytes = file->get_data();
    file_handle = reinterpret_cast<uint64_t>(queue->loadFile(
        std::vector<uint8_t>(bytes.ptr(), bytes.ptr() + bytes.size())));
    artboard_handle = reinterpret_cast<uint64_t>(queue->instantiateArtboardNamed(
        reinterpret_cast<rive::FileHandle>(file_handle),
        artboard.utf8().get_data()));
    // Empty name historically meant "first state machine"; the queue's ""
    // means the artboard's *designated* default, which many files lack.
    // Resolve the first machine's name from metadata to keep the old
    // semantics deterministic.
    String machine_name = state_machine;
    if (machine_name.is_empty()) {
        const PackedStringArray machines =
            file->get_state_machine_names(artboard);
        if (!machines.is_empty()) {
            machine_name = machines[0];
        }
    }
    state_machine_handle =
        reinterpret_cast<uint64_t>(queue->instantiateStateMachineNamed(
            reinterpret_cast<rive::ArtboardHandle>(artboard_handle),
            machine_name.utf8().get_data()));
    {
        const int64_t id = instance_id;
        const uint64_t fh = file_handle;
        const uint64_t ah = artboard_handle;
        const uint64_t sh = state_machine_handle;
        const Vector2i size = p_size;
        queue->runOnce([server, id, fh, ah, sh, size](rive::CommandServer*) {
            server->rt_init_instance(id, fh, ah, sh, size);
        });
        server->request_pump();
    }
    // Replay inspector/script-set inputs and view-model properties onto the
    // fresh instance.
    for (const KeyValue<String, Variant>& entry : input_values) {
        post_input(entry.key, entry.value);
    }
    for (const KeyValue<String, Variant>& entry : property_values) {
        post_property(entry.key, entry.value);
    }
    for (const KeyValue<String, bool>& entry : watched_paths) {
        post_watch(entry.key);
    }
    texture_bound = false;
}

void RiveInstance::release() {
    if (instance_id == 0) {
        return;
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server != nullptr) {
        rive::CommandQueue* queue = server->queue();
        const int64_t id = instance_id;
        queue->runOnce([server, id](rive::CommandServer*) {
            server->rt_free_instance(id);
        });
        // Object teardown strictly after our last use (FIFO).
        queue->deleteStateMachine(
            reinterpret_cast<rive::StateMachineHandle>(state_machine_handle));
        queue->deleteArtboard(
            reinterpret_cast<rive::ArtboardHandle>(artboard_handle));
        queue->deleteFile(reinterpret_cast<rive::FileHandle>(file_handle));
        server->request_pump();
    }
    instance_id = 0;
    file_handle = 0;
    artboard_handle = 0;
    state_machine_handle = 0;
    texture_bound = false;
    canvas_texture = RID();
    rd_texture = RID();
}

void RiveInstance::frame(double p_delta) {
    if (instance_id == 0) {
        return;
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    const int64_t id = instance_id;
    server->queue()->runOnce([server, id, p_delta](rive::CommandServer*) {
        server->rt_frame(id, p_delta);
    });
    server->request_pump();
}

bool RiveInstance::update_texture_binding() {
    if (instance_id == 0 || texture_bound) {
        return false;
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    RID rid = server->get_canvas_texture_rid(instance_id);
    if (!rid.is_valid()) {
        return false;
    }
    canvas_texture = rid;
    rd_texture = server->get_texture_rid(instance_id);
    texture_bound = true;
    return true;
}

Array RiveInstance::take_events() {
    if (instance_id == 0) {
        return Array();
    }
    return RiveRenderServer::get_singleton()->take_events(instance_id);
}

Array RiveInstance::take_state_changes() {
    if (instance_id == 0) {
        return Array();
    }
    return RiveRenderServer::get_singleton()->take_state_changes(instance_id);
}

void RiveInstance::post_property(const String& p_path, const Variant& p_value) {
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    switch (p_value.get_type()) {
        case Variant::BOOL:
            RIVEGD_POST(rt_set_vm_bool, p_path, bool(p_value));
            break;
        case Variant::INT:
        case Variant::FLOAT:
            RIVEGD_POST(rt_set_vm_number, p_path, double(p_value));
            break;
        case Variant::STRING:
        case Variant::STRING_NAME:
            RIVEGD_POST(rt_set_vm_string, p_path, String(p_value));
            break;
        case Variant::COLOR:
            RIVEGD_POST(rt_set_vm_color, p_path, Color(p_value));
            break;
        case Variant::OBJECT: {
            // Image properties accept a Godot Image or Texture2D; encode to
            // PNG here (main thread) and decode through the context factory
            // on the render thread.
            Ref<Image> image = p_value;
            if (image.is_null()) {
                Ref<Texture2D> texture = p_value;
                if (texture.is_valid()) {
                    image = texture->get_image();
                }
            }
            if (image.is_null()) {
                ERR_PRINT("rivegd: expected an Image or Texture2D for '" +
                          p_path + "'");
                return;
            }
            RIVEGD_POST(rt_set_vm_image, p_path, image->save_png_to_buffer());
        } break;
        default:
            ERR_PRINT("rivegd: unsupported view-model property type for '" +
                      p_path + "'");
            break;
    }
}

void RiveInstance::set_property(const String& p_path, const Variant& p_value) {
    property_values[p_path] = p_value;
    post_property(p_path, p_value);
}

void RiveInstance::post_watch(const String& p_path) {
    RIVEGD_POST(rt_watch_vm_property, p_path);
}

void RiveInstance::watch_property(const String& p_path) {
    watched_paths[p_path] = true;
    post_watch(p_path);
}

Variant RiveInstance::get_property(const String& p_path) const {
    const Variant* cached = property_cache.getptr(p_path);
    if (cached != nullptr) {
        return *cached;
    }
    const Variant* set_value = property_values.getptr(p_path);
    return set_value != nullptr ? *set_value : Variant();
}

Array RiveInstance::take_property_changes() {
    if (instance_id == 0) {
        return Array();
    }
    Array changes =
        RiveRenderServer::get_singleton()->take_property_changes(instance_id);
    for (int i = 0; i < changes.size(); ++i) {
        Dictionary change = changes[i];
        property_cache[change["path"]] = change["value"];
    }
    return changes;
}



void RiveInstance::key(int p_rive_key, int p_modifiers, bool p_pressed,
                       bool p_repeat) {
    RIVEGD_POST(rt_key, p_rive_key, p_modifiers, p_pressed, p_repeat);
}

void RiveInstance::text_input(const String& p_text) {
    RIVEGD_POST(rt_text_input, p_text);
}

void RiveInstance::focus_move(int p_direction) {
    RIVEGD_POST(rt_focus_move, p_direction);
}

void RiveInstance::gamepads(const PackedByteArray& p_batch) {
    RIVEGD_POST(rt_gamepads, p_batch);
}

void RiveInstance::list_append(const String& p_path, const String& p_view_model,
                               const String& p_instance_name) {
    RIVEGD_POST(rt_list_append, p_path, p_view_model, p_instance_name);
}

void RiveInstance::list_remove_at(const String& p_path, int p_index) {
    RIVEGD_POST(rt_list_remove_at, p_path, p_index);
}

void RiveInstance::list_swap(const String& p_path, int p_a, int p_b) {
    RIVEGD_POST(rt_list_swap, p_path, p_a, p_b);
}

void RiveInstance::list_clear(const String& p_path) {
    RIVEGD_POST(rt_list_clear, p_path);
}

void RiveInstance::list_set_property(const String& p_path, int p_index,
                                     const String& p_sub_path,
                                     const Variant& p_value) {
    RIVEGD_POST(rt_list_set, p_path, p_index, p_sub_path, p_value);
}

void RiveInstance::set_artboard_property(const String& p_path,
                                         const String& p_artboard_name) {
    RIVEGD_POST(rt_set_vm_artboard, p_path, p_artboard_name);
}

void RiveInstance::replace_view_model(const String& p_path,
                                      const String& p_view_model,
                                      const String& p_instance_name) {
    RIVEGD_POST(rt_replace_view_model, p_path, p_view_model, p_instance_name);
}

void RiveInstance::fire_property_trigger(const String& p_path) {
    RIVEGD_POST(rt_fire_vm_trigger, p_path);
}

void RiveInstance::post_input(const String& p_name, const Variant& p_value) {
    switch (p_value.get_type()) {
        case Variant::BOOL:
            RIVEGD_POST(rt_set_bool, p_name, bool(p_value));
            break;
        case Variant::FLOAT:
        case Variant::INT:
            RIVEGD_POST(rt_set_number, p_name, double(p_value));
            break;
        default:
            break;
    }
}

void RiveInstance::set_bool_input(const String& p_name, bool p_value) {
    input_values[p_name] = p_value;
    post_input(p_name, p_value);
}

void RiveInstance::set_number_input(const String& p_name, double p_value) {
    input_values[p_name] = p_value;
    post_input(p_name, p_value);
}

void RiveInstance::fire_trigger(const String& p_name) {
    RIVEGD_POST(rt_fire_trigger, p_name);
}

void RiveInstance::pointer(int p_phase, const Vector2& p_local,
                           const Vector2& p_node_size) {
    RIVEGD_POST(rt_pointer, p_phase, p_local, p_node_size);
}

void RiveInstance::get_property_list(List<PropertyInfo>* p_list) const {
    if (file.is_null() || !file->is_valid()) {
        return;
    }
    Array inputs = file->get_input_descriptions(artboard, state_machine);
    if (!inputs.is_empty()) {
        p_list->push_back(PropertyInfo(Variant::NIL, "Inputs",
                                       PROPERTY_HINT_NONE, "inputs/",
                                       PROPERTY_USAGE_GROUP));
    }
    for (int i = 0; i < inputs.size(); ++i) {
        Dictionary description = inputs[i];
        const String name = description["name"];
        const String type = description["type"];
        const String property_name = String(kInputPrefix) + name;
        if (type == "bool") {
            p_list->push_back(
                PropertyInfo(Variant::BOOL, property_name));
        } else if (type == "number") {
            p_list->push_back(
                PropertyInfo(Variant::FLOAT, property_name));
        }
        // Triggers are actions, not values; they stay script-only until the
        // editor inspector plugin adds fire buttons (Phase 2b).
    }

    Array vm_properties = file->get_property_descriptions(artboard);
    bool group_added = false;
    for (int i = 0; i < vm_properties.size(); ++i) {
        Dictionary description = vm_properties[i];
        const String type = description["type"];
        Variant::Type variant_type = Variant::NIL;
        PropertyHint hint = PROPERTY_HINT_NONE;
        String hint_string;
        if (type == "number") {
            variant_type = Variant::FLOAT;
        } else if (type == "boolean") {
            variant_type = Variant::BOOL;
        } else if (type == "string") {
            variant_type = Variant::STRING;
        } else if (type == "color") {
            variant_type = Variant::COLOR;
        } else if (type == "enum" && description.has("enum_values")) {
            variant_type = Variant::STRING;
            hint = PROPERTY_HINT_ENUM;
            hint_string = String(",").join(description["enum_values"]);
        } else {
            continue; // triggers/lists/images/nested: script-only for now
        }
        if (!group_added) {
            p_list->push_back(PropertyInfo(Variant::NIL, "Data Binding",
                                           PROPERTY_HINT_NONE, kPropertyPrefix,
                                           PROPERTY_USAGE_GROUP));
            group_added = true;
        }
        p_list->push_back(PropertyInfo(
            variant_type, String(kPropertyPrefix) + String(description["name"]),
            hint, hint_string));
    }
}

bool RiveInstance::try_set(const StringName& p_name, const Variant& p_value) {
    const String name = p_name;
    if (name.begins_with(kPropertyPrefix)) {
        set_property(name.trim_prefix(kPropertyPrefix), p_value);
        return true;
    }
    if (!name.begins_with(kInputPrefix)) {
        return false;
    }
    const String input_name = name.trim_prefix(kInputPrefix);
    if (p_value.get_type() == Variant::BOOL) {
        set_bool_input(input_name, p_value);
    } else {
        set_number_input(input_name, p_value);
    }
    return true;
}

bool RiveInstance::try_get(const StringName& p_name, Variant& r_value) const {
    const String name = p_name;
    if (name.begins_with(kPropertyPrefix)) {
        r_value = get_property(name.trim_prefix(kPropertyPrefix));
        return true;
    }
    if (!name.begins_with(kInputPrefix)) {
        return false;
    }
    const String input_name = name.trim_prefix(kInputPrefix);
    const Variant* cached = input_values.getptr(input_name);
    if (cached != nullptr) {
        r_value = *cached;
        return true;
    }
    // Fall back to the file's declared default.
    if (file.is_valid() && file->is_valid()) {
        Array inputs = file->get_input_descriptions(artboard, state_machine);
        for (int i = 0; i < inputs.size(); ++i) {
            Dictionary description = inputs[i];
            if (String(description["name"]) == input_name) {
                r_value = description.get("default", Variant());
                return true;
            }
        }
    }
    r_value = Variant();
    return true;
}

void RiveInstance::validate_property(PropertyInfo& p_property) const {
    if (file.is_null() || !file->is_valid()) {
        return;
    }
    // Turn artboard/state_machine into dropdowns populated from the file.
    if (p_property.name == StringName("artboard")) {
        p_property.hint = PROPERTY_HINT_ENUM;
        p_property.hint_string =
            String(",").join(file->get_artboard_names());
    } else if (p_property.name == StringName("state_machine")) {
        p_property.hint = PROPERTY_HINT_ENUM;
        p_property.hint_string =
            String(",").join(file->get_state_machine_names(artboard));
    }
}

} // namespace rivegd

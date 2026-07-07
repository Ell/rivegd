#include "godot/rive_instance.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/classes/rendering_server.hpp>

using namespace godot;

namespace rivegd {

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
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_init_instance)
            .bind(instance_id, file->get_data(), artboard, state_machine,
                  p_size));
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
        RenderingServer::get_singleton()->call_on_render_thread(
            callable_mp(server, &RiveRenderServer::rt_free_instance)
                .bind(instance_id));
    }
    instance_id = 0;
    texture_bound = false;
    canvas_texture = RID();
    rd_texture = RID();
}

void RiveInstance::frame(double p_delta) {
    if (instance_id == 0) {
        return;
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_frame)
            .bind(instance_id, p_delta));
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
    RenderingServer* rs = RenderingServer::get_singleton();
    switch (p_value.get_type()) {
        case Variant::BOOL:
            rs->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_vm_bool)
                    .bind(instance_id, p_path, bool(p_value)));
            break;
        case Variant::INT:
        case Variant::FLOAT:
            rs->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_vm_number)
                    .bind(instance_id, p_path, double(p_value)));
            break;
        case Variant::STRING:
        case Variant::STRING_NAME:
            rs->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_vm_string)
                    .bind(instance_id, p_path, String(p_value)));
            break;
        case Variant::COLOR:
            rs->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_vm_color)
                    .bind(instance_id, p_path, Color(p_value)));
            break;
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
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_watch_vm_property)
            .bind(instance_id, p_path));
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

void RiveInstance::fire_property_trigger(const String& p_path) {
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_fire_vm_trigger)
            .bind(instance_id, p_path));
}

void RiveInstance::post_input(const String& p_name, const Variant& p_value) {
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    switch (p_value.get_type()) {
        case Variant::BOOL:
            RenderingServer::get_singleton()->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_bool)
                    .bind(instance_id, p_name, bool(p_value)));
            break;
        case Variant::FLOAT:
        case Variant::INT:
            RenderingServer::get_singleton()->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_set_number)
                    .bind(instance_id, p_name, double(p_value)));
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
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_fire_trigger)
            .bind(instance_id, p_name));
}

void RiveInstance::pointer(int p_phase, const Vector2& p_local,
                           const Vector2& p_node_size) {
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr || instance_id == 0) {
        return;
    }
    RenderingServer::get_singleton()->call_on_render_thread(
        callable_mp(server, &RiveRenderServer::rt_pointer)
            .bind(instance_id, p_phase, p_local, p_node_size));
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
        if (type == "number") {
            variant_type = Variant::FLOAT;
        } else if (type == "boolean") {
            variant_type = Variant::BOOL;
        } else if (type == "string") {
            variant_type = Variant::STRING;
        } else if (type == "color") {
            variant_type = Variant::COLOR;
        } else {
            continue; // triggers/enums/nested: script-only for now
        }
        if (!group_added) {
            p_list->push_back(PropertyInfo(Variant::NIL, "Data Binding",
                                           PROPERTY_HINT_NONE, kPropertyPrefix,
                                           PROPERTY_USAGE_GROUP));
            group_added = true;
        }
        p_list->push_back(PropertyInfo(
            variant_type, String(kPropertyPrefix) + String(description["name"])));
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

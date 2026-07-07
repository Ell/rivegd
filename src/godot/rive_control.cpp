#include "godot/rive_control.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

void RiveControl::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file", "file"), &RiveControl::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &RiveControl::get_file);
    ClassDB::bind_method(D_METHOD("set_artboard", "artboard"),
                         &RiveControl::set_artboard);
    ClassDB::bind_method(D_METHOD("get_artboard"), &RiveControl::get_artboard);
    ClassDB::bind_method(D_METHOD("set_state_machine", "state_machine"),
                         &RiveControl::set_state_machine);
    ClassDB::bind_method(D_METHOD("get_state_machine"),
                         &RiveControl::get_state_machine);
    ClassDB::bind_method(D_METHOD("set_playing", "playing"),
                         &RiveControl::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &RiveControl::is_playing);
    ClassDB::bind_method(D_METHOD("set_pause_when_hidden", "pause"),
                         &RiveControl::set_pause_when_hidden);
    ClassDB::bind_method(D_METHOD("get_pause_when_hidden"),
                         &RiveControl::get_pause_when_hidden);
    ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"),
                         &RiveControl::set_speed_scale);
    ClassDB::bind_method(D_METHOD("get_speed_scale"),
                         &RiveControl::get_speed_scale);
    ClassDB::bind_method(D_METHOD("set_bool_input", "name", "value"),
                         &RiveControl::set_bool_input);
    ClassDB::bind_method(D_METHOD("set_number_input", "name", "value"),
                         &RiveControl::set_number_input);
    ClassDB::bind_method(D_METHOD("fire_trigger", "name"),
                         &RiveControl::fire_trigger);
    ClassDB::bind_method(D_METHOD("set_property", "path", "value"),
                         &RiveControl::set_property);
    ClassDB::bind_method(D_METHOD("fire_property_trigger", "path"),
                         &RiveControl::fire_property_trigger);
    ClassDB::bind_method(D_METHOD("watch_property", "path"),
                         &RiveControl::watch_property);
    ClassDB::bind_method(D_METHOD("get_property", "path"),
                         &RiveControl::get_property);
    ClassDB::bind_method(
        D_METHOD("list_append", "path", "view_model", "instance_name"),
        &RiveControl::list_append, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("list_remove_at", "path", "index"),
                         &RiveControl::list_remove_at);
    ClassDB::bind_method(D_METHOD("list_swap", "path", "a", "b"),
                         &RiveControl::list_swap);
    ClassDB::bind_method(D_METHOD("list_clear", "path"), &RiveControl::list_clear);
    ClassDB::bind_method(
        D_METHOD("list_set_property", "path", "index", "sub_path", "value"),
        &RiveControl::list_set_property);
    ClassDB::bind_method(
        D_METHOD("list_read_property", "path", "index", "sub_path"),
        &RiveControl::list_read_property);
    ClassDB::bind_method(
        D_METHOD("set_artboard_property", "path", "artboard_name"),
        &RiveControl::set_artboard_property);
    ClassDB::bind_method(D_METHOD("set_gamepad_enabled", "enabled"),
                         &RiveControl::set_gamepad_enabled);
    ClassDB::bind_method(D_METHOD("get_gamepad_enabled"),
                         &RiveControl::get_gamepad_enabled);
    ClassDB::bind_method(D_METHOD("submit_gamepad_batch", "batch"),
                         &RiveControl::submit_gamepad_batch);
    ClassDB::bind_method(D_METHOD("send_pointer_event", "phase", "position"),
                         &RiveControl::send_pointer_event);
    ClassDB::bind_method(D_METHOD("focus_next_element"),
                         &RiveControl::focus_next_element);
    ClassDB::bind_method(D_METHOD("focus_previous_element"),
                         &RiveControl::focus_previous_element);
    ClassDB::bind_method(D_METHOD("send_text_input", "text"),
                         &RiveControl::send_text_input);
    ClassDB::bind_method(
        D_METHOD("replace_view_model", "path", "view_model", "instance_name"),
        &RiveControl::replace_view_model, DEFVAL(String()));

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file",
                              PROPERTY_HINT_RESOURCE_TYPE, "RiveFileResource"),
                 "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard"), "set_artboard",
                 "get_artboard");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine"),
                 "set_state_machine", "get_state_machine");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing",
                 "is_playing");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pause_when_hidden"),
                 "set_pause_when_hidden", "get_pause_when_hidden");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gamepad_enabled"),
                 "set_gamepad_enabled", "get_gamepad_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale",
                              PROPERTY_HINT_RANGE, "0,4,0.01,or_greater"),
                 "set_speed_scale", "get_speed_scale");

    ADD_SIGNAL(MethodInfo("rive_event",
                          PropertyInfo(Variant::STRING, "name"),
                          PropertyInfo(Variant::DICTIONARY, "properties")));
    ADD_SIGNAL(MethodInfo("state_changed",
                          PropertyInfo(Variant::STRING, "state_name")));
    ADD_SIGNAL(MethodInfo("property_changed",
                          PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::NIL, "value")));
}

void RiveControl::set_file(const Ref<RiveFileResource>& p_file) {
    // Hot reload: re-imports emit changed on the resource.
    Callable reload = callable_mp(this, &RiveControl::recreate_instance);
    if (rive.file.is_valid() && rive.file->is_connected("changed", reload)) {
        rive.file->disconnect("changed", reload);
    }
    rive.file = p_file;
    if (rive.file.is_valid()) {
        rive.file->connect("changed", reload);
    }
    recreate_instance();
    update_minimum_size();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveControl::set_artboard(const String& p_artboard) {
    rive.artboard = p_artboard;
    recreate_instance();
    update_minimum_size();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveControl::set_state_machine(const String& p_state_machine) {
    rive.state_machine = p_state_machine;
    recreate_instance();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveControl::set_playing(bool p_playing) {
    playing = p_playing;
    set_process(playing && rive.is_live());
}

void RiveControl::set_pause_when_hidden(bool p_pause) {
    pause_when_hidden = p_pause;
}

void RiveControl::set_speed_scale(double p_speed_scale) {
    speed_scale = MAX(0.0, p_speed_scale);
}

void RiveControl::set_bool_input(const String& p_name, bool p_value) {
    rive.set_bool_input(p_name, p_value);
}

void RiveControl::set_number_input(const String& p_name, double p_value) {
    rive.set_number_input(p_name, p_value);
}

void RiveControl::fire_trigger(const String& p_name) {
    rive.fire_trigger(p_name);
}

void RiveControl::set_property(const String& p_path, const Variant& p_value) {
    rive.set_property(p_path, p_value);
}

void RiveControl::fire_property_trigger(const String& p_path) {
    rive.fire_property_trigger(p_path);
}

void RiveControl::watch_property(const String& p_path) {
    rive.watch_property(p_path);
}

Variant RiveControl::get_property(const String& p_path) const {
    return rive.get_property(p_path);
}

void RiveControl::list_append(const String& p_path, const String& p_view_model,
                        const String& p_instance_name) {
    rive.list_append(p_path, p_view_model, p_instance_name);
}

void RiveControl::list_remove_at(const String& p_path, int p_index) {
    rive.list_remove_at(p_path, p_index);
}

void RiveControl::list_swap(const String& p_path, int p_a, int p_b) {
    rive.list_swap(p_path, p_a, p_b);
}

void RiveControl::list_clear(const String& p_path) {
    rive.list_clear(p_path);
}

void RiveControl::list_set_property(const String& p_path, int p_index,
                              const String& p_sub_path,
                              const Variant& p_value) {
    rive.list_set_property(p_path, p_index, p_sub_path, p_value);
}

void RiveControl::list_read_property(const String& p_path, int p_index,
                               const String& p_sub_path) {
    rive.list_read_property(p_path, p_index, p_sub_path);
}

void RiveControl::set_artboard_property(const String& p_path,
                                  const String& p_artboard_name) {
    rive.set_artboard_property(p_path, p_artboard_name);
}

void RiveControl::replace_view_model(const String& p_path, const String& p_view_model,
                               const String& p_instance_name) {
    rive.replace_view_model(p_path, p_view_model, p_instance_name);
}

void RiveControl::set_gamepad_enabled(bool p_enabled) {
    gamepad_enabled = p_enabled;
}

void RiveControl::submit_gamepad_batch(const PackedByteArray& p_batch) {
    rive.gamepads(p_batch);
}

void RiveControl::send_pointer_event(int p_phase, const Vector2& p_position) {
    if (rive.is_live()) {
        rive.pointer(p_phase, p_position, get_size());
    }
}

void RiveControl::focus_next_element() { rive.focus_move(0); }

void RiveControl::focus_previous_element() { rive.focus_move(1); }

void RiveControl::send_text_input(const String& p_text) {
    rive.text_input(p_text);
}

Vector2 RiveControl::_get_minimum_size() const {
    // A Rive control can shrink; the artboard scales. Keep a small floor so
    // empty controls remain clickable in the editor.
    return Vector2(8, 8);
}

Vector2i RiveControl::texture_size() const {
    const Vector2 control_size = get_size();
    return Vector2i(MAX(8, int(control_size.x)), MAX(8, int(control_size.y)));
}

void RiveControl::recreate_instance() {
    rive.release();
    if (!is_inside_tree()) {
        return;
    }
    live_texture_size = texture_size();
    rive.create(live_texture_size);
    set_process(playing && rive.is_live());
    queue_redraw();
}

// Godot keycode -> rive's GLFW-style Key value (0 = unmapped).
static int godot_key_to_rive(Key p_key) {
    // Printable ASCII range matches directly (Godot KEY_A..KEY_Z == 65..90,
    // digits/punctuation likewise).
    const int code = int(p_key);
    if (code >= 32 && code <= 96) {
        return code;
    }
    switch (p_key) {
        case KEY_ESCAPE: return 256;
        case KEY_ENTER: return 257;
        case KEY_KP_ENTER: return 335;
        case KEY_TAB: return 258;
        case KEY_BACKSPACE: return 259;
        case KEY_INSERT: return 260;
        case KEY_DELETE: return 261;
        case KEY_RIGHT: return 262;
        case KEY_LEFT: return 263;
        case KEY_DOWN: return 264;
        case KEY_UP: return 265;
        case KEY_PAGEUP: return 266;
        case KEY_PAGEDOWN: return 267;
        case KEY_HOME: return 268;
        case KEY_END: return 269;
        case KEY_CAPSLOCK: return 280;
        case KEY_SHIFT: return 340;      // leftShift
        case KEY_CTRL: return 341;       // leftControl
        case KEY_ALT: return 342;        // leftAlt
        case KEY_META: return 343;       // leftSuper
        default:
            if (p_key >= KEY_F1 && p_key <= KEY_F12) {
                return 290 + (int(p_key) - int(KEY_F1));
            }
            return 0;
    }
}

static int godot_modifiers_to_rive(const Ref<InputEventWithModifiers>& p_event) {
    int modifiers = 0;
    if (p_event->is_shift_pressed()) modifiers |= 1;  // shift
    if (p_event->is_ctrl_pressed()) modifiers |= 2;   // ctrl
    if (p_event->is_alt_pressed()) modifiers |= 4;    // alt
    if (p_event->is_meta_pressed()) modifiers |= 8;   // meta
    return modifiers;
}

void RiveControl::_gui_input(const Ref<InputEvent>& p_event) {
    if (!rive.is_live()) {
        return;
    }
    Ref<InputEventKey> key_event = p_event;
    if (key_event.is_valid()) {
        const int rive_key = godot_key_to_rive(key_event->get_keycode());
        if (rive_key != 0) {
            rive.key(rive_key, godot_modifiers_to_rive(key_event),
                     key_event->is_pressed(), key_event->is_echo());
            accept_event();
        }
        // Printable characters also flow as text input (Rive text fields).
        if (key_event->is_pressed() && key_event->get_unicode() >= 32) {
            rive.text_input(String::chr(key_event->get_unicode()));
        }
        return;
    }
    Ref<InputEventMouseMotion> motion = p_event;
    if (motion.is_valid()) {
        rive.pointer(RiveRenderServer::POINTER_MOVE, motion->get_position(),
                     get_size());
        return;
    }
    Ref<InputEventMouseButton> button = p_event;
    if (button.is_valid() && button->get_button_index() == MOUSE_BUTTON_LEFT) {
        rive.pointer(button->is_pressed() ? RiveRenderServer::POINTER_DOWN
                                          : RiveRenderServer::POINTER_UP,
                     button->get_position(), get_size());
        accept_event();
    }
}

PackedStringArray RiveControl::_get_configuration_warnings() const {
    PackedStringArray warnings;
    if (rive.file.is_null()) {
        warnings.push_back("Assign a RiveFileResource (.riv) to play.");
        return warnings;
    }
    if (!rive.file->is_valid()) {
        warnings.push_back("The .riv file failed to load: " +
                           rive.file->get_import_error());
        return warnings;
    }
    if (!rive.artboard.is_empty() &&
        !rive.file->get_artboard_names().has(rive.artboard)) {
        warnings.push_back("Artboard \"" + rive.artboard +
                           "\" does not exist in this file.");
    }
    if (!rive.state_machine.is_empty() &&
        !rive.file->get_state_machine_names(rive.artboard)
                 .has(rive.state_machine)) {
        warnings.push_back("State machine \"" + rive.state_machine +
                           "\" does not exist on this artboard.");
    }
    return warnings;
}

void RiveControl::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            recreate_instance();
        } break;
        case NOTIFICATION_EXIT_TREE: {
            rive.release();
            set_process(false);
        } break;
        case NOTIFICATION_RESIZED: {
            resize_cooldown = kResizeDebounceSeconds;
        } break;
        case NOTIFICATION_MOUSE_EXIT: {
            if (rive.is_live()) {
                rive.pointer(RiveRenderServer::POINTER_EXIT, Vector2(),
                             get_size());
            }
        } break;
        case NOTIFICATION_PROCESS: {
            if (resize_cooldown > 0.0) {
                resize_cooldown -= get_process_delta_time();
                if (resize_cooldown <= 0.0 &&
                    texture_size() != live_texture_size) {
                    recreate_instance();
                    break;
                }
            }
            if (pause_when_hidden && !is_visible_in_tree()) {
                break; // GOALS G4.6: hidden instances stop advancing
            }
            if (gamepad_enabled) {
                PackedByteArray batch = gamepad_encoder.encode_frame();
                if (!batch.is_empty()) {
                    rive.gamepads(batch);
                }
            }
            rive.frame(get_process_delta_time() * speed_scale);
            if (rive.update_texture_binding()) {
                queue_redraw();
            }
            Array events = rive.take_events();
            for (int i = 0; i < events.size(); ++i) {
                Dictionary event = events[i];
                emit_signal("rive_event", event["name"], event["properties"]);
            }
            Array states = rive.take_state_changes();
            for (int i = 0; i < states.size(); ++i) {
                emit_signal("state_changed", states[i]);
            }
            Array changes = rive.take_property_changes();
            for (int i = 0; i < changes.size(); ++i) {
                Dictionary change = changes[i];
                emit_signal("property_changed", change["path"],
                            change["value"]);
            }
        } break;
        case NOTIFICATION_DRAW: {
            if (rive.get_canvas_texture_rid().is_valid()) {
                RenderingServer::get_singleton()->canvas_item_add_texture_rect(
                    get_canvas_item(), Rect2(Vector2(), get_size()),
                    rive.get_canvas_texture_rid());
            }
        } break;
    }
}

bool RiveControl::_set(const StringName& p_name, const Variant& p_value) {
    return rive.try_set(p_name, p_value);
}

bool RiveControl::_get(const StringName& p_name, Variant& r_value) const {
    return rive.try_get(p_name, r_value);
}

void RiveControl::_get_property_list(List<PropertyInfo>* p_list) const {
    rive.get_property_list(p_list);
}

void RiveControl::_validate_property(PropertyInfo& p_property) const {
    rive.validate_property(p_property);
}

} // namespace rivegd

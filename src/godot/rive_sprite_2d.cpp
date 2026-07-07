#include "godot/rive_sprite_2d.h"

#include "godot/rive_audio_stream.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

void RiveSprite2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file", "file"), &RiveSprite2D::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &RiveSprite2D::get_file);
    ClassDB::bind_method(D_METHOD("set_artboard", "artboard"),
                         &RiveSprite2D::set_artboard);
    ClassDB::bind_method(D_METHOD("get_artboard"), &RiveSprite2D::get_artboard);
    ClassDB::bind_method(D_METHOD("set_audio_bus", "bus"),
                         &RiveSprite2D::set_audio_bus);
    ClassDB::bind_method(D_METHOD("get_audio_bus"), &RiveSprite2D::get_audio_bus);
    ClassDB::bind_method(D_METHOD("set_fit", "fit"), &RiveSprite2D::set_fit);
    ClassDB::bind_method(D_METHOD("get_fit"), &RiveSprite2D::get_fit);
    ClassDB::bind_method(D_METHOD("set_alignment", "alignment"),
                         &RiveSprite2D::set_alignment);
    ClassDB::bind_method(D_METHOD("get_alignment"), &RiveSprite2D::get_alignment);
    ClassDB::bind_method(D_METHOD("set_layout_scale", "scale"),
                         &RiveSprite2D::set_layout_scale);
    ClassDB::bind_method(D_METHOD("get_layout_scale"),
                         &RiveSprite2D::get_layout_scale);
    ClassDB::bind_method(D_METHOD("set_state_machine", "state_machine"),
                         &RiveSprite2D::set_state_machine);
    ClassDB::bind_method(D_METHOD("get_state_machine"),
                         &RiveSprite2D::get_state_machine);
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveSprite2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveSprite2D::get_size);
    ClassDB::bind_method(D_METHOD("set_playing", "playing"),
                         &RiveSprite2D::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &RiveSprite2D::is_playing);
    ClassDB::bind_method(D_METHOD("set_pause_when_hidden", "pause"),
                         &RiveSprite2D::set_pause_when_hidden);
    ClassDB::bind_method(D_METHOD("get_pause_when_hidden"),
                         &RiveSprite2D::get_pause_when_hidden);
    ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"),
                         &RiveSprite2D::set_speed_scale);
    ClassDB::bind_method(D_METHOD("get_speed_scale"),
                         &RiveSprite2D::get_speed_scale);
    ClassDB::bind_method(D_METHOD("set_bool_input", "name", "value"),
                         &RiveSprite2D::set_bool_input);
    ClassDB::bind_method(D_METHOD("set_number_input", "name", "value"),
                         &RiveSprite2D::set_number_input);
    ClassDB::bind_method(D_METHOD("fire_trigger", "name"),
                         &RiveSprite2D::fire_trigger);
    ClassDB::bind_method(D_METHOD("set_property", "path", "value"),
                         &RiveSprite2D::set_property);
    ClassDB::bind_method(D_METHOD("fire_property_trigger", "path"),
                         &RiveSprite2D::fire_property_trigger);
    ClassDB::bind_method(D_METHOD("watch_property", "path"),
                         &RiveSprite2D::watch_property);
    ClassDB::bind_method(D_METHOD("get_property", "path"),
                         &RiveSprite2D::get_property);
    ClassDB::bind_method(D_METHOD("on_event", "name", "callable"),
                         &RiveSprite2D::on_event);
    ClassDB::bind_method(D_METHOD("on_property", "path", "callable"),
                         &RiveSprite2D::on_property);
    ClassDB::bind_method(D_METHOD("_dispatch_event", "event_name",
                                  "properties", "want", "callable"),
                         &RiveSprite2D::_dispatch_event);
    ClassDB::bind_method(D_METHOD("_dispatch_property", "path", "value",
                                  "want", "callable"),
                         &RiveSprite2D::_dispatch_property);
    ClassDB::bind_method(
        D_METHOD("list_append", "path", "view_model", "instance_name"),
        &RiveSprite2D::list_append, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("list_remove_at", "path", "index"),
                         &RiveSprite2D::list_remove_at);
    ClassDB::bind_method(D_METHOD("list_swap", "path", "a", "b"),
                         &RiveSprite2D::list_swap);
    ClassDB::bind_method(D_METHOD("list_clear", "path"), &RiveSprite2D::list_clear);
    ClassDB::bind_method(
        D_METHOD("list_set_property", "path", "index", "sub_path", "value"),
        &RiveSprite2D::list_set_property);
    ClassDB::bind_method(
        D_METHOD("list_read_property", "path", "index", "sub_path"),
        &RiveSprite2D::list_read_property);
    ClassDB::bind_method(
        D_METHOD("set_artboard_property", "path", "artboard_name"),
        &RiveSprite2D::set_artboard_property);
    ClassDB::bind_method(
        D_METHOD("replace_view_model", "path", "view_model", "instance_name"),
        &RiveSprite2D::replace_view_model, DEFVAL(String()));

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file",
                              PROPERTY_HINT_RESOURCE_TYPE, "RiveFileResource"),
                 "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard"), "set_artboard",
                 "get_artboard");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine"),
                 "set_state_machine", "get_state_machine");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "audio_bus"),
                 "set_audio_bus", "get_audio_bus");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fit", PROPERTY_HINT_ENUM,
                              "Contain,Cover,Fill,Fit Width,Fit Height,None,Scale Down,Layout"),
                 "set_fit", "get_fit");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM,
                              "Top Left,Top Center,Top Right,Center Left,Center,Center Right,Bottom Left,Bottom Center,Bottom Right"),
                 "set_alignment", "get_alignment");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "layout_scale",
                              PROPERTY_HINT_RANGE, "0.25,4,0.01,or_greater"),
                 "set_layout_scale", "get_layout_scale");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size",
                 "get_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing",
                 "is_playing");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pause_when_hidden"),
                 "set_pause_when_hidden", "get_pause_when_hidden");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale",
                              PROPERTY_HINT_RANGE, "0,4,0.01,or_greater"),
                 "set_speed_scale", "get_speed_scale");

    ADD_SIGNAL(MethodInfo("loaded"));
    ADD_SIGNAL(MethodInfo("rive_event",
                          PropertyInfo(Variant::STRING, "name"),
                          PropertyInfo(Variant::DICTIONARY, "properties")));
    ADD_SIGNAL(MethodInfo("state_changed",
                          PropertyInfo(Variant::STRING, "state_name")));
    ADD_SIGNAL(MethodInfo("property_changed",
                          PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::NIL, "value")));
}

void RiveSprite2D::set_file(const Ref<RiveFileResource>& p_file) {
    // Hot reload: re-imports emit changed on the resource.
    Callable reload = callable_mp(this, &RiveSprite2D::recreate_instance);
    if (rive.file.is_valid() && rive.file->is_connected("changed", reload)) {
        rive.file->disconnect("changed", reload);
    }
    rive.file = p_file;
    if (rive.file.is_valid()) {
        rive.file->connect("changed", reload);
    }
    recreate_instance();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveSprite2D::set_artboard(const String& p_artboard) {
    rive.artboard = p_artboard;
    recreate_instance();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveSprite2D::set_audio_bus(const String& p_bus) {
    audio_bus = p_bus;
    rive.dedicated_audio = !audio_bus.is_empty();
    recreate_instance();
    update_audio_player();
}

void RiveSprite2D::update_audio_player() {
    if (audio_bus.is_empty()) {
        if (audio_player != nullptr) {
            audio_player->queue_free();
            audio_player = nullptr;
        }
        return;
    }
    if (audio_player == nullptr) {
        audio_player = memnew(godot::AudioStreamPlayer);
        Ref<RiveAudioStream> stream;
        stream.instantiate();
        audio_player->set_stream(stream);
        add_child(audio_player, false, INTERNAL_MODE_BACK);
    }
    audio_player->set_bus(audio_bus);
    Ref<RiveAudioStream> stream = audio_player->get_stream();
    if (stream.is_valid()) {
        stream->set_instance_id(rive.get_instance_id());
    }
    if (!audio_player->is_playing()) {
        // play() before the player is READY is silently reset — defer.
        audio_player->call_deferred("play");
    }
}

void RiveSprite2D::set_fit(int p_fit) {
    rive.fit = CLAMP(p_fit, 0, 7);
    recreate_instance();
}

void RiveSprite2D::set_alignment(int p_alignment) {
    alignment_index = CLAMP(p_alignment, 0, 8);
    // 3x3 anchor grid -> [-1,1] per axis.
    rive.alignment = godot::Vector2(float(alignment_index % 3) - 1.0f,
                                    float(alignment_index / 3) - 1.0f);
    recreate_instance();
}

void RiveSprite2D::set_layout_scale(double p_scale) {
    rive.layout_scale = MAX(0.01, p_scale);
    recreate_instance();
}

void RiveSprite2D::set_state_machine(const String& p_state_machine) {
    rive.state_machine = p_state_machine;
    recreate_instance();
    update_configuration_warnings();
    notify_property_list_changed();
}

void RiveSprite2D::set_size(const Vector2i& p_size) {
    size = p_size;
    recreate_instance();
}

void RiveSprite2D::set_playing(bool p_playing) {
    playing = p_playing;
    set_process(playing && rive.is_live());
}

void RiveSprite2D::set_pause_when_hidden(bool p_pause) {
    pause_when_hidden = p_pause;
}

void RiveSprite2D::set_speed_scale(double p_speed_scale) {
    speed_scale = MAX(0.0, p_speed_scale);
}

void RiveSprite2D::set_bool_input(const String& p_name, bool p_value) {
    rive.set_bool_input(p_name, p_value);
}

void RiveSprite2D::set_number_input(const String& p_name, double p_value) {
    rive.set_number_input(p_name, p_value);
}

void RiveSprite2D::fire_trigger(const String& p_name) {
    rive.fire_trigger(p_name);
}

void RiveSprite2D::set_property(const String& p_path, const Variant& p_value) {
    rive.set_property(p_path, p_value);
}

void RiveSprite2D::fire_property_trigger(const String& p_path) {
    rive.fire_property_trigger(p_path);
}

void RiveSprite2D::watch_property(const String& p_path) {
    rive.watch_property(p_path);
}

Variant RiveSprite2D::get_property(const String& p_path) const {
    return rive.get_property(p_path);
}

void RiveSprite2D::_dispatch_event(const String& p_event_name,
                            const Dictionary& p_properties, const String& p_want,
                            const Callable& p_callable) {
    if (p_event_name == p_want) {
        p_callable.call(p_properties);
    }
}

void RiveSprite2D::_dispatch_property(const String& p_path, const Variant& p_value,
                              const String& p_want, const Callable& p_callable) {
    if (p_path == p_want) {
        p_callable.call(p_value);
    }
}

Callable RiveSprite2D::on_event(const String& p_name, const Callable& p_callable) {
    Callable wrapper = callable_mp(this, &RiveSprite2D::_dispatch_event)
                           .bind(p_name, p_callable);
    connect("rive_event", wrapper);
    return wrapper;
}

Callable RiveSprite2D::on_property(const String& p_path, const Callable& p_callable) {
    watch_property(p_path); // ensure changes are reported
    Callable wrapper = callable_mp(this, &RiveSprite2D::_dispatch_property)
                           .bind(p_path, p_callable);
    connect("property_changed", wrapper);
    return wrapper;
}

void RiveSprite2D::list_append(const String& p_path, const String& p_view_model,
                        const String& p_instance_name) {
    rive.list_append(p_path, p_view_model, p_instance_name);
}

void RiveSprite2D::list_remove_at(const String& p_path, int p_index) {
    rive.list_remove_at(p_path, p_index);
}

void RiveSprite2D::list_swap(const String& p_path, int p_a, int p_b) {
    rive.list_swap(p_path, p_a, p_b);
}

void RiveSprite2D::list_clear(const String& p_path) {
    rive.list_clear(p_path);
}

void RiveSprite2D::list_set_property(const String& p_path, int p_index,
                              const String& p_sub_path,
                              const Variant& p_value) {
    rive.list_set_property(p_path, p_index, p_sub_path, p_value);
}

void RiveSprite2D::list_read_property(const String& p_path, int p_index,
                               const String& p_sub_path) {
    rive.list_read_property(p_path, p_index, p_sub_path);
}

void RiveSprite2D::set_artboard_property(const String& p_path,
                                  const String& p_artboard_name) {
    rive.set_artboard_property(p_path, p_artboard_name);
}

void RiveSprite2D::replace_view_model(const String& p_path, const String& p_view_model,
                               const String& p_instance_name) {
    rive.replace_view_model(p_path, p_view_model, p_instance_name);
}

void RiveSprite2D::recreate_instance() {
    rive.release();
    if (!is_inside_tree()) {
        return;
    }
    rive.create(size);
    set_process(playing && rive.is_live());
    queue_redraw();
    update_audio_player(); // instance id changed; re-point the stream
}

PackedStringArray RiveSprite2D::_get_configuration_warnings() const {
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

void RiveSprite2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            recreate_instance();
        } break;
        case NOTIFICATION_EXIT_TREE: {
            rive.release();
            set_process(false);
        } break;
        case NOTIFICATION_PROCESS: {
            if (pause_when_hidden && !is_visible_in_tree()) {
                break; // GOALS G4.6: hidden instances stop advancing
            }
            rive.frame(get_process_delta_time() * speed_scale);
            if (rive.update_texture_binding()) {
                queue_redraw();
                // First bind after (re)create: the instance is live on the
                // render thread — VM reads/writes and pointer input are
                // now meaningful (mirrors rive-unity's WidgetStatus.Loaded).
                emit_signal("loaded");
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
                    get_canvas_item(), Rect2(Vector2(), Vector2(size)),
                    rive.get_canvas_texture_rid());
            }
        } break;
    }
}

bool RiveSprite2D::_set(const StringName& p_name, const Variant& p_value) {
    return rive.try_set(p_name, p_value);
}

bool RiveSprite2D::_get(const StringName& p_name, Variant& r_value) const {
    return rive.try_get(p_name, r_value);
}

void RiveSprite2D::_get_property_list(List<PropertyInfo>* p_list) const {
    rive.get_property_list(p_list);
}

void RiveSprite2D::_validate_property(PropertyInfo& p_property) const {
    rive.validate_property(p_property);
}

} // namespace rivegd

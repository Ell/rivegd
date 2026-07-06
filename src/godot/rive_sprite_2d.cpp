#include "godot/rive_sprite_2d.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

void RiveSprite2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file", "file"), &RiveSprite2D::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &RiveSprite2D::get_file);
    ClassDB::bind_method(D_METHOD("set_artboard", "artboard"),
                         &RiveSprite2D::set_artboard);
    ClassDB::bind_method(D_METHOD("get_artboard"), &RiveSprite2D::get_artboard);
    ClassDB::bind_method(D_METHOD("set_state_machine", "state_machine"),
                         &RiveSprite2D::set_state_machine);
    ClassDB::bind_method(D_METHOD("get_state_machine"),
                         &RiveSprite2D::get_state_machine);
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveSprite2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveSprite2D::get_size);
    ClassDB::bind_method(D_METHOD("set_playing", "playing"),
                         &RiveSprite2D::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &RiveSprite2D::is_playing);
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

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file",
                              PROPERTY_HINT_RESOURCE_TYPE, "RiveFileResource"),
                 "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard"), "set_artboard",
                 "get_artboard");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine"),
                 "set_state_machine", "get_state_machine");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size",
                 "get_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing",
                 "is_playing");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale",
                              PROPERTY_HINT_RANGE, "0,4,0.01,or_greater"),
                 "set_speed_scale", "get_speed_scale");

    ADD_SIGNAL(MethodInfo("rive_event",
                          PropertyInfo(Variant::STRING, "name"),
                          PropertyInfo(Variant::DICTIONARY, "properties")));
}

void RiveSprite2D::set_file(const Ref<RiveFileResource>& p_file) {
    rive.file = p_file;
    recreate_instance();
    notify_property_list_changed();
}

void RiveSprite2D::set_artboard(const String& p_artboard) {
    rive.artboard = p_artboard;
    recreate_instance();
    notify_property_list_changed();
}

void RiveSprite2D::set_state_machine(const String& p_state_machine) {
    rive.state_machine = p_state_machine;
    recreate_instance();
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

void RiveSprite2D::recreate_instance() {
    rive.release();
    if (!is_inside_tree()) {
        return;
    }
    rive.create(size);
    set_process(playing && rive.is_live());
    queue_redraw();
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
            rive.frame(get_process_delta_time() * speed_scale);
            if (rive.update_texture_binding()) {
                queue_redraw();
            }
            Array events = rive.take_events();
            for (int i = 0; i < events.size(); ++i) {
                Dictionary event = events[i];
                emit_signal("rive_event", event["name"], event["properties"]);
            }
        } break;
        case NOTIFICATION_DRAW: {
            if (rive.get_texture().is_valid() &&
                rive.get_texture()->get_texture_rd_rid().is_valid()) {
                draw_texture_rect(rive.get_texture(),
                                  Rect2(Vector2(), Vector2(size)), false);
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

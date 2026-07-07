#include "godot/rive_texture.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

RiveTexture::RiveTexture() = default;

RiveTexture::~RiveTexture() {
    if (frame_hook_connected) {
        RenderingServer::get_singleton()->disconnect(
            "frame_pre_draw", callable_mp(this, &RiveTexture::on_frame_pre_draw));
    }
    // Unbind the RID before the instance (and its RD texture) is released.
    set_texture_rd_rid(RID());
}

void RiveTexture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file", "file"), &RiveTexture::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &RiveTexture::get_file);
    ClassDB::bind_method(D_METHOD("set_artboard", "artboard"),
                         &RiveTexture::set_artboard);
    ClassDB::bind_method(D_METHOD("get_artboard"), &RiveTexture::get_artboard);
    ClassDB::bind_method(D_METHOD("set_state_machine", "state_machine"),
                         &RiveTexture::set_state_machine);
    ClassDB::bind_method(D_METHOD("get_state_machine"),
                         &RiveTexture::get_state_machine);
    ClassDB::bind_method(D_METHOD("set_render_size", "render_size"),
                         &RiveTexture::set_render_size);
    ClassDB::bind_method(D_METHOD("get_render_size"),
                         &RiveTexture::get_render_size);
    ClassDB::bind_method(D_METHOD("set_playing", "playing"),
                         &RiveTexture::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &RiveTexture::is_playing);
    ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"),
                         &RiveTexture::set_speed_scale);
    ClassDB::bind_method(D_METHOD("get_speed_scale"),
                         &RiveTexture::get_speed_scale);
    ClassDB::bind_method(D_METHOD("set_bool_input", "name", "value"),
                         &RiveTexture::set_bool_input);
    ClassDB::bind_method(D_METHOD("set_number_input", "name", "value"),
                         &RiveTexture::set_number_input);
    ClassDB::bind_method(D_METHOD("fire_trigger", "name"),
                         &RiveTexture::fire_trigger);
    ClassDB::bind_method(D_METHOD("set_property", "path", "value"),
                         &RiveTexture::set_property);
    ClassDB::bind_method(D_METHOD("fire_property_trigger", "path"),
                         &RiveTexture::fire_property_trigger);
    ClassDB::bind_method(D_METHOD("watch_property", "path"),
                         &RiveTexture::watch_property);
    ClassDB::bind_method(D_METHOD("get_property", "path"),
                         &RiveTexture::get_property);
    ClassDB::bind_method(
        D_METHOD("list_append", "path", "view_model", "instance_name"),
        &RiveTexture::list_append, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("list_remove_at", "path", "index"),
                         &RiveTexture::list_remove_at);
    ClassDB::bind_method(D_METHOD("list_swap", "path", "a", "b"),
                         &RiveTexture::list_swap);
    ClassDB::bind_method(D_METHOD("list_clear", "path"), &RiveTexture::list_clear);
    ClassDB::bind_method(
        D_METHOD("list_set_property", "path", "index", "sub_path", "value"),
        &RiveTexture::list_set_property);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file",
                              PROPERTY_HINT_RESOURCE_TYPE, "RiveFileResource"),
                 "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard"), "set_artboard",
                 "get_artboard");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine"),
                 "set_state_machine", "get_state_machine");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "render_size"),
                 "set_render_size", "get_render_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing",
                 "is_playing");
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

void RiveTexture::set_file(const Ref<RiveFileResource>& p_file) {
    Callable reload = callable_mp(this, &RiveTexture::recreate_instance);
    if (rive.file.is_valid() && rive.file->is_connected("changed", reload)) {
        rive.file->disconnect("changed", reload);
    }
    rive.file = p_file;
    if (rive.file.is_valid()) {
        rive.file->connect("changed", reload);
    }
    recreate_instance();
}

void RiveTexture::set_artboard(const String& p_artboard) {
    rive.artboard = p_artboard;
    recreate_instance();
}

void RiveTexture::set_state_machine(const String& p_state_machine) {
    rive.state_machine = p_state_machine;
    recreate_instance();
}

void RiveTexture::set_render_size(const Vector2i& p_size) {
    render_size = Vector2i(MAX(1, p_size.x), MAX(1, p_size.y));
    recreate_instance();
}

void RiveTexture::set_playing(bool p_playing) { playing = p_playing; }

void RiveTexture::set_speed_scale(double p_speed_scale) {
    speed_scale = MAX(0.0, p_speed_scale);
}

void RiveTexture::set_bool_input(const String& p_name, bool p_value) {
    rive.set_bool_input(p_name, p_value);
}

void RiveTexture::set_number_input(const String& p_name, double p_value) {
    rive.set_number_input(p_name, p_value);
}

void RiveTexture::fire_trigger(const String& p_name) {
    rive.fire_trigger(p_name);
}

void RiveTexture::set_property(const String& p_path, const Variant& p_value) {
    rive.set_property(p_path, p_value);
}

void RiveTexture::fire_property_trigger(const String& p_path) {
    rive.fire_property_trigger(p_path);
}

void RiveTexture::watch_property(const String& p_path) {
    rive.watch_property(p_path);
}

Variant RiveTexture::get_property(const String& p_path) const {
    return rive.get_property(p_path);
}

void RiveTexture::list_append(const String& p_path, const String& p_view_model,
                        const String& p_instance_name) {
    rive.list_append(p_path, p_view_model, p_instance_name);
}

void RiveTexture::list_remove_at(const String& p_path, int p_index) {
    rive.list_remove_at(p_path, p_index);
}

void RiveTexture::list_swap(const String& p_path, int p_a, int p_b) {
    rive.list_swap(p_path, p_a, p_b);
}

void RiveTexture::list_clear(const String& p_path) {
    rive.list_clear(p_path);
}

void RiveTexture::list_set_property(const String& p_path, int p_index,
                              const String& p_sub_path,
                              const Variant& p_value) {
    rive.list_set_property(p_path, p_index, p_sub_path, p_value);
}

void RiveTexture::recreate_instance() {
    rive.release();
    set_texture_rd_rid(RID());
    if (rive.file.is_null() || !rive.file->is_valid()) {
        return;
    }
    rive.create(render_size);
    last_frame_usec = Time::get_singleton()->get_ticks_usec();
    if (!frame_hook_connected) {
        RenderingServer::get_singleton()->connect(
            "frame_pre_draw", callable_mp(this, &RiveTexture::on_frame_pre_draw));
        frame_hook_connected = true;
    }
}

void RiveTexture::on_frame_pre_draw() {
    if (!rive.is_live()) {
        return;
    }
    const uint64_t now = Time::get_singleton()->get_ticks_usec();
    const double delta =
        MIN(0.1, double(now - last_frame_usec) / 1000000.0) * speed_scale;
    last_frame_usec = now;

    if (playing) {
        rive.frame(delta);
    }
    if (rive.update_texture_binding()) {
        if (rive.get_rd_texture_rid().is_valid()) {
            set_texture_rd_rid(rive.get_rd_texture_rid());
        } else {
            ERR_PRINT_ONCE("rivegd: RiveTexture needs a RenderingDevice "
                           "renderer (Forward+/Mobile); use RiveSprite2D or "
                           "RiveControl under Compatibility.");
        }
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
        emit_signal("property_changed", change["path"], change["value"]);
    }
}

void RiveTexture::_validate_property(PropertyInfo& p_property) const {
    rive.validate_property(p_property);
}

} // namespace rivegd

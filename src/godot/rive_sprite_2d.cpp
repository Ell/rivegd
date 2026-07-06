#include "godot/rive_sprite_2d.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace rivegd {

RiveSprite2D::RiveSprite2D() {
    texture.instantiate();
}

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

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file", PROPERTY_HINT_RESOURCE_TYPE,
                              "RiveFileResource"),
                 "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard"), "set_artboard",
                 "get_artboard");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine"),
                 "set_state_machine", "get_state_machine");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size",
                 "get_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing",
                 "is_playing");
}

void RiveSprite2D::set_file(const Ref<RiveFileResource>& p_file) {
    file = p_file;
    recreate_instance();
}

void RiveSprite2D::set_artboard(const String& p_artboard) {
    artboard = p_artboard;
    recreate_instance();
}

void RiveSprite2D::set_state_machine(const String& p_state_machine) {
    state_machine = p_state_machine;
    recreate_instance();
}

void RiveSprite2D::set_size(const Vector2i& p_size) {
    size = p_size;
    recreate_instance();
}

void RiveSprite2D::set_playing(bool p_playing) {
    playing = p_playing;
    set_process(playing && instance_id != 0);
}

void RiveSprite2D::recreate_instance() {
    release_instance();
    if (!is_inside_tree() || file.is_null() || !file->is_valid()) {
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
                  size));
    texture_bound = false;
    set_process(playing);
}

void RiveSprite2D::release_instance() {
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
    texture->set_texture_rd_rid(RID());
    set_process(false);
}

void RiveSprite2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            recreate_instance();
        } break;
        case NOTIFICATION_EXIT_TREE: {
            release_instance();
        } break;
        case NOTIFICATION_PROCESS: {
            if (instance_id == 0) {
                break;
            }
            RiveRenderServer* server = RiveRenderServer::get_singleton();
            RenderingServer::get_singleton()->call_on_render_thread(
                callable_mp(server, &RiveRenderServer::rt_frame)
                    .bind(instance_id, get_process_delta_time()));
            if (!texture_bound) {
                RID rid = server->get_texture_rid(instance_id);
                if (rid.is_valid()) {
                    texture->set_texture_rd_rid(rid);
                    texture_bound = true;
                    queue_redraw();
                }
            }
        } break;
        case NOTIFICATION_DRAW: {
            if (texture_bound) {
                draw_texture_rect(texture, Rect2(Vector2(), Vector2(size)),
                                  false);
            }
        } break;
    }
}

} // namespace rivegd

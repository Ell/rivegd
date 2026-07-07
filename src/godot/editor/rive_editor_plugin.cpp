#include "godot/editor/rive_editor_plugin.h"

#include "godot/rive_control.h"
#include "godot/rive_file_resource.h"
#include "godot/rive_sprite_2d.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_resource_preview.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

using namespace godot;

namespace rivegd {

bool RiveInspectorPlugin::_can_handle(Object* p_object) const {
    return Object::cast_to<RiveFileResource>(p_object) != nullptr ||
           Object::cast_to<RiveSprite2D>(p_object) != nullptr ||
           Object::cast_to<RiveControl>(p_object) != nullptr;
}

void RiveInspectorPlugin::_parse_begin(Object* p_object) {
    // Live preview for the resource itself.
    if (auto* file = Object::cast_to<RiveFileResource>(p_object)) {
        if (!file->is_valid()) {
            return;
        }
        RiveControl* preview = memnew(RiveControl);
        preview->set_file(Ref<RiveFileResource>(file));
        preview->set_custom_minimum_size(Vector2(0, 220));
        add_custom_control(preview);
        return;
    }

    // Trigger buttons for nodes.
    Ref<RiveFileResource> file;
    String artboard;
    String state_machine;
    if (auto* sprite = Object::cast_to<RiveSprite2D>(p_object)) {
        file = sprite->get_file();
        artboard = sprite->get_artboard();
        state_machine = sprite->get_state_machine();
    } else if (auto* control = Object::cast_to<RiveControl>(p_object)) {
        file = control->get_file();
        artboard = control->get_artboard();
        state_machine = control->get_state_machine();
    }
    if (file.is_null() || !file->is_valid()) {
        return;
    }

    Array inputs = file->get_input_descriptions(artboard, state_machine);
    VBoxContainer* box = nullptr;
    for (int i = 0; i < inputs.size(); ++i) {
        Dictionary description = inputs[i];
        if (String(description["type"]) != "trigger") {
            continue;
        }
        if (box == nullptr) {
            box = memnew(VBoxContainer);
        }
        const String name = description["name"];
        Button* button = memnew(Button);
        button->set_text("Fire \"" + name + "\"");
        button->connect("pressed",
                        Callable(p_object, "fire_trigger").bind(name));
        box->add_child(button);
    }
    if (box != nullptr) {
        add_custom_control(box);
    }
}

bool RivePreviewGenerator::_handles(const String& p_type) const {
    return p_type == "RiveFileResource";
}

Ref<Texture2D> RivePreviewGenerator::_generate(const Ref<Resource>& p_resource,
                                               const Vector2i& p_size,
                                               const Dictionary&) const {
    Ref<RiveFileResource> file = p_resource;
    if (file.is_null() || !file->is_valid()) {
        return Ref<Texture2D>();
    }
    RiveRenderServer* server = RiveRenderServer::get_singleton();
    if (server == nullptr) {
        return Ref<Texture2D>();
    }
    Ref<Image> image = server->render_thumbnail(file->get_data(), p_size);
    if (image.is_null()) {
        return Ref<Texture2D>();
    }
    return ImageTexture::create_from_image(image);
}

void RiveEditorPlugin::_enter_tree() {
    inspector_plugin.instantiate();
    add_inspector_plugin(inspector_plugin);
    preview_generator.instantiate();
    get_editor_interface()->get_resource_previewer()->add_preview_generator(
        preview_generator);
}

void RiveEditorPlugin::_exit_tree() {
    get_editor_interface()->get_resource_previewer()->remove_preview_generator(
        preview_generator);
    preview_generator.unref();
    remove_inspector_plugin(inspector_plugin);
    inspector_plugin.unref();
}

} // namespace rivegd

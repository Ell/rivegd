#include "godot/editor/rive_editor_plugin.h"

#include "godot/rive_control.h"
#include "godot/rive_file_resource.h"
#include "godot/rive_sprite_2d.h"

#include <godot_cpp/classes/button.hpp>
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

void RiveEditorPlugin::_enter_tree() {
    inspector_plugin.instantiate();
    add_inspector_plugin(inspector_plugin);
}

void RiveEditorPlugin::_exit_tree() {
    remove_inspector_plugin(inspector_plugin);
    inspector_plugin.unref();
}

} // namespace rivegd

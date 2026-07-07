#include "godot/editor/rive_editor_plugin.h"

#include "godot/rive_control.h"
#include "godot/rive_file_resource.h"
#include "godot/rive_sprite_2d.h"

#include "godot/rive_render_server.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_resource_preview.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include "godot/rive_control.h"
#include "godot/rive_sprite_2d.h"
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

// Is p_data a single-.riv "files" drag from the FileSystem dock?
static String riv_drag_path(const Variant& p_data) {
    if (p_data.get_type() != Variant::DICTIONARY) {
        return String();
    }
    const Dictionary d = p_data;
    if (String(d.get("type", "")) != "files") {
        return String();
    }
    const PackedStringArray files = d.get("files", PackedStringArray());
    if (files.size() != 1 || !files[0].ends_with(".riv")) {
        return String();
    }
    return files[0];
}

void RiveDropOverlay::_process(double) {
    // Inert unless a .riv drag is in flight over the visible 2D screen.
    Viewport* viewport = get_viewport();
    bool active = false;
    if (viewport != nullptr && viewport->gui_is_dragging()) {
        if (!riv_drag_path(viewport->gui_get_drag_data()).is_empty()) {
            SubViewport* vp2d =
                EditorInterface::get_singleton()->get_editor_viewport_2d();
            Control* container =
                Object::cast_to<Control>(vp2d->get_parent());
            active = container != nullptr && container->is_visible_in_tree();
        }
    }
    set_mouse_filter(active ? MOUSE_FILTER_STOP : MOUSE_FILTER_IGNORE);
}

bool RiveDropOverlay::_can_drop_data(const Vector2&,
                                     const Variant& p_data) const {
    return !riv_drag_path(p_data).is_empty();
}

void RiveDropOverlay::_drop_data(const Vector2& p_at, const Variant& p_data) {
    const String path = riv_drag_path(p_data);
    EditorInterface* editor = EditorInterface::get_singleton();
    Node* root = editor->get_edited_scene_root();
    if (path.is_empty() || root == nullptr || plugin == nullptr) {
        return;
    }
    Ref<RiveFileResource> file = ResourceLoader::get_singleton()->load(path);
    if (file.is_null()) {
        return;
    }

    // Overlay-local -> 2D-world through the editor viewport's canvas
    // transform (offset by the viewport container's position on screen).
    SubViewport* vp2d = editor->get_editor_viewport_2d();
    Vector2 world = p_at;
    if (Control* container = Object::cast_to<Control>(vp2d->get_parent())) {
        const Vector2 in_viewport = get_global_position() + p_at -
                                    container->get_global_position();
        world = vp2d->get_global_canvas_transform().affine_inverse().xform(
            in_viewport);
    }

    // Control scenes get a RiveControl; everything else a RiveSprite2D.
    Node* node = nullptr;
    if (Object::cast_to<Control>(root) != nullptr) {
        RiveControl* control = memnew(RiveControl);
        control->set_file(file);
        control->set_position(world);
        node = control;
    } else {
        RiveSprite2D* sprite = memnew(RiveSprite2D);
        sprite->set_file(file);
        sprite->set_position(world);
        node = sprite;
    }
    node->set_name(path.get_file().get_basename());

    EditorUndoRedoManager* undo = plugin->get_undo_redo();
    undo->create_action("Instantiate Rive node");
    undo->add_do_method(root, "add_child", node, true);
    undo->add_do_method(node, "set_owner", root);
    undo->add_do_reference(node);
    undo->add_undo_method(root, "remove_child", node);
    undo->commit_action();

    editor->get_selection()->clear();
    editor->get_selection()->add_node(node);
}

void RiveEditorPlugin::_enter_tree() {
    inspector_plugin.instantiate();
    add_inspector_plugin(inspector_plugin);
    preview_generator.instantiate();
    get_editor_interface()->get_resource_previewer()->add_preview_generator(
        preview_generator);

    drop_overlay = memnew(RiveDropOverlay);
    drop_overlay->plugin = this;
    drop_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    drop_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    get_editor_interface()->get_editor_main_screen()->add_child(drop_overlay);
}

void RiveEditorPlugin::_exit_tree() {
    get_editor_interface()->get_resource_previewer()->remove_preview_generator(
        preview_generator);
    preview_generator.unref();
    remove_inspector_plugin(inspector_plugin);
    inspector_plugin.unref();
    if (drop_overlay != nullptr) {
        drop_overlay->queue_free();
        drop_overlay = nullptr;
    }
}

} // namespace rivegd

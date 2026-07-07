#pragma once

#include <godot_cpp/classes/editor_inspector_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_resource_preview_generator.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace rivegd {

// Inspector integration (GOALS G3.2 partial):
// - RiveFileResource gets a live playback preview at the top of its
//   inspector (rendered by the real renderer — WYSIWYG).
// - Nodes with trigger inputs get "Fire" buttons.
class RiveInspectorPlugin : public godot::EditorInspectorPlugin {
    GDCLASS(RiveInspectorPlugin, godot::EditorInspectorPlugin)

public:
    bool _can_handle(godot::Object* p_object) const override;
    void _parse_begin(godot::Object* p_object) override;

protected:
    static void _bind_methods() {}
};

// FileSystem-dock thumbnails: renders the default artboard's first frame
// through the real renderer (GOALS G3.2).
class RivePreviewGenerator : public godot::EditorResourcePreviewGenerator {
    GDCLASS(RivePreviewGenerator, godot::EditorResourcePreviewGenerator)

public:
    bool _handles(const godot::String& p_type) const override;
    godot::Ref<godot::Texture2D> _generate(
        const godot::Ref<godot::Resource>& p_resource,
        const godot::Vector2i& p_size,
        const godot::Dictionary& p_metadata) const override;

protected:
    static void _bind_methods() {}
};

class RiveEditorPlugin : public godot::EditorPlugin {
    GDCLASS(RiveEditorPlugin, godot::EditorPlugin)

public:
    void _enter_tree() override;
    void _exit_tree() override;

protected:
    static void _bind_methods() {}

private:
    godot::Ref<RiveInspectorPlugin> inspector_plugin;
    godot::Ref<RivePreviewGenerator> preview_generator;
};

} // namespace rivegd

#pragma once

#include "godot/rive_file_resource.h"

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>

namespace rivegd {

// World-space 2D node that plays a Rive artboard (GOALS G4.1).
// Phase 1 scope: file/artboard/state-machine selection, size, playback.
class RiveSprite2D : public godot::Node2D {
    GDCLASS(RiveSprite2D, godot::Node2D)

public:
    RiveSprite2D();

    void set_file(const godot::Ref<RiveFileResource>& p_file);
    godot::Ref<RiveFileResource> get_file() const { return file; }

    void set_artboard(const godot::String& p_artboard);
    godot::String get_artboard() const { return artboard; }

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return state_machine; }

    void set_size(const godot::Vector2i& p_size);
    godot::Vector2i get_size() const { return size; }

    void set_playing(bool p_playing);
    bool is_playing() const { return playing; }

    void _notification(int p_what);

protected:
    static void _bind_methods();

private:
    void recreate_instance();
    void release_instance();

    godot::Ref<RiveFileResource> file;
    godot::String artboard;
    godot::String state_machine;
    godot::Vector2i size = godot::Vector2i(512, 512);
    bool playing = true;

    int64_t instance_id = 0;
    godot::Ref<godot::Texture2DRD> texture;
    bool texture_bound = false;
};

} // namespace rivegd

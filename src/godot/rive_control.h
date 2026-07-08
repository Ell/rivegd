#pragma once

#include "godot/rive_gamepad_encoder.h"
#include "godot/rive_instance.h"

#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>

namespace rivegd {

// UI node that plays a Rive artboard inside a Control rect and forwards
// pointer input to the state machine, so Rive listeners (hover/click/drag)
// work with zero code (GOALS G4.1/G4.7).
//
// The render texture tracks the control's size; resizes are debounced so a
// live drag doesn't re-import the file every frame.
class RiveControl : public godot::Control {
    GDCLASS(RiveControl, godot::Control)

public:
    void set_file(const godot::Ref<RiveFileResource>& p_file);
    godot::Ref<RiveFileResource> get_file() const { return rive.file; }

    void set_artboard(const godot::String& p_artboard);
    godot::String get_artboard() const { return rive.artboard; }

    // Non-empty routes THIS node's Rive audio to that Godot bus via an
    // internal AudioStreamPlayer + dedicated engine (G5.3 per-node
    // routing). Empty (default) mixes into the shared global stream.
    void set_audio_bus(const godot::String& p_bus);
    godot::String get_audio_bus() const { return audio_bus; }

    void set_fit(int p_fit);
    int get_fit() const { return rive.fit; }
    void set_alignment(int p_alignment);
    int get_alignment() const { return alignment_index; }
    void set_layout_scale(double p_scale);
    double get_layout_scale() const { return rive.layout_scale; }

    void set_state_machine(const godot::String& p_state_machine);
    godot::String get_state_machine() const { return rive.state_machine; }

    void set_playing(bool p_playing);
    bool is_playing() const { return playing; }

    void set_pause_when_hidden(bool p_pause);
    bool get_pause_when_hidden() const { return pause_when_hidden; }

    void set_speed_scale(double p_speed_scale);
    double get_speed_scale() const { return speed_scale; }

    void set_bool_input(const godot::String& p_name, bool p_value);
    void set_number_input(const godot::String& p_name, double p_value);
    void fire_trigger(const godot::String& p_name);

    void set_property(const godot::String& p_path, const godot::Variant& p_value);
    void fire_property_trigger(const godot::String& p_path);
    void watch_property(const godot::String& p_path);
    godot::Variant get_property(const godot::String& p_path) const;

    // Targeted callables (G4.3 tier 2): sugar over the rive_event /
    // property_changed signals. Each returns the connected Callable so it
    // can be passed to disconnect() to unsubscribe.
    godot::Callable on_event(const godot::String& p_name,
                             const godot::Callable& p_callable);
    godot::Callable on_property(const godot::String& p_path,
                                const godot::Callable& p_callable);
    void list_append(const godot::String& p_path,
                     const godot::String& p_view_model,
                     const godot::String& p_instance_name);
    void list_remove_at(const godot::String& p_path, int p_index);
    void list_swap(const godot::String& p_path, int p_a, int p_b);
    void list_clear(const godot::String& p_path);
    void list_set_property(const godot::String& p_path, int p_index,
                           const godot::String& p_sub_path,
                           const godot::Variant& p_value);
    void list_read_property(const godot::String& p_path, int p_index,
                            const godot::String& p_sub_path);
    void set_artboard_property(const godot::String& p_path,
                               const godot::String& p_artboard_name);
    void replace_view_model(const godot::String& p_path,
                            const godot::String& p_view_model,
                            const godot::String& p_instance_name);

    // 0 = OPAQUE (whole rect captures, default), 1 = TRANSLUCENT (only
    // points over an interactive Rive listener capture; clicks elsewhere
    // fall through to whatever is behind). Unity-equivalent hit modes.
    void set_hit_test_behavior(int p_behavior);
    int get_hit_test_behavior() const { return hit_test_behavior; }

    bool _has_point(const godot::Vector2& p_point) const override;

    // Mirrors the artboard's authored semantics (roles, labels, bounds)
    // into Godot's accessibility tree (AccessKit) as sub-elements of this
    // control — screen readers see Rive UI. First game-engine Rive runtime
    // with this. Elements publish during accessibility updates (i.e., when
    // assistive tech is active); the semantics stream itself always flows
    // when enabled.
    void set_accessibility_enabled(bool p_enabled);
    bool get_accessibility_enabled() const { return accessibility_enabled; }
    // Debug/testing: semantic nodes currently mirrored.
    int get_semantics_node_count() const { return semantic_nodes.size(); }

    void set_gamepad_enabled(bool p_enabled);
    bool get_gamepad_enabled() const { return gamepad_enabled; }
    // Power-user/testing entry: submit a raw rive gamepad batch (wire v2).
    void submit_gamepad_batch(const godot::PackedByteArray& p_batch);

    // Programmatic pointer injection (control-local coordinates); phase:
    // 0 = move, 1 = down, 2 = up, 3 = exit. Same path as _gui_input.
    void send_pointer_event(int p_phase, const godot::Vector2& p_position,
                            int p_pointer_id = 0);

    void focus_next_element();
    void focus_previous_element();
    void send_text_input(const godot::String& p_text);

    godot::Vector2 _get_minimum_size() const override;
    void _gui_input(const godot::Ref<godot::InputEvent>& p_event) override;

    godot::PackedStringArray _get_configuration_warnings() const override;
    void _notification(int p_what);
    bool _set(const godot::StringName& p_name, const godot::Variant& p_value);
    bool _get(const godot::StringName& p_name, godot::Variant& r_value) const;
    void _get_property_list(godot::List<godot::PropertyInfo>* p_list) const;
    void _validate_property(godot::PropertyInfo& p_property) const;

    // Signal-forwarding dispatchers for on_event/on_property (bound
    // callables; not meant to be called directly).
    void _dispatch_event(const godot::String& p_event_name,
                         const godot::Dictionary& p_properties,
                         const godot::String& p_want,
                         const godot::Callable& p_callable);
    void _dispatch_property(const godot::String& p_path,
                            const godot::Variant& p_value,
                            const godot::String& p_want,
                            const godot::Callable& p_callable);

    // The RenderingServer texture this instance renders into — bind it
    // into an ImageTexture via RenderingServer.texture_replace to present
    // Rive on any Texture2D consumer (3D materials, Sprite3D...) on every
    // renderer, including GL where Texture2DRD is unavailable.
    godot::RID get_texture_rid() const { return rive.get_canvas_texture_rid(); }

protected:
    static void _bind_methods();

private:
    void recreate_instance();
    godot::Vector2i texture_size() const;

    RiveInstance rive;
    int alignment_index = 4; // Center
    godot::String audio_bus;
    godot::AudioStreamPlayer* audio_player = nullptr;
    void update_audio_player();
    RiveGamepadEncoder gamepad_encoder;
    int hit_test_behavior = 0;
    bool accessibility_enabled = false;
    struct SemanticNode {
        int role = 0;
        godot::String label;
        godot::String value;
        godot::Rect2 bounds; // texture space
        godot::RID element;  // valid only after an accessibility pass
    };
    godot::HashMap<int64_t, SemanticNode> semantic_nodes;
    bool semantics_layout_dirty = false;
    void apply_semantics(const godot::Array& p_changes);
    void publish_accessibility();
    bool gamepad_enabled = false;
    bool playing = true;
    bool pause_when_hidden = true;
    double speed_scale = 1.0;

    // Resize debounce: recreate the texture only after the size has been
    // stable for this long.
    static constexpr double kResizeDebounceSeconds = 0.3;
    double resize_cooldown = 0.0;
    godot::Vector2i live_texture_size;
};

} // namespace rivegd

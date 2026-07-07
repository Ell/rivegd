#pragma once

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace rivegd {

// Encodes Godot joypad state into rive's gamepad batch wire format
// (version 2, little-endian; see rive/input/gamepad_batch.hpp). Buttons and
// axes are remapped from Godot's SDL layout to the W3C Standard Gamepad
// layout rive expects; triggers become analog buttons 6/7 AND axes 4/5.
class RiveGamepadEncoder {
public:
    // Diffs current Input state against the last call and returns a batch
    // with connected/update/disconnected records (empty when idle).
    godot::PackedByteArray encode_frame();

private:
    struct PadState {
        float buttons[17] = {};
        float axes[6] = {};
        bool announced = false;
    };
    godot::HashMap<int64_t, PadState> pads;
};

} // namespace rivegd

#include "godot/rive_gamepad_encoder.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/templates/local_vector.hpp>

using namespace godot;

namespace rivegd {

static constexpr uint32_t kWireVersion = 2;
static constexpr int kButtonCount = 17;
static constexpr int kAxisCount = 6;

static void put_u32(PackedByteArray& buffer, uint32_t value) {
    const int64_t at = buffer.size();
    buffer.resize(at + 4);
    buffer.encode_u32(at, value);
}

static void put_u8(PackedByteArray& buffer, uint8_t value) {
    buffer.push_back(value);
}

static void put_f32(PackedByteArray& buffer, float value) {
    const int64_t at = buffer.size();
    buffer.resize(at + 4);
    buffer.encode_float(at, value);
}

// W3C standard button index -> Godot source. Triggers (6/7) come from axes.
static float read_w3c_button(Input* input, int device, int w3c_index) {
    static constexpr JoyButton kMap[kButtonCount] = {
        JOY_BUTTON_A,              // 0 south
        JOY_BUTTON_B,              // 1 east
        JOY_BUTTON_X,              // 2 west
        JOY_BUTTON_Y,              // 3 north
        JOY_BUTTON_LEFT_SHOULDER,  // 4
        JOY_BUTTON_RIGHT_SHOULDER, // 5
        JOY_BUTTON_INVALID,        // 6 leftTrigger (axis-sourced)
        JOY_BUTTON_INVALID,        // 7 rightTrigger (axis-sourced)
        JOY_BUTTON_BACK,           // 8
        JOY_BUTTON_START,          // 9 forward
        JOY_BUTTON_LEFT_STICK,     // 10
        JOY_BUTTON_RIGHT_STICK,    // 11
        JOY_BUTTON_DPAD_UP,        // 12
        JOY_BUTTON_DPAD_DOWN,      // 13
        JOY_BUTTON_DPAD_LEFT,      // 14
        JOY_BUTTON_DPAD_RIGHT,     // 15
        JOY_BUTTON_GUIDE,          // 16 start (home)
    };
    if (w3c_index == 6) {
        return float(input->get_joy_axis(device, JOY_AXIS_TRIGGER_LEFT));
    }
    if (w3c_index == 7) {
        return float(input->get_joy_axis(device, JOY_AXIS_TRIGGER_RIGHT));
    }
    return input->is_joy_button_pressed(device, kMap[w3c_index]) ? 1.0f : 0.0f;
}

static float read_w3c_axis(Input* input, int device, int w3c_index) {
    static constexpr JoyAxis kMap[kAxisCount] = {
        JOY_AXIS_LEFT_X,  JOY_AXIS_LEFT_Y,       JOY_AXIS_RIGHT_X,
        JOY_AXIS_RIGHT_Y, JOY_AXIS_TRIGGER_LEFT, JOY_AXIS_TRIGGER_RIGHT,
    };
    return float(input->get_joy_axis(device, kMap[w3c_index]));
}

PackedByteArray RiveGamepadEncoder::encode_frame() {
    Input* input = Input::get_singleton();
    PackedByteArray batch;
    put_u32(batch, kWireVersion);
    const int64_t header_size = batch.size();

    // Disconnections first.
    LocalVector<int64_t> gone;
    for (const KeyValue<int64_t, PadState>& entry : pads) {
        if (!input->get_connected_joypads().has(entry.key)) {
            gone.push_back(entry.key);
        }
    }
    for (int64_t device : gone) {
        put_u8(batch, 2); // disconnected
        put_u32(batch, uint32_t(device));
        pads.erase(device);
    }

    for (int64_t device : input->get_connected_joypads()) {
        PadState* state = pads.getptr(device);
        if (state == nullptr) {
            // New device: full connected snapshot.
            PadState fresh;
            for (int b = 0; b < kButtonCount; ++b) {
                fresh.buttons[b] = read_w3c_button(input, device, b);
            }
            for (int a = 0; a < kAxisCount; ++a) {
                fresh.axes[a] = read_w3c_axis(input, device, a);
            }
            fresh.announced = true;
            put_u8(batch, 0); // connected
            put_u32(batch, uint32_t(device));
            put_u8(batch, 0); // mapping: standard
            put_u8(batch, kButtonCount);
            put_u8(batch, kAxisCount);
            put_u8(batch, 0); // padding
            for (int b = 0; b < kButtonCount; ++b) {
                put_f32(batch, fresh.buttons[b]);
            }
            for (int a = 0; a < kAxisCount; ++a) {
                put_f32(batch, fresh.axes[a]);
            }
            pads[device] = fresh;
            continue;
        }

        // Existing device: collect deltas into one update record.
        struct Change {
            uint8_t kind; // 0 = button, 1 = axis
            uint8_t index;
            float value;
        };
        LocalVector<Change> changes;
        for (int b = 0; b < kButtonCount; ++b) {
            const float value = read_w3c_button(input, device, b);
            if (value != state->buttons[b]) {
                state->buttons[b] = value;
                changes.push_back({0, uint8_t(b), value});
            }
        }
        for (int a = 0; a < kAxisCount; ++a) {
            const float value = read_w3c_axis(input, device, a);
            if (value != state->axes[a]) {
                state->axes[a] = value;
                changes.push_back({1, uint8_t(a), value});
            }
        }
        if (!changes.is_empty()) {
            put_u8(batch, 1); // update
            put_u32(batch, uint32_t(device));
            put_u8(batch, uint8_t(changes.size()));
            for (const Change& change : changes) {
                put_u8(batch, change.kind);
                put_u8(batch, change.index);
                put_f32(batch, change.value);
            }
        }
    }

    if (batch.size() == header_size) {
        return PackedByteArray(); // nothing happened
    }
    return batch;
}

} // namespace rivegd

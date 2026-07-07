#pragma once

// core/ is Godot-free: rive-runtime + standard library only.

#include <cstddef>
#include <cstdint>

namespace rivegd::core {

// Global fallback-font registry (GOALS G5.1): fonts consulted, in
// registration order, when a Rive text run hits a glyph its authored font
// cannot draw (CJK, emoji, localization). Thread-safe; installs rive's
// Font::gFallbackProc on first registration. Shaping runs on the render
// thread; registration is main-thread — guarded internally.
bool add_fallback_font(const uint8_t* p_data, size_t p_size);
void clear_fallback_fonts();
size_t fallback_font_count();

} // namespace rivegd::core

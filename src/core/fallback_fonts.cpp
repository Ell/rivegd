#include "core/fallback_fonts.hpp"

#include "rive/text_engine.hpp"
#include "utils/no_op_factory.hpp"

#include <mutex>
#include <vector>

namespace rivegd::core {

static std::mutex& registry_mutex() {
    static std::mutex mutex;
    return mutex;
}

static std::vector<rive::rcp<rive::Font>>& registry() {
    static std::vector<rive::rcp<rive::Font>> fonts;
    return fonts;
}

// rive iterates fallbackIndex upward until a returned font covers the
// missing unichar or we return null.
static rive::rcp<rive::Font> fallback_proc(const rive::Unichar,
                                           const uint32_t p_fallback_index,
                                           const rive::Font*) {
    std::lock_guard<std::mutex> lock(registry_mutex());
    if (p_fallback_index >= registry().size()) {
        return nullptr;
    }
    return registry()[p_fallback_index];
}

bool add_fallback_font(const uint8_t* p_data, size_t p_size) {
    // decodeFont is a non-virtual Factory helper: any factory works.
    static rive::NoOpFactory factory;
    rive::rcp<rive::Font> font =
        factory.decodeFont(rive::Span<const uint8_t>(p_data, p_size));
    if (font == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().push_back(std::move(font));
    rive::Font::gFallbackProc = fallback_proc;
    rive::Font::gFallbackProcEnabled = true;
    return true;
}

void clear_fallback_fonts() {
    std::lock_guard<std::mutex> lock(registry_mutex());
    registry().clear();
}

size_t fallback_font_count() {
    std::lock_guard<std::mutex> lock(registry_mutex());
    return registry().size();
}

} // namespace rivegd::core

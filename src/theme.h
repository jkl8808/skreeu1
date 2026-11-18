#pragma once

#include <lvgl.h>
#include <cstdint>
#include <string>

namespace theme {

// Material 3 inspired dark palette, seeded from Snapmaker orange.
constexpr uint32_t BG = 0x101216;
constexpr uint32_t SURFACE = 0x1A1D23;
constexpr uint32_t SURFACE_HI = 0x242832;
constexpr uint32_t OUTLINE = 0x2E333C;
constexpr uint32_t TEXT = 0xE8EAF0;
constexpr uint32_t MUTED = 0x9AA1AC;
constexpr uint32_t PRIMARY = 0xFF8A3C;
constexpr uint32_t PRIMARY_DIM = 0x3A2A1B;
constexpr uint32_t SUCCESS = 0x5BD68A;
constexpr uint32_t SUCCESS_DIM = 0x17301F;
constexpr uint32_t WARN = 0xF5B942;
constexpr uint32_t DANGER = 0xEF5A5A;

inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

void init(const std::string& font_dir);

lv_font_t* font(int size, bool bold = false);
lv_font_t* icon(int size);

} // namespace theme

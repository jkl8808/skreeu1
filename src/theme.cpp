#include "theme.h"

#include <cstdio>
#include <map>
#include <string>

namespace theme {

namespace {
std::map<int, lv_font_t*> g_regular;
std::map<int, lv_font_t*> g_bold;
std::map<int, lv_font_t*> g_icons;
std::string g_dir = "assets/fonts";

lv_font_t* make(const std::string& path, int size, lv_freetype_font_style_t style) {
    lv_font_t* f = lv_freetype_font_create(path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                          (uint32_t)size, style);
    if (!f) {
        LV_LOG_ERROR("font load failed: %s (%d)", path.c_str(), size);
    }
    return f;
}
} // namespace

void init(const std::string& font_dir) {
    g_dir = font_dir;
}

lv_font_t* font(int size, bool bold) {
    auto& cache = bold ? g_bold : g_regular;
    auto it = cache.find(size);
    if (it != cache.end()) return it->second;

    std::string file = bold ? "/NotoSansCJKsc-Bold.otf" : "/NotoSansCJKsc-Regular.otf";
    lv_font_t* f = make(g_dir + file, size,
                        bold ? LV_FREETYPE_FONT_STYLE_NORMAL : LV_FREETYPE_FONT_STYLE_NORMAL);
    cache[size] = f;
    return f;
}

lv_font_t* icon(int size) {
    auto it = g_icons.find(size);
    if (it != g_icons.end()) return it->second;
    lv_font_t* f = make(g_dir + "/materialdesignicons-webfont.ttf", size,
                        LV_FREETYPE_FONT_STYLE_NORMAL);
    g_icons[size] = f;
    return f;
}

} // namespace theme

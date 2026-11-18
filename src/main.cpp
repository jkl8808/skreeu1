#include "app.h"
#include "theme.h"
#include "ui.h"

#include <lvgl.h>
#include <png.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

#if U1UI_DEVICE
#include "lv_evdev.h"
#include "lv_linux_fbdev.h"
#else
extern "C" lv_display_t* lv_sdl_window_create(int32_t hor_res, int32_t ver_res);
extern "C" lv_indev_t* lv_sdl_mouse_create(void);
#endif

namespace {

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 320;

uint32_t g_fb[SCREEN_W * SCREEN_H];

uint32_t tick_ms() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - start).count();
}

void flush_headless(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
    (void)area;
    std::memcpy(g_fb, px, sizeof(g_fb));
    lv_display_flush_ready(disp);
}

void save_png(const char* path) {
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        return;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    png_init_io(png, fp);
    png_set_IHDR(png, info, SCREEN_W, SCREEN_H, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    auto* row = new uint8_t[SCREEN_W * 3];
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t p = g_fb[y * SCREEN_W + x];  // 0xAARRGGBB
            row[x * 3 + 0] = (p >> 16) & 0xFF;
            row[x * 3 + 1] = (p >> 8) & 0xFF;
            row[x * 3 + 2] = p & 0xFF;
        }
        png_write_row(png, row);
    }
    delete[] row;
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    printf("saved %s (%dx%d)\n", path, SCREEN_W, SCREEN_H);
}

void parse_args(int argc, char** argv, Config& cfg, std::string& shot_path, int& page,
                int& scroll, int& bench, int& sheet, bool& picker, bool& temp_page,
                bool& bed_temp, bool& keypad, bool& nozzle, bool& color, bool& toast,
                int& slot) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) {
            shot_path = argv[++i];
        } else if (a == "--page" && i + 1 < argc) {
            page = atoi(argv[++i]);
        } else if (a == "--scroll" && i + 1 < argc) {
            scroll = atoi(argv[++i]);
        } else if (a == "--bench" && i + 1 < argc) {
            bench = atoi(argv[++i]);
        } else if (a == "--sheet" && i + 1 < argc) {
            sheet = atoi(argv[++i]);
        } else if (a == "--picker") {
            picker = true;
        } else if (a == "--nozzle") {
            nozzle = true;
        } else if (a == "--color") {
            color = true;
        } else if (a == "--toast") {
            toast = true;
        } else if (a == "--temp") {
            temp_page = true;
        } else if (a == "--bed") {
            bed_temp = true;
        } else if (a == "--keypad") {
            keypad = true;
        } else if (a == "--slot" && i + 1 < argc) {
            slot = atoi(argv[++i]);
        } else if (a == "--host" && i + 1 < argc) {
            cfg.host = argv[++i];
        } else if (a == "--port" && i + 1 < argc) {
            cfg.port = atoi(argv[++i]);
        } else if (a == "--readonly") {
            cfg.read_only = true;
        } else if (a == "--fonts" && i + 1 < argc) {
            cfg.font_dir = argv[++i];
        }
    }
    if (const char* h = getenv("U1UI_HOST")) cfg.host = h;
    if (const char* p = getenv("U1UI_PORT")) cfg.port = atoi(p);
    if (const char* r = getenv("U1UI_READONLY")) cfg.read_only = (atoi(r) != 0);
    if (const char* f = getenv("U1UI_FONTS")) cfg.font_dir = f;
    if (const char* pi = getenv("U1UI_PRODUCT_INFO")) cfg.product_info = pi;
}

} // namespace

int main(int argc, char** argv) {
    Config cfg;
    std::string shot_path;
    int page = 0;
    int scroll = 0;
    int bench = 0;
    int sheet = -1;
    bool picker = false;
    bool temp_page = false;
    bool bed_temp = false;
    bool keypad = false;
    bool nozzle = false;
    bool color = false;
    bool toast = false;
    int slot = -1;
    parse_args(argc, argv, cfg, shot_path, page, scroll, bench, sheet, picker, temp_page,
               bed_temp, keypad, nozzle, color, toast, slot);

    lv_init();
    lv_tick_set_cb(tick_ms);
    theme::init(cfg.font_dir);

    lv_display_t* disp = nullptr;
    if (!shot_path.empty() && bench <= 0) {
        static uint8_t draw_buf[SCREEN_W * SCREEN_H * 4];
        disp = lv_display_create(SCREEN_W, SCREEN_H);
        lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
        lv_display_set_buffers(disp, draw_buf, nullptr, sizeof(draw_buf),
                               LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(disp, flush_headless);
    } else {
#if U1UI_DEVICE
        // U1 面板：/dev/fb0 (rockchipdrmfb, 480x320x32) + tlsc6x 触摸在 event0
        const char* fb = getenv("U1UI_FB");
        if (!fb) fb = "/dev/fb0";
        disp = lv_linux_fbdev_create();
        if (disp) {
            lv_linux_fbdev_set_file(disp, fb);
            lv_display_set_default(disp);
        } else {
            fprintf(stderr, "fbdev create failed\n");
        }
        const char* touch = getenv("U1UI_TOUCH");
        if (!touch) touch = "/dev/input/event0";
        if (!lv_evdev_create(LV_INDEV_TYPE_POINTER, touch))
            fprintf(stderr, "touch open failed: %s\n", touch);
        else
            fprintf(stderr, "touch ok: %s\n", touch);
#else
        disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
        lv_sdl_mouse_create();
#endif
    }

    Moonraker mr(cfg);
    mr.start();

    ui_init(&mr, lv_screen_active());
    ui_show(page);

    lv_timer_create([](lv_timer_t*) { ui_tick(); }, 500, nullptr);

    if (bench > 0) {
        // 刷新性能基准：每帧滚动当前页再整屏刷新，测面板实际能达到的帧率
        using namespace std::chrono;
        for (int i = 0; i < 20; i++) {  // 预热：等字体、首屏缓存就绪
            lv_timer_handler();
            lv_refr_now(disp);
        }
        auto t0 = steady_clock::now();
        for (int i = 0; i < bench; i++) {
            lv_obj_update_layout(lv_screen_active());
            ui_scroll_active((i % 2) ? 6 : -6);       // 内容变化，避免命中缓存
            lv_obj_invalidate(lv_screen_active());     // 强制整屏重绘
            lv_refr_now(disp);
            lv_timer_handler();
        }
        auto dt = duration_cast<microseconds>(steady_clock::now() - t0).count();
        double ms = dt / 1000.0 / bench;
        printf("bench: %.2f ms/frame -> %.1f fps (%d frames)\n", ms, 1000.0 / ms, bench);
        mr.stop();
        return 0;
    }

    if (!shot_path.empty()) {
        // give the poller time to fill state, then render a few frames
        std::this_thread::sleep_for(std::chrono::milliseconds(1800));
        for (int i = 0; i < 12; i++) {
            lv_timer_handler();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        auto render_frames = [](int n) {
            for (int i = 0; i < n; i++) {
                lv_timer_handler();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        };
        if (sheet >= 0) {
            ui_show(0);
            ui_open_tool(sheet);
            render_frames(6);
        }
        if (bed_temp) {
            ui_show(0);
            ui_open_bed_temp();
            render_frames(6);
        }
        if (temp_page) {
            ui_show(0);
            ui_open_temp();
            render_frames(6);
        }
        if (keypad) {
            ui_open_keypad(slot);
            render_frames(6);
        }
        if (picker) {
            ui_open_picker();
            render_frames(6);
        }
        if (nozzle) {
            ui_open_nozzle();
            render_frames(6);
        }
        if (color) {
            ui_open_color();
            render_frames(6);
        }
        if (toast) {
            ui_toast("自动进料切换失败，已恢复");
            render_frames(6);
        }
        if (scroll != 0) {
            lv_obj_update_layout(lv_screen_active());
            ui_scroll_active(scroll);
            for (int i = 0; i < 6; i++) {
                lv_timer_handler();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        lv_refr_now(disp);
        save_png(shot_path.c_str());
        mr.stop();
        return 0;
    }

    while (true) {
        lv_timer_handler();
        usleep(5000);
    }
    return 0;
}

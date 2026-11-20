#include "ui.h"
#include "wifi.h"

#include "app.h"
#include "theme.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Material Design Icons (bundled TTF, Supplementary PUA-A)
constexpr const char* IC_HOME = "\U000F02DC";
constexpr const char* IC_PRINT = "\U000F042B";
constexpr const char* IC_WRENCH = "\U000F1323";
constexpr const char* IC_COG = "\U000F0493";
constexpr const char* IC_PLAY = "\U000F040A";
constexpr const char* IC_PAUSE = "\U000F03E4";
constexpr const char* IC_STOP = "\U000F04DB";
constexpr const char* IC_CHECK = "\U000F012C";
constexpr const char* IC_LIGHT = "\U000F0335";
constexpr const char* IC_NFC = "\U000F0396";
constexpr const char* IC_CLOSE = "\U000F0156";
constexpr const char* IC_LOAD = "\U000F0120";
constexpr const char* IC_UNLOAD = "\U000F011D";
constexpr const char* IC_PALETTE = "\U000F03D8";
constexpr const char* IC_THERMO = "\U000F050F";
constexpr const char* IC_BED = "\U000F0438";  // radiator：热床 + 热气
constexpr const char* IC_WIFI = "\U000F05A9";
constexpr const char* IC_WIFI_1 = "\U000F091F";
constexpr const char* IC_WIFI_2 = "\U000F0922";
constexpr const char* IC_WIFI_3 = "\U000F0925";
constexpr const char* IC_WIFI_4 = "\U000F0928";
constexpr const char* IC_WIFI_OFF = "\U000F05AA";
constexpr const char* IC_LOCK = "\U000F033E";
constexpr const char* IC_REFRESH = "\U000F0450";
constexpr const char* IC_CHECK_CIRCLE = "\U000F05E0";
constexpr const char* IC_TOOLHEAD = "\U000F0E5B";  // printer-3d-nozzle
constexpr const char* IC_CONTROL = "\U000F0297";  // gamepad-variant
constexpr const char* IC_ARROW_UP = "\U000F0737";
constexpr const char* IC_ARROW_DOWN = "\U000F072E";
constexpr const char* IC_ARROW_LEFT = "\U000F0731";
constexpr const char* IC_ARROW_RIGHT = "\U000F0734";
constexpr const char* IC_POWER = "\U000F0425";
constexpr const char* IC_FAN = "\U000F0210";
constexpr const char* IC_CHEVRON_RIGHT = "\U000F0142";
constexpr const char* IC_BACKSPACE = "\U000F0B5C";

Moonraker* MR = nullptr;
lv_obj_t* nav_btns[4] = {nullptr};
lv_obj_t* pages[4] = {nullptr};
int active_page = 0;
int g_sheet_tool = -1;  // 当前详情面板对应的喷嘴

// 乐观更新：刚点完开关/喷嘴参数时先用本地值显示，别等 1s 轮询
bool g_feed_pending = true;
uint32_t g_feed_at = 0;
double g_nz_dia = -1;
int g_nz_flow = -1;
uint32_t g_nz_at = 0;

// 更换耗材：左品牌 + 左下颜色 + 右耗材
std::string g_pick_brand = "generic";
uint32_t g_pick_color = 0xFFFFFF;
std::vector<MaterialOption> g_mat_view;  // 当前品牌下可选的耗材
std::vector<MaterialOption> g_custom_mats;  // 自定义类型（skreeu1_types.cfg）

// 乐观更新：刚改完耗材时先用本地值显示，别等轮询
bool g_fil_pending = false;
std::string g_fil_mat, g_fil_sub, g_fil_vendor;
uint32_t g_fil_argb = 0xFFFFFF;
uint32_t g_fil_at = 0;

const uint32_t kPalette[28] = {
    0xE53935, 0xF4511E, 0xFB8C00, 0xFFB300, 0xFDD835, 0xC0CA33, 0x7CB342,
    0x43A047, 0x00897B, 0x00ACC1, 0x039BE5, 0x1E88E5, 0x3949AB, 0x5E35B1,
    0x8E24AA, 0xD81B60, 0xF06292, 0x6D4C41, 0xFFFFFF, 0xCFD8DC, 0x90A4AE,
    0x546E7A, 0x37474F, 0x212121, 0xF5F5DC, 0x827717, 0x4E342E, 0x1A237E};

void update_sheet();
void update_nozzle_page();
void show_toast(const char* msg);
void open_print_sheet_cb(lv_event_t* e);
void ctrl_tab_cb(lv_event_t* e);
void ctrl_light_cb(lv_event_t* e);
std::string feed_title(const std::string& act, bool finished, bool failed, int tool);
void feed_banner_set(bool error, bool success, const char* icon, const char* title,
                     const char* sub);
void feed_tick(const PrinterState& s);
void notice_show(bool error, bool success, const char* icon, const std::string& title,
                 const std::string& sub, uint32_t duration_ms);
void notice_error(const std::string& title, const std::string& sub);
void update_feed_banner_pos();
void feed_banner_gesture_cb(lv_event_t* e);
void open_net_page();
void net_card_cb(lv_event_t* e);

int g_nozzle_feed_mode[4] = {0, 0, 0, 0};  // 0 未进料 1 已进料 2 进料中 3 退料中 4 失败
bool g_feed_local = false;
int g_feed_local_tool = 0;
int g_feed_local_kind = 0;   // 1=进料 2=退料
int g_feed_pref_tool = -1;   // 本次操作对应的喷嘴，优先显示它的状态
uint32_t g_feed_local_at = 0;
bool g_feed_was_active = false;
int g_feed_active_tool = 0;
std::string g_feed_active_state;
uint32_t g_feed_done_until = 0;
uint32_t g_notice_until = 0;   // 顶部通知的显示截止时间（0 表示由 feed 持续占用）
std::string g_feed_err_seen;

int g_ctrl_tab = 0;
bool g_feed_dismissed = false;
int g_move_step = 10;
bool g_home_pending = false;
uint32_t g_home_at = 0;
bool g_fan_pending[3] = {false, false, false};
int g_fan_val[3] = {0, 0, 0};
uint32_t g_fan_at[3] = {0, 0, 0};
bool g_fan_dragging[3] = {false, false, false};
int g_fan_page = 0;
bool g_light_pending = false;
bool g_light_val = false;
uint32_t g_light_at = 0;

Wifi* WIFI = nullptr;
bool g_net_open = false;
bool g_net_pw_open = false;
uint32_t g_net_req_at = 0;
uint32_t g_net_scan_at = 0;
uint32_t g_net_scan_gen = 0;
std::string g_net_last_ssid;
bool g_net_last_conn = false;
std::string g_net_pw_ssid;
bool g_net_pw_sae = false;
std::string g_net_last_error;
std::string g_net_target;
bool g_wifi_restart_seen = false;

uint32_t g_toast_until = 0;
std::vector<std::string> g_brands;

// 喷嘴参数的乐观值：等轮询结果和本地值一致（或 8 秒超时）再解除
void nozzle_view(double& nd, bool& hf) {
    if (g_nz_at == 0) return;
    bool confirmed = (g_nz_dia <= 0 || fabs(nd - g_nz_dia) < 0.01) &&
                     (g_nz_flow < 0 || hf == (g_nz_flow == 1));
    if (confirmed || lv_tick_get() - g_nz_at > 8000) {
        if (!confirmed) notice_error("喷嘴参数未生效", "已恢复原来的设置");
        g_nz_dia = -1;
        g_nz_flow = -1;
        g_nz_at = 0;
        return;
    }
    if (g_nz_dia > 0) nd = g_nz_dia;
    if (g_nz_flow >= 0) hf = (g_nz_flow == 1);
}

struct Refs {
    lv_obj_t* status_card = nullptr;
    lv_obj_t* status_title = nullptr;
    lv_obj_t* status_sub = nullptr;
    lv_obj_t* status_sub2 = nullptr;
    lv_obj_t* status_arc = nullptr;
    lv_obj_t* status_pct = nullptr;
    lv_obj_t* status_check = nullptr;
    lv_obj_t* temp_bed = nullptr;
    lv_obj_t* bed_target = nullptr;

    // 首页喷嘴卡片
    lv_obj_t* nozzle_card[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_swatch[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_temp[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_target[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_mat[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_feed[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_feed_line[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_feed_head[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nozzle_nfc[4] = {nullptr, nullptr, nullptr, nullptr};

    // 喷嘴详情面板
    lv_obj_t* sheet = nullptr;
    lv_obj_t* sheet_title = nullptr;
    lv_obj_t* sheet_nfc = nullptr;
    lv_obj_t* sheet_temp_card = nullptr;
    lv_obj_t* sheet_temp_now = nullptr;
    lv_obj_t* sheet_temp_target = nullptr;
    lv_obj_t* sheet_swatch = nullptr;
    lv_obj_t* sheet_mat = nullptr;
    lv_obj_t* sheet_vendor = nullptr;
    lv_obj_t* sheet_spec = nullptr;
    lv_obj_t* sheet_spec2 = nullptr;
    lv_obj_t* sheet_lock = nullptr;

    // 半屏温度页
    lv_obj_t* scrim = nullptr;
    lv_obj_t* temp_page = nullptr;
    lv_obj_t* tp_title = nullptr;
    lv_obj_t* tp_big = nullptr;
    lv_obj_t* tp_now = nullptr;
    lv_obj_t* tp_slider = nullptr;
    lv_obj_t* tp_fill = nullptr;
    lv_obj_t* tp_knob = nullptr;
    lv_obj_t* tp_chip[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    // 数字键盘
    lv_obj_t* keypad_scrim = nullptr;
    lv_obj_t* keypad = nullptr;
    lv_obj_t* keypad_title = nullptr;
    lv_obj_t* keypad_value = nullptr;

    // 材料选择面板
    lv_obj_t* picker = nullptr;
    lv_obj_t* picker_title = nullptr;
    lv_obj_t* picker_count = nullptr;
    lv_obj_t* picker_brand_list = nullptr;
    lv_obj_t* picker_color = nullptr;
    lv_obj_t* picker_list = nullptr;

    // 颜色选择面板
    lv_obj_t* color_scrim = nullptr;
    lv_obj_t* color_page = nullptr;
    lv_obj_t* color_sw[28] = {};

    // 轻提示
    lv_obj_t* toast = nullptr;
    lv_obj_t* toast_label = nullptr;
    lv_obj_t* print_arc = nullptr;
    lv_obj_t* print_pct = nullptr;
    lv_obj_t* print_file = nullptr;
    lv_obj_t* print_meta = nullptr;
    lv_obj_t* print_temps = nullptr;
    lv_obj_t* speed_label = nullptr;
    lv_obj_t* zoff_label = nullptr;
    lv_obj_t* filter_bar = nullptr;
    lv_obj_t* pur_hours = nullptr;
    lv_obj_t* pur_rpm = nullptr;
    lv_obj_t* conn_line = nullptr;
    lv_obj_t* conn_state = nullptr;
    lv_obj_t* dev_line = nullptr;
    lv_obj_t* dev_nozzle = nullptr;
    lv_obj_t* gui_line = nullptr;
    lv_obj_t* stat_line = nullptr;

    // 自动进料 / 喷嘴类型（详情页里当前喷嘴）
    lv_obj_t* sheet_feed_sw = nullptr;
    lv_obj_t* sheet_feed_hint = nullptr;
    lv_obj_t* sheet_nozzle_label = nullptr;

    // 喷嘴参数弹层
    lv_obj_t* nz_scrim = nullptr;
    lv_obj_t* nz_page = nullptr;
    lv_obj_t* nz_title = nullptr;
    lv_obj_t* nz_dia_chip[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* nz_type_chip[2] = {nullptr, nullptr};
    lv_obj_t* pref_replenish_sw = nullptr;
    lv_obj_t* pref_ignore_color_sw = nullptr;
    lv_obj_t* pref_end_led_sw = nullptr;

    // 打印偏好
    lv_obj_t* pref_bedlevel_sw = nullptr;
    lv_obj_t* pref_flow_sw = nullptr;
    lv_obj_t* pref_shaper_sw = nullptr;
    lv_obj_t* pref_timelapse_sw = nullptr;

    // 维护
    lv_obj_t* head_line = nullptr;
    lv_obj_t* head_nozzle_line = nullptr;
    lv_obj_t* pur_speed = nullptr;
    lv_obj_t* pur_speed_dec = nullptr;
    lv_obj_t* pur_speed_inc = nullptr;
    lv_obj_t* pur_mode_line = nullptr;
    lv_obj_t* pur_mode_btn[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* ent_sw = nullptr;
    lv_obj_t* ent_seg[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* def_sw = nullptr;
    lv_obj_t* def_group_sw[4] = {nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* def_seg[2] = {nullptr, nullptr};

    // 首页快捷打印
    lv_obj_t* home_pause_btn = nullptr;
    lv_obj_t* home_resume_btn = nullptr;
    lv_obj_t* home_stop_btn = nullptr;
    lv_obj_t* home_print_btn = nullptr;

    // 打印控制浮层
    lv_obj_t* print_sheet = nullptr;

    // 控制页
    lv_obj_t* ctrl_body = nullptr;
    lv_obj_t* ctrl_tab[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_chip[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_step_chip[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_light_btn = nullptr;
    lv_obj_t* ctrl_light_icon = nullptr;
    lv_obj_t* ctrl_light_text = nullptr;
    lv_obj_t* ctrl_fan_pct[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_fan_slider[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_fan_icon[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* ctrl_fan_page0 = nullptr;
    lv_obj_t* ctrl_fan_page1 = nullptr;
    lv_obj_t* ctrl_fan_page_label = nullptr;
    lv_obj_t* move_hint = nullptr;
    lv_obj_t* move_btn[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    // 进料/退料状态通知
    lv_obj_t* feed_banner = nullptr;
    lv_obj_t* feed_banner_icon = nullptr;
    lv_obj_t* feed_banner_title = nullptr;
    lv_obj_t* feed_banner_sub = nullptr;
    lv_obj_t* feed_banner_spin = nullptr;

    // 网络
    lv_obj_t* net_line = nullptr;
    lv_obj_t* net_page = nullptr;
    lv_obj_t* net_status = nullptr;
    lv_obj_t* net_sub = nullptr;
    lv_obj_t* net_list = nullptr;
    lv_obj_t* net_scan_btn = nullptr;
    lv_obj_t* net_rec_btn = nullptr;
    lv_obj_t* net_dis_btn = nullptr;
    lv_obj_t* net_pw_page = nullptr;
    lv_obj_t* net_pw_title = nullptr;
    lv_obj_t* net_pw_ta = nullptr;
    lv_obj_t* net_pw_kb = nullptr;
} R;

// ---------------------------------------------------------------- helpers

lv_obj_t* mk_obj(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

void style_card(lv_obj_t* o, uint32_t bg) {
    lv_obj_set_style_bg_color(o, theme::c(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 14, 0);
    lv_obj_set_style_pad_all(o, 10, 0);
    lv_obj_set_style_border_width(o, 0, 0);
}

lv_obj_t* mk_card(lv_obj_t* parent, uint32_t bg = theme::SURFACE) {
    lv_obj_t* o = mk_obj(parent);
    style_card(o, bg);
    return o;
}

lv_obj_t* mk_label(lv_obj_t* parent, const char* txt, int size, bool bold, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, theme::font(size, bold), 0);
    lv_obj_set_style_text_color(l, theme::c(color), 0);
    return l;
}

lv_obj_t* mk_icon(lv_obj_t* parent, const char* cp, int size, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, cp);
    lv_obj_set_style_text_font(l, theme::icon(size), 0);
    lv_obj_set_style_text_color(l, theme::c(color), 0);
    return l;
}

void send_action(const char* gcode) {
    if (MR) MR->send_gcode(gcode);
}

void action_cb(lv_event_t* e) {
    const char* g = static_cast<const char*>(lv_event_get_user_data(e));
    if (g) send_action(g);
}

lv_obj_t* mk_mini_button(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, void* ud) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 40, 34);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, theme::font(16, true), 0);
    lv_obj_set_style_text_color(l, theme::c(theme::TEXT), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

lv_obj_t* mk_button(lv_obj_t* parent, const char* txt, bool primary, lv_event_cb_t cb,
                    void* ud = nullptr, int h = 46) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_height(b, h);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, theme::c(primary ? theme::PRIMARY : theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 4, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, theme::font(14, false), 0);
    lv_obj_set_style_text_color(l, theme::c(primary ? 0x201005 : theme::TEXT), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

// 一行：左标题、右控件
lv_obj_t* mk_row(lv_obj_t* parent, int h) {
    lv_obj_t* r = mk_obj(parent);
    lv_obj_set_size(r, LV_PCT(100), h);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    return r;
}

void apply_toggle(lv_obj_t* sw, bool on) {
    if (!sw) return;
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, theme::c(on ? theme::PRIMARY : theme::SURFACE_HI), 0);
    lv_obj_set_style_border_color(sw, theme::c(on ? theme::PRIMARY : theme::OUTLINE), 0);
    lv_obj_t* knob = lv_obj_get_child(sw, 0);
    if (knob) {
        lv_obj_set_style_bg_color(knob, theme::c(on ? 0xFFFFFF : theme::TEXT), 0);
        lv_obj_align(knob, on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, on ? -3 : 3, 0);
    }
}

void set_disabled(lv_obj_t* b, bool dis) {
    if (!b) return;
    lv_obj_set_style_opa(b, dis ? LV_OPA_40 : LV_OPA_COVER, 0);
    if (dis) lv_obj_add_state(b, LV_STATE_DISABLED);
    else lv_obj_remove_state(b, LV_STATE_DISABLED);
}

// 自绘 M3 开关（LVGL 默认 switch 在 480×320 上比例不合适）
void toggle_click_cb(lv_event_t* e) {
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    apply_toggle(sw, !lv_obj_has_state(sw, LV_STATE_CHECKED));
}

lv_obj_t* mk_switch(lv_obj_t* parent, bool on, lv_event_cb_t cb, void* ud) {
    lv_obj_t* sw = mk_obj(parent);
    lv_obj_set_size(sw, 46, 26);
    lv_obj_set_style_radius(sw, 999, 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* knob = mk_obj(sw);
    lv_obj_set_size(knob, 20, 20);
    lv_obj_set_style_radius(knob, 999, 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_CLICKABLE);  // 点白点也要能触发整个开关
    lv_obj_align(knob, LV_ALIGN_LEFT_MID, 3, 0);

    lv_obj_add_event_cb(sw, toggle_click_cb, LV_EVENT_CLICKED, nullptr);
    if (cb) lv_obj_add_event_cb(sw, cb, LV_EVENT_CLICKED, ud);
    apply_toggle(sw, on);
    return sw;
}

// 小胶囊按钮，用于分段选择（灵敏度 / 喷嘴类型 / 净化器模式）
// w <= 0 时按 flex 平分剩余宽度
lv_obj_t* mk_chip(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, void* ud, int w = 52,
                  int h = 30) {
    lv_obj_t* b = lv_button_create(parent);
    if (w > 0) lv_obj_set_size(b, w, h);
    else {
        lv_obj_set_height(b, h);
        lv_obj_set_flex_grow(b, 1);
    }
    lv_obj_set_style_radius(b, 999, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, theme::font(12, false), 0);
    lv_obj_set_style_text_color(l, theme::c(theme::MUTED), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

void style_chip(lv_obj_t* b, bool on) {
    if (!b) return;
    lv_obj_set_style_bg_color(b, theme::c(on ? theme::PRIMARY_DIM : theme::SURFACE_HI), 0);
    lv_obj_t* l = lv_obj_get_child(b, 0);
    if (l) lv_obj_set_style_text_color(l, theme::c(on ? theme::PRIMARY : theme::MUTED), 0);
}

void set_sw(lv_obj_t* sw, bool on) {
    if (!sw) return;
    bool cur = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (cur == on) return;
    apply_toggle(sw, on);
}

void set_chip_row(lv_obj_t** chips, int n, int active) {
    for (int i = 0; i < n; i++) style_chip(chips[i], i == active);
}

void nav_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ui_show(idx);
}

std::string fmt_hours(double h) {
    char buf[32];
    if (h < 10) snprintf(buf, sizeof(buf), "%.1f h", h);
    else snprintf(buf, sizeof(buf), "%.0f h", h);
    return buf;
}

std::string short_name(const std::string& in, size_t max_len = 30) {
    std::string name = in;
    size_t slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() <= max_len) return name;
    size_t head = max_len / 2 - 2;
    size_t tail = max_len - head - 3;
    return name.substr(0, head) + "..." + name.substr(name.size() - tail);
}

std::string fmt_time(double s) {
    char buf[32];
    int total = (int)s;
    snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

// ---------------------------------------------------------------- nav bar

void style_nav(int active) {
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = nav_btns[i];
        if (!b) continue;
        bool on = (i == active);
        lv_obj_set_style_bg_color(b, theme::c(on ? theme::PRIMARY_DIM : theme::BG), 0);
        lv_obj_set_style_bg_opa(b, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        for (uint32_t j = 0; j < lv_obj_get_child_count(b); j++) {
            lv_obj_t* c = lv_obj_get_child(b, (int32_t)j);
            uint32_t col = on ? theme::PRIMARY : theme::MUTED;
            lv_obj_set_style_text_color(c, theme::c(col), 0);
        }
    }
}

void speed_step_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    if (!MR) return;
    int v = (int)lround(MR->snapshot().speed_factor * 100) + dir * 5;
    if (v < 20) v = 20;
    if (v > 300) v = 300;
    MR->send_gcode("M220 S" + std::to_string(v));
}

void z_step_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    if (!MR) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "SET_GCODE_OFFSET Z_ADJUST=%.2f MOVE=1", dir * 0.02);
    MR->send_gcode(buf);
}

// ---------------------------------------------------------------- Snapmaker 扩展回调

bool is_busy() {
    if (!MR) return false;
    PrinterState s = MR->snapshot();
    return s.print_state == "printing" || s.print_state == "paused";
}

void send_feed(bool on) {
    if (g_sheet_tool < 0 || g_sheet_tool > 3) return;
    g_feed_pending = on;
    g_feed_at = lv_tick_get();
    // 和原厂 GUI 一致：由固件按 EXTRUDER 自己映射 left/right 和 channel
    char buf[64];
    snprintf(buf, sizeof(buf), "AUTO_FEEDING EXTRUDER=%d AUTO=%d",
             g_sheet_tool, on ? 1 : 0);
    send_action(buf);
}

void sheet_feed_cb(lv_event_t* e) {
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    send_feed(lv_obj_has_state(sw, LV_STATE_CHECKED));
}

// 点卡片空白处也能开关
void feed_card_cb(lv_event_t*) {
    if (!R.sheet_feed_sw) return;
    bool on = !lv_obj_has_state(R.sheet_feed_sw, LV_STATE_CHECKED);
    apply_toggle(R.sheet_feed_sw, on);
    send_feed(on);
}

// 喷嘴参数弹层：直径 / 流量
const double kNozzleDias[4] = {0.2, 0.4, 0.6, 0.8};

void nozzle_send(const char* key, const char* value) {
    if (g_sheet_tool < 0 || g_sheet_tool > 3) return;
    if (is_busy()) return;  // 打印中不允许换喷嘴参数
    static const char* names[] = {"extruder", "extruder1", "extruder2", "extruder3"};
    char buf[96];
    snprintf(buf, sizeof(buf), "SET_NOZZLE_PROPERTIES EXTRUDER=%s %s=%s", names[g_sheet_tool],
             key, value);
    send_action(buf);
}

void nozzle_dia_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 3) return;
    g_nz_dia = kNozzleDias[i];
    g_nz_at = lv_tick_get();
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", kNozzleDias[i]);
    nozzle_send("DIAMETER", buf);
    update_nozzle_page();
    update_sheet();
}

void nozzle_flow_cb(lv_event_t* e) {
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    g_nz_flow = v;
    g_nz_at = lv_tick_get();
    nozzle_send("VOLUME_TYPE", v ? "high_flow" : "standard");
    update_nozzle_page();
    update_sheet();
}

// 通用布尔偏好：user_data 是 SET_PRINT_PREFERENCES 的参数名
void pref_switch_cb(lv_event_t* e) {
    const char* key = static_cast<const char*>(lv_event_get_user_data(e));
    if (!key) return;
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    std::string cmd = std::string("SET_PRINT_PREFERENCES ") + key + "=" + (on ? "1" : "0");
    send_action(cmd.c_str());
}

void entangle_sens_cb(lv_event_t* e) {
    static const char* vals[] = {"low", "medium", "high"};
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 2) return;
    std::string cmd = std::string("SET_PRINT_PREFERENCES FILAMENT_ENTANGLE_SEN=") + vals[i];
    send_action(cmd.c_str());
}

void defect_main_cb(lv_event_t* e) {
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    std::string cmd = std::string("DEFECT_DETECTION_CONFIG MAIN_ENABLE=") + (on ? "1" : "0");
    send_action(cmd.c_str());
}

void defect_group_cb(lv_event_t* e) {
    static const char* keys[] = {"CLEAN_BED_ENABLE", "NOODLE_ENABLE", "RESIDUE_ENABLE",
                                 "NOZZLE_ENABLE"};
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 3) return;
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    std::string cmd = std::string("DEFECT_DETECTION_CONFIG ") + keys[i] + "=" + (on ? "1" : "0");
    send_action(cmd.c_str());
}

void defect_sens_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    std::string cmd = std::string("DEFECT_DETECTION_CONFIG SENSITIVITY=") +
                      (i == 1 ? "high" : "low");
    send_action(cmd.c_str());
}

void purifier_speed_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    if (!MR) return;
    PrinterState s = MR->snapshot();
    if (!s.purifier_present) return;  // 未检测到净化器时不下发，避免 Klipper 报错
    int v = (int)lround(s.purifier_inner_speed) + dir * 10;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    char buf[64];
    snprintf(buf, sizeof(buf), "SET_PURIFIER FAN=inner SPEED=%d", v);
    send_action(buf);
}

void purifier_mode_cb(lv_event_t* e) {
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    if (!MR || !MR->snapshot().purifier_present) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "SET_PURIFIER_MODE MODE=%d", mode);
    send_action(buf);
}


// ---------------------------------------------------------------- 喷嘴详情

std::vector<MaterialOption> g_mat_opts;

std::string vendor_text(const std::string& v) {
    return (v.empty() || v == "generic") ? "通用" : v;
}

std::string sub_text(const std::string& s) {
    return (s.empty() || s == "generic") ? "" : s;
}

// G-code 参数用空格分隔，带空格的品牌名要加引号（如 "Bambu Lab"）
std::string gcode_arg(const std::string& s) {
    if (s.find_first_of(" \t") == std::string::npos) return s;
    return "\"" + s + "\"";
}

void set_swatch(lv_obj_t* o, const FilamentInfo& f) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, theme::c(f.present ? (f.argb & 0xFFFFFF) : 0x3A3F49), 0);
    lv_obj_set_style_bg_opa(o, f.present ? LV_OPA_COVER : LV_OPA_30, 0);
}

int g_temp_kind = 0;   // 0 = 喷嘴, 1 = 热床
int g_slider_max = 300;
bool g_temp_pending = false;
int g_temp_pending_kind = 0;
int g_temp_pending_tool = -1;
int g_temp_pending_val = 0;
uint32_t g_temp_pending_at = 0;

void temp_apply(int temp) {
    if (!MR) return;
    char buf[48];
    if (g_temp_kind == 1) {
        snprintf(buf, sizeof(buf), "M140 S%d", temp);
    } else if (g_sheet_tool <= 0) {
        snprintf(buf, sizeof(buf), "M104 S%d", temp);
    } else {
        snprintf(buf, sizeof(buf), "M104 T%d S%d", g_sheet_tool, temp);
    }
    if (!MR->config().read_only) {
        g_temp_pending = true;
        g_temp_pending_kind = g_temp_kind;
        g_temp_pending_tool = g_sheet_tool;
        g_temp_pending_val = temp;
        g_temp_pending_at = lv_tick_get();
    }
    MR->send_gcode(buf);
}

// 调温（半屏）

// 前置声明：这两个辅助函数在下面实现
lv_obj_t* mk_overlay(lv_obj_t* screen);
lv_obj_t* mk_close_button(lv_obj_t* parent, lv_event_cb_t cb);

// 自定义快捷温度：所有喷嘴共用，持久化到工作目录下的 skreeu1_temps.cfg
int g_presets[6] = {180, 200, 220, 240, 260, 280};
int g_bed_presets[6] = {60, 70, 80, 90, 100, 110};
int g_edit_slot = -1;          // >=0：数字键盘正在编辑该快捷温度
int g_slider_val = 0;          // 滑块当前目标温度
bool g_slider_dragging = false;
uint32_t g_slider_apply_at = 0;
int g_keypad_val = 0;
bool g_keypad_fresh = true;

const char* preset_path() {
    const char* p = getenv("SKREEU1_TEMPS");
    if (p) return p;
#ifdef SKREEU1_DEVICE
    return "/userdata/skreeu1/skreeu1_temps.cfg";
#else
    return "skreeu1_temps.cfg";
#endif
}

const char* bed_preset_path() {
    const char* p = getenv("SKREEU1_BED_TEMPS");
    if (p) return p;
#ifdef SKREEU1_DEVICE
    return "/userdata/skreeu1/skreeu1_bed_temps.cfg";
#else
    return "skreeu1_bed_temps.cfg";
#endif
}

void load_bed_presets() {
    FILE* f = fopen(bed_preset_path(), "r");
    if (!f) return;
    for (int i = 0; i < 6; i++) {
        int v = 0;
        if (fscanf(f, "%d", &v) != 1) break;
        if (v < 0) v = 0;
        if (v > 120) v = 120;
        g_bed_presets[i] = v;
    }
    fclose(f);
}

void load_presets() {
    FILE* f = fopen(preset_path(), "r");
    if (!f) return;
    for (int i = 0; i < 6; i++) {
        int v = 0;
        if (fscanf(f, "%d", &v) != 1) break;
        if (v < 0) v = 0;
        if (v > 300) v = 300;
        g_presets[i] = v;
    }
    fclose(f);
}

// 自定义品牌列表：每行一个品牌名，文件不存在时用内置常用品牌
void load_brands() {
    g_brands.clear();
    const char* p = getenv("SKREEU1_BRANDS");
#ifdef SKREEU1_DEVICE
    if (!p) p = "/userdata/skreeu1/skreeu1_brands.cfg";
#else
    if (!p) p = "skreeu1_brands.cfg";
#endif
    FILE* f = fopen(p, "r");
    if (f) {
        char line[80];
        while (fgets(line, sizeof(line), f)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
                s.pop_back();
            if (!s.empty()) g_brands.push_back(s);
        }
        fclose(f);
    }
    if (g_brands.empty()) {
        g_brands = {"generic",  "Snapmaker", "Polymaker", "eSun",    "SUNLU",
                    "Bambu Lab", "Prusament", "Creality",  "Anycubic", "Elegoo",
                    "Overture", "Jayo"};
    }
}

// 自定义类型列表：每行 "TYPE" 或 "TYPE SUBTYPE"，'#' 开头为注释
void load_custom_materials() {
    g_custom_mats.clear();
    const char* p = getenv("SKREEU1_TYPES");
#ifdef SKREEU1_DEVICE
    if (!p) p = "/userdata/skreeu1/skreeu1_types.cfg";
#else
    if (!p) p = "skreeu1_types.cfg";
#endif
    FILE* f = fopen(p, "r");
    if (!f) return;
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' ||
                              s.back() == '\t'))
            s.pop_back();
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        s = s.substr(start);
        if (s.empty() || s[0] == '#') continue;
        MaterialOption m;
        size_t sp = s.find_first_of(" \t");
        if (sp == std::string::npos) {
            m.type = s;
        } else {
            m.type = s.substr(0, sp);
            size_t ss = s.find_first_not_of(" \t", sp);
            if (ss != std::string::npos) m.sub = s.substr(ss);
        }
        if (!m.type.empty()) {
            m.custom = true;
            g_custom_mats.push_back(m);
        }
    }
    fclose(f);
}

void show_toast(const char* msg) {
    if (!R.toast || !R.toast_label) return;
    lv_label_set_text(R.toast_label, msg);
    lv_obj_remove_flag(R.toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.toast, -1);
    g_toast_until = lv_tick_get() + 2600;
}

void apply_light_ui(bool on) {
    if (R.ctrl_light_btn)
        lv_obj_set_style_bg_color(R.ctrl_light_btn,
                                  theme::c(on ? theme::PRIMARY_DIM : theme::SURFACE_HI), 0);
    if (R.ctrl_light_icon)
        lv_obj_set_style_text_color(R.ctrl_light_icon,
                                    theme::c(on ? theme::PRIMARY : theme::MUTED), 0);
    if (R.ctrl_light_text)
        lv_obj_set_style_text_color(R.ctrl_light_text,
                                    theme::c(on ? theme::TEXT : theme::MUTED), 0);
}

void ctrl_light_cb(lv_event_t*) {
    if (!MR) return;
    PrinterState s = MR->snapshot();
    bool cur = g_light_pending ? g_light_val : s.light_on;
    bool want = !cur;
    g_light_pending = true;
    g_light_val = want;
    g_light_at = lv_tick_get();
    MR->send_gcode(want ? "SET_LED LED=cavity_led WHITE=1"
                        : "SET_LED LED=cavity_led WHITE=0");
    apply_light_ui(want);
}

void save_presets() {
    FILE* f = fopen(preset_path(), "w");
    if (!f) return;
    for (int i = 0; i < 6; i++) fprintf(f, "%d%c", g_presets[i], i == 5 ? '\n' : ' ');
    fclose(f);
}

void save_bed_presets() {
    FILE* f = fopen(bed_preset_path(), "w");
    if (!f) return;
    for (int i = 0; i < 6; i++) fprintf(f, "%d%c", g_bed_presets[i], i == 5 ? '\n' : ' ');
    fclose(f);
}

void slider_set_value(int v) {
    if (v < 0) v = 0;
    if (v > g_slider_max) v = g_slider_max;
    g_slider_val = v;
    if (!R.tp_slider) return;
    lv_coord_t h = lv_obj_get_height(R.tp_slider);
    lv_coord_t fh = (lv_coord_t)((int32_t)h * v / g_slider_max);
    if (fh < 0) fh = 0;
    // 填充顶部多画 10px，藏到白色手柄下面；否则圆角会和手柄之间露出缺口
    lv_coord_t draw_h = fh;
    if (fh > 0) draw_h = fh + 10 > h ? h : fh + 10;
    lv_obj_set_height(R.tp_fill, draw_h);
    lv_obj_align(R.tp_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_align(R.tp_knob, LV_ALIGN_BOTTOM_MID, 0, -fh);
    if (R.tp_big) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d°C", v);
        lv_label_set_text(R.tp_big, buf);
    }
}

void slider_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED)
        return;
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(R.tp_slider, &a);
    int h = (int)(a.y2 - a.y1) + 1;
    int rel = (int)(p.y - a.y1);
    int v = (int)lround((1.0 - (double)rel / h) * (double)g_slider_max);
    if (v < 0) v = 0;
    if (v > g_slider_max) v = g_slider_max;
    g_slider_dragging = (code != LV_EVENT_RELEASED);
    slider_set_value(v);
    if (code == LV_EVENT_RELEASED) {
        temp_apply(v);
        g_slider_apply_at = lv_tick_get();
    }
}

void close_temp_page_cb(lv_event_t*) {
    if (R.temp_page) lv_obj_add_flag(R.temp_page, LV_OBJ_FLAG_HIDDEN);
    if (R.scrim) lv_obj_add_flag(R.scrim, LV_OBJ_FLAG_HIDDEN);
}

void temp_chip_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 5) return;
    if (g_temp_kind == 0 && g_sheet_tool < 0) return;
    const int* arr = (g_temp_kind == 1) ? g_bed_presets : g_presets;
    temp_apply(arr[i]);
    slider_set_value(arr[i]);
    g_slider_apply_at = lv_tick_get();
}

void open_keypad(bool for_preset, int slot, int initial);

void temp_chip_long_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 5) return;
    const int* arr = (g_temp_kind == 1) ? g_bed_presets : g_presets;
    open_keypad(true, i, arr[i]);
}

void temp_big_cb(lv_event_t*) {
    if (!MR) return;
    PrinterState s = MR->snapshot();
    if (g_temp_kind == 1) {
        open_keypad(false, -1, (int)lround(s.bed_target));
    } else if (g_sheet_tool >= 0) {
        open_keypad(false, -1, (int)lround(s.tools[g_sheet_tool].target));
    }
}

void refresh_temp_chips() {
    const int* arr = (g_temp_kind == 1) ? g_bed_presets : g_presets;
    for (int i = 0; i < 6; i++) {
        if (!R.tp_chip[i]) continue;
        lv_obj_t* lb = lv_obj_get_child(R.tp_chip[i], 0);
        if (!lb) continue;
        char buf[16];
        if (arr[i] == 0) snprintf(buf, sizeof(buf), "关温");
        else snprintf(buf, sizeof(buf), "%d°", arr[i]);
        lv_label_set_text(lb, buf);
    }
}

void open_temp_page(int kind) {
    if (!R.temp_page) return;
    g_temp_kind = kind;
    g_slider_max = (kind == 1) ? 120 : 300;
    refresh_temp_chips();
    if (MR) {
        PrinterState s = MR->snapshot();
        int t = 0;
        if (kind == 1) t = (int)lround(s.bed_target);
        else if (g_sheet_tool >= 0) t = (int)lround(s.tools[g_sheet_tool].target);
        slider_set_value(t);
    }
    char buf[24];
    if (kind == 1) snprintf(buf, sizeof(buf), "热床");
    else snprintf(buf, sizeof(buf), "T%d 温度", g_sheet_tool + 1);
    if (R.tp_title) lv_label_set_text(R.tp_title, buf);
    if (R.scrim) {
        lv_obj_remove_flag(R.scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.scrim, -1);
    }
    lv_obj_remove_flag(R.temp_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.temp_page, -1);
}

void open_temp_cb(lv_event_t*) {
    if (g_sheet_tool >= 0) open_temp_page(0);
}

void open_bed_temp_cb(lv_event_t*) {
    open_temp_page(1);
}

// 数字键盘
void keypad_refresh() {
    if (!R.keypad_value) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g_keypad_val);
    lv_label_set_text(R.keypad_value, buf);
}

void keypad_digit_cb(lv_event_t* e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    if (g_keypad_fresh) {
        g_keypad_val = 0;
        g_keypad_fresh = false;
    }
    if (g_keypad_val < 100) g_keypad_val = g_keypad_val * 10 + d;
    keypad_refresh();
}

void keypad_back_cb(lv_event_t*) {
    g_keypad_fresh = false;
    g_keypad_val /= 10;
    keypad_refresh();
}

void keypad_close_cb(lv_event_t*) {
    if (R.keypad) lv_obj_add_flag(R.keypad, LV_OBJ_FLAG_HIDDEN);
    if (R.keypad_scrim) lv_obj_add_flag(R.keypad_scrim, LV_OBJ_FLAG_HIDDEN);
    g_edit_slot = -1;
}

void keypad_ok_cb(lv_event_t*) {
    int v = g_keypad_val;
    int maxv = (g_temp_kind == 1) ? 120 : 300;
    if (v < 0) v = 0;
    if (v > maxv) v = maxv;
    if (g_edit_slot >= 0 && g_edit_slot <= 5) {
        if (g_temp_kind == 1) {
            g_bed_presets[g_edit_slot] = v;
            save_bed_presets();
        } else {
            g_presets[g_edit_slot] = v;
            save_presets();
        }
        refresh_temp_chips();
    } else if (g_temp_kind == 1 || g_sheet_tool >= 0) {
        temp_apply(v);
        slider_set_value(v);
        g_slider_apply_at = lv_tick_get();
    }
    keypad_close_cb(nullptr);
}

void open_keypad(bool for_preset, int slot, int initial) {
    if (!R.keypad) return;
    g_edit_slot = for_preset ? slot : -1;
    int maxv = (g_temp_kind == 1) ? 120 : 300;
    g_keypad_val = initial < 0 ? 0 : initial;
    if (g_keypad_val > maxv) g_keypad_val = maxv;
    g_keypad_fresh = true;
    if (R.keypad_title) {
        const char* title = "设置喷嘴温度";
        if (for_preset) title = "设置快捷温度";
        else if (g_temp_kind == 1) title = "设置热床温度";
        lv_label_set_text(R.keypad_title, title);
    }
    keypad_refresh();
    if (R.keypad_scrim) {
        lv_obj_remove_flag(R.keypad_scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.keypad_scrim, -1);
    }
    lv_obj_remove_flag(R.keypad, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.keypad, -1);
}

// 键盘上的大按钮
lv_obj_t* mk_key(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, void* ud,
                 bool primary) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, LV_PCT(100));
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::c(primary ? theme::PRIMARY : theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, theme::font(20, true), 0);
    lv_obj_set_style_text_color(l, theme::c(primary ? 0x201005 : theme::TEXT), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

lv_obj_t* mk_key_icon(lv_obj_t* parent, const char* icon, lv_event_cb_t cb, void* ud) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, LV_PCT(100));
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* ic = mk_icon(b, icon, 20, theme::TEXT);
    lv_obj_center(ic);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

void build_temp_page(lv_obj_t* screen) {
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    // 半透明遮罩：点空白处关闭
    lv_obj_t* scrim = mk_obj(screen);
    lv_obj_set_size(scrim, 392, 304);
    lv_obj_align(scrim, rail_right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 rail_right ? -8 : 80, 0);
    lv_obj_set_style_bg_color(scrim, theme::c(0x000000), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_50, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scrim, close_temp_page_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    R.scrim = scrim;

    // 居中浮层（不是严格半屏）
    lv_obj_t* pg = mk_obj(screen);
    lv_obj_set_size(pg, 368, 204);
    lv_obj_align(pg, LV_ALIGN_CENTER, rail_right ? -36 : 36, 0);
    lv_obj_set_style_bg_color(pg, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(pg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pg, 20, 0);
    lv_obj_set_style_pad_all(pg, 12, 0);
    lv_obj_set_style_border_width(pg, 1, 0);
    lv_obj_set_style_border_color(pg, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(pg, 24, 0);
    lv_obj_set_style_shadow_opa(pg, LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(pg, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(pg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(pg, 6, 0);
    lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
    R.temp_page = pg;

    lv_obj_t* head = mk_obj(pg);
    lv_obj_set_size(head, LV_PCT(100), 26);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* hl = mk_obj(head);
    lv_obj_set_height(hl, LV_PCT(100));
    lv_obj_set_flex_flow(hl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hl, 8, 0);
    R.tp_title = mk_label(hl, "T1 温度", 18, true, theme::TEXT);
    mk_label(hl, "长按可编辑", 10, false, theme::MUTED);

    mk_close_button(head, close_temp_page_cb);

    lv_obj_t* body = mk_obj(pg);
    lv_obj_set_size(body, LV_PCT(100), 148);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(body, 12, 0);

    // 滑块（原生亮度条风格：圆角矩形轨道 + 填充 + 宽手柄）
    lv_obj_t* sl = mk_obj(body);
    lv_obj_set_size(sl, 56, 148);
    lv_obj_add_flag(sl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sl, slider_event_cb, LV_EVENT_ALL, nullptr);
    R.tp_slider = sl;

    lv_obj_t* track = mk_obj(sl);
    lv_obj_set_size(track, 36, LV_PCT(100));
    lv_obj_set_style_radius(track, 10, 0);
    lv_obj_set_style_bg_color(track, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_align(track, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);

    R.tp_fill = mk_obj(sl);
    lv_obj_set_size(R.tp_fill, 36, 0);
    lv_obj_set_style_radius(R.tp_fill, 10, 0);
    lv_obj_set_style_bg_color(R.tp_fill, theme::c(theme::PRIMARY), 0);
    lv_obj_set_style_bg_opa(R.tp_fill, LV_OPA_COVER, 0);
    lv_obj_align(R.tp_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_remove_flag(R.tp_fill, LV_OBJ_FLAG_CLICKABLE);

    R.tp_knob = mk_obj(sl);
    lv_obj_set_size(R.tp_knob, 56, 16);
    lv_obj_set_style_radius(R.tp_knob, 6, 0);
    lv_obj_set_style_bg_color(R.tp_knob, theme::c(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(R.tp_knob, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(R.tp_knob, 8, 0);
    lv_obj_set_style_shadow_opa(R.tp_knob, LV_OPA_30, 0);
    lv_obj_align(R.tp_knob, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_remove_flag(R.tp_knob, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* right = mk_obj(body);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 2, 0);

    R.tp_big = mk_label(right, "0°C", 28, true, theme::TEXT);
    lv_obj_add_flag(R.tp_big, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(R.tp_big, temp_big_cb, LV_EVENT_CLICKED, nullptr);
    R.tp_now = mk_label(right, "当前 --°C", 11, false, theme::MUTED);
    mk_label(right, "点数字可手动输入", 10, false, theme::MUTED);

    lv_obj_t* spacer = mk_obj(right);
    lv_obj_set_width(spacer, LV_PCT(100));
    lv_obj_set_flex_grow(spacer, 1);

    for (int row_i = 0; row_i < 2; row_i++) {
        lv_obj_t* row = mk_obj(right);
        lv_obj_set_size(row, LV_PCT(100), 30);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);
        for (int col = 0; col < 3; col++) {
            int i = row_i * 3 + col;
            char buf[16];
            snprintf(buf, sizeof(buf), "%d°", g_presets[i]);
            R.tp_chip[i] = mk_chip(row, buf, temp_chip_cb, (void*)(intptr_t)i, 0, 30);
            lv_obj_set_style_bg_color(R.tp_chip[i], theme::c(theme::SURFACE), 0);
            lv_obj_t* lb = lv_obj_get_child(R.tp_chip[i], 0);
            if (lb) lv_obj_set_style_text_color(lb, theme::c(theme::TEXT), 0);
            lv_obj_add_event_cb(R.tp_chip[i], temp_chip_long_cb, LV_EVENT_LONG_PRESSED,
                                (void*)(intptr_t)i);
        }
    }
}

void build_keypad(lv_obj_t* screen) {
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    // 键盘自己再压一层遮罩：层次 = 详情页 < 调温浮层 < 数字键盘
    lv_obj_t* scrim = mk_obj(screen);
    lv_obj_set_size(scrim, 392, 304);
    lv_obj_align(scrim, rail_right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 rail_right ? -8 : 80, 0);
    lv_obj_set_style_bg_color(scrim, theme::c(0x000000), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_40, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scrim, keypad_close_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    R.keypad_scrim = scrim;

    lv_obj_t* kp = mk_obj(screen);
    lv_obj_set_size(kp, 300, 260);
    lv_obj_align(kp, LV_ALIGN_BOTTOM_MID, rail_right ? -36 : 36, -8);
    lv_obj_set_style_bg_color(kp, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(kp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(kp, 20, 0);
    lv_obj_set_style_pad_all(kp, 12, 0);
    lv_obj_set_style_border_width(kp, 1, 0);
    lv_obj_set_style_border_color(kp, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(kp, 24, 0);
    lv_obj_set_style_shadow_opa(kp, LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(kp, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(kp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(kp, 6, 0);
    lv_obj_add_flag(kp, LV_OBJ_FLAG_HIDDEN);
    R.keypad = kp;

    lv_obj_t* head = mk_obj(kp);
    lv_obj_set_size(head, LV_PCT(100), 28);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    R.keypad_title = mk_label(head, "设置喷嘴温度", 18, true, theme::TEXT);
    mk_close_button(head, keypad_close_cb);

    lv_obj_t* valrow = mk_obj(kp);
    lv_obj_set_size(valrow, LV_PCT(100), 40);
    lv_obj_set_flex_flow(valrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(valrow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(valrow, 4, 0);
    R.keypad_value = mk_label(valrow, "0", 30, true, theme::TEXT);
    mk_label(valrow, "°C", 14, false, theme::MUTED);

    static const char* rows[4][3] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {nullptr, "0", "确定"},
    };
    for (int r = 0; r < 4; r++) {
        lv_obj_t* row = mk_obj(kp);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_flex_grow(row, 1);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 8, 0);
        for (int c = 0; c < 3; c++) {
            if (r == 3 && c == 0) {
                mk_key_icon(row, IC_BACKSPACE, keypad_back_cb, nullptr);
            } else if (r == 3 && c == 2) {
                mk_key(row, rows[r][c], keypad_ok_cb, nullptr, true);
            } else {
                int d = rows[r][c][0] - '0';
                mk_key(row, rows[r][c], keypad_digit_cb, (void*)(intptr_t)d, false);
            }
        }
    }
}

void update_temp_page() {
    if (!MR || !R.temp_page) return;
    if (lv_obj_has_flag(R.temp_page, LV_OBJ_FLAG_HIDDEN)) return;
    PrinterState s = MR->snapshot();
    double cur = 0, tgt = 0;
    if (g_temp_kind == 1) {
        cur = s.bed_temp;
        tgt = s.bed_target;
    } else if (g_sheet_tool >= 0 && g_sheet_tool <= 3) {
        cur = s.tools[g_sheet_tool].temp;
        tgt = s.tools[g_sheet_tool].target;
    } else {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "当前 %.0f°C", cur);
    if (R.tp_now) lv_label_set_text(R.tp_now, buf);
    // 别和用户拖动/刚下发的目标打架
    if (!g_slider_dragging && lv_tick_get() - g_slider_apply_at > 1500)
        slider_set_value((int)lround(tgt));
}

// 喷嘴参数弹层
void close_nozzle_cb(lv_event_t*) {
    if (R.nz_page) lv_obj_add_flag(R.nz_page, LV_OBJ_FLAG_HIDDEN);
    if (R.nz_scrim) lv_obj_add_flag(R.nz_scrim, LV_OBJ_FLAG_HIDDEN);
}

void update_nozzle_page();

void open_nozzle_cb(lv_event_t*) {
    if (!R.nz_page || g_sheet_tool < 0) return;
    char buf[24];
    snprintf(buf, sizeof(buf), "T%d 喷嘴", g_sheet_tool + 1);
    if (R.nz_title) lv_label_set_text(R.nz_title, buf);
    if (R.nz_scrim) {
        lv_obj_remove_flag(R.nz_scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.nz_scrim, -1);
    }
    lv_obj_remove_flag(R.nz_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.nz_page, -1);
    update_nozzle_page();
}

void build_nozzle_page(lv_obj_t* screen) {
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    lv_obj_t* scrim = mk_obj(screen);
    lv_obj_set_size(scrim, 392, 304);
    lv_obj_align(scrim, rail_right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 rail_right ? -8 : 80, 0);
    lv_obj_set_style_bg_color(scrim, theme::c(0x000000), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_40, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scrim, close_nozzle_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    R.nz_scrim = scrim;

    lv_obj_t* pg = mk_obj(screen);
    lv_obj_set_size(pg, 300, 170);
    lv_obj_align(pg, LV_ALIGN_CENTER, rail_right ? -36 : 36, 0);
    lv_obj_set_style_bg_color(pg, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(pg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pg, 20, 0);
    lv_obj_set_style_pad_all(pg, 12, 0);
    lv_obj_set_style_border_width(pg, 1, 0);
    lv_obj_set_style_border_color(pg, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(pg, 24, 0);
    lv_obj_set_style_shadow_opa(pg, LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(pg, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(pg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(pg, 6, 0);
    lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
    R.nz_page = pg;

    lv_obj_t* head = mk_obj(pg);
    lv_obj_set_size(head, LV_PCT(100), 26);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    R.nz_title = mk_label(head, "T1 喷嘴", 18, true, theme::TEXT);
    mk_close_button(head, close_nozzle_cb);

    // 直径
    mk_label(pg, "直径", 11, false, theme::MUTED);
    lv_obj_t* drow = mk_obj(pg);
    lv_obj_set_size(drow, LV_PCT(100), 34);
    lv_obj_set_flex_flow(drow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(drow, 8, 0);
    for (int i = 0; i < 4; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.1f", kNozzleDias[i]);
        R.nz_dia_chip[i] = mk_chip(drow, buf, nozzle_dia_cb, (void*)(intptr_t)i, 0, 34);
        lv_obj_set_style_bg_color(R.nz_dia_chip[i], theme::c(theme::SURFACE), 0);
        lv_obj_t* lb = lv_obj_get_child(R.nz_dia_chip[i], 0);
        if (lb) lv_obj_set_style_text_color(lb, theme::c(theme::TEXT), 0);
    }

    // 流量类型
    mk_label(pg, "流量", 11, false, theme::MUTED);
    lv_obj_t* frow = mk_obj(pg);
    lv_obj_set_size(frow, LV_PCT(100), 34);
    lv_obj_set_flex_flow(frow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(frow, 8, 0);
    R.nz_type_chip[0] = mk_chip(frow, "标准", nozzle_flow_cb, (void*)(intptr_t)0, 0, 34);
    R.nz_type_chip[1] = mk_chip(frow, "高流", nozzle_flow_cb, (void*)(intptr_t)1, 0, 34);
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(R.nz_type_chip[i], theme::c(theme::SURFACE), 0);
        lv_obj_t* lb = lv_obj_get_child(R.nz_type_chip[i], 0);
        if (lb) lv_obj_set_style_text_color(lb, theme::c(theme::TEXT), 0);
    }
}

void update_nozzle_page() {
    if (!MR || !R.nz_page) return;
    if (lv_obj_has_flag(R.nz_page, LV_OBJ_FLAG_HIDDEN)) return;
    if (g_sheet_tool < 0 || g_sheet_tool > 3) return;
    PrinterState s = MR->snapshot();
    double d = s.nozzle_diameter[g_sheet_tool];
    bool hf = s.nozzle_type[g_sheet_tool] == "high_flow";
    nozzle_view(d, hf);
    int active = -1;
    for (int i = 0; i < 4; i++)
        if (fabs(d - kNozzleDias[i]) < 0.01) active = i;
    for (int i = 0; i < 4; i++) style_chip(R.nz_dia_chip[i], i == active);
    style_chip(R.nz_type_chip[0], !hf);
    style_chip(R.nz_type_chip[1], hf);
}

void load_filament_cb(lv_event_t*) {
    if (!MR || g_sheet_tool < 0) return;
    MR->send_gcode("AUTO_FEEDING EXTRUDER=" + std::to_string(g_sheet_tool) + " LOAD=1");
    g_feed_dismissed = false;
    g_feed_local = true;
    g_feed_local_tool = g_sheet_tool;
    g_feed_local_kind = 1;
    g_feed_pref_tool = g_sheet_tool;
    g_feed_local_at = lv_tick_get();
    g_feed_was_active = false;
    g_feed_done_until = 0;
    feed_banner_set(false, false, IC_LOAD,
                    feed_title("load", false, false, g_sheet_tool).c_str(), "等待打印机…");
}

void unload_filament_cb(lv_event_t*) {
    if (!MR || g_sheet_tool < 0) return;
    std::string n = std::to_string(g_sheet_tool);
    MR->send_gcode("AUTO_FEEDING EXTRUDER=" + n + " UNLOAD=1 STAGE=prepare\n" +
                   "AUTO_FEEDING EXTRUDER=" + n + " UNLOAD=1 STAGE=doing");
    g_feed_dismissed = false;
    g_feed_local = true;
    g_feed_local_tool = g_sheet_tool;
    g_feed_local_kind = 2;
    g_feed_pref_tool = g_sheet_tool;
    g_feed_local_at = lv_tick_get();
    g_feed_was_active = false;
    g_feed_done_until = 0;
    feed_banner_set(false, false, IC_UNLOAD,
                    feed_title("unload", false, false, g_sheet_tool).c_str(), "等待打印机…");
}

void hide_sheets() {
    if (R.sheet) lv_obj_add_flag(R.sheet, LV_OBJ_FLAG_HIDDEN);
    if (R.picker) lv_obj_add_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.temp_page) lv_obj_add_flag(R.temp_page, LV_OBJ_FLAG_HIDDEN);
    if (R.scrim) lv_obj_add_flag(R.scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.keypad) lv_obj_add_flag(R.keypad, LV_OBJ_FLAG_HIDDEN);
    if (R.keypad_scrim) lv_obj_add_flag(R.keypad_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.nz_page) lv_obj_add_flag(R.nz_page, LV_OBJ_FLAG_HIDDEN);
    if (R.nz_scrim) lv_obj_add_flag(R.nz_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.color_page) lv_obj_add_flag(R.color_page, LV_OBJ_FLAG_HIDDEN);
    if (R.color_scrim) lv_obj_add_flag(R.color_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.net_page) lv_obj_add_flag(R.net_page, LV_OBJ_FLAG_HIDDEN);
    if (R.print_sheet) lv_obj_add_flag(R.print_sheet, LV_OBJ_FLAG_HIDDEN);
    if (R.net_pw_page) lv_obj_add_flag(R.net_pw_page, LV_OBJ_FLAG_HIDDEN);
    g_net_open = false;
    g_net_pw_open = false;
    g_edit_slot = -1;
}

void close_sheet_cb(lv_event_t*) { hide_sheets(); }

void update_sheet();

void open_tool_sheet(int t) {
    if (t < 0 || t > 3) return;
    g_nz_dia = -1;
    g_nz_flow = -1;
    g_nz_at = 0;
    g_feed_at = 0;
    g_fil_pending = false;
    g_sheet_tool = t;
    char buf[32];
    snprintf(buf, sizeof(buf), "T%d 喷嘴", t + 1);
    if (R.sheet_title) lv_label_set_text(R.sheet_title, buf);
    if (R.picker) lv_obj_add_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.temp_page) lv_obj_add_flag(R.temp_page, LV_OBJ_FLAG_HIDDEN);
    if (R.keypad) lv_obj_add_flag(R.keypad, LV_OBJ_FLAG_HIDDEN);
    if (R.keypad_scrim) lv_obj_add_flag(R.keypad_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.nz_page) lv_obj_add_flag(R.nz_page, LV_OBJ_FLAG_HIDDEN);
    if (R.nz_scrim) lv_obj_add_flag(R.nz_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.color_page) lv_obj_add_flag(R.color_page, LV_OBJ_FLAG_HIDDEN);
    if (R.color_scrim) lv_obj_add_flag(R.color_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.sheet) lv_obj_remove_flag(R.sheet, LV_OBJ_FLAG_HIDDEN);
    update_sheet();
    if (MR) feed_tick(MR->snapshot());
}

void nozzle_card_cb(lv_event_t* e) {
    open_tool_sheet((int)(intptr_t)lv_event_get_user_data(e));
}

void sheet_to_picker_cb(lv_event_t*);

void rebuild_picker();
void picker_back_cb(lv_event_t*);

void send_filament_config(const MaterialOption& m, uint32_t color) {
    if (!MR || g_sheet_tool < 0) return;
    std::string sub = sub_text(m.sub);
    if (sub.empty()) sub = "generic";
    char rgba[16];
    snprintf(rgba, sizeof(rgba), "%06XFF", (unsigned)(color & 0xFFFFFF));
    std::string cmd = "SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=" +
                      std::to_string(g_sheet_tool) +
                      " VENDOR=" + gcode_arg(m.vendor.empty() ? "generic" : m.vendor) +
                      " FILAMENT_TYPE=" + gcode_arg(m.type) +
                      " FILAMENT_SUBTYPE=" + gcode_arg(sub) +
                      " FILAMENT_COLOR_RGBA=" + rgba;
    // 先乐观更新，再异步下发；选完耗材界面立刻变化
    g_fil_pending = true;
    g_fil_mat = m.type;
    g_fil_sub = sub_text(m.sub);
    g_fil_vendor = vendor_text(m.vendor);
    g_fil_argb = color & 0xFFFFFF;
    g_fil_at = lv_tick_get();
    MR->send_gcode(cmd);
}

void material_pick_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= (int)g_mat_view.size()) return;
    MaterialOption m = g_mat_view[i];
    m.vendor = g_pick_brand;  // 品牌以左侧选择为准
    send_filament_config(m, g_pick_color);
    if (R.picker) lv_obj_add_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.sheet) {
        lv_obj_remove_flag(R.sheet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.sheet, -1);
    }
    update_sheet();
}

void brand_pick_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= (int)g_brands.size()) return;
    g_pick_brand = g_brands[i];
    rebuild_picker();
}

// 颜色
void refresh_color_swatches() {
    for (int i = 0; i < 28; i++) {
        if (!R.color_sw[i]) continue;
        bool on = (kPalette[i] & 0xFFFFFF) == (g_pick_color & 0xFFFFFF);
        lv_obj_set_style_border_width(R.color_sw[i], on ? 3 : 1, 0);
        lv_obj_set_style_border_color(R.color_sw[i],
                                      theme::c(on ? theme::PRIMARY : theme::OUTLINE), 0);
    }
}

void refresh_picker_color() {
    if (!R.picker_color) return;
    lv_obj_set_style_bg_color(R.picker_color, theme::c(g_pick_color), 0);
    lv_obj_set_style_bg_opa(R.picker_color, LV_OPA_COVER, 0);
}

void close_color_cb(lv_event_t*) {
    if (R.color_page) lv_obj_add_flag(R.color_page, LV_OBJ_FLAG_HIDDEN);
    if (R.color_scrim) lv_obj_add_flag(R.color_scrim, LV_OBJ_FLAG_HIDDEN);
    if (R.picker) {
        lv_obj_remove_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.picker, -1);
    }
}

void color_pick_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= 28) return;
    g_pick_color = kPalette[i];
    refresh_picker_color();
    close_color_cb(nullptr);
}

void open_color_page() {
    if (!R.color_page) return;
    refresh_color_swatches();
    if (R.picker) lv_obj_add_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.color_scrim) {
        lv_obj_remove_flag(R.color_scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(R.color_scrim, -1);
    }
    lv_obj_remove_flag(R.color_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.color_page, -1);
}

void color_card_cb(lv_event_t*) { open_color_page(); }

void build_color_page(lv_obj_t* screen) {
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    lv_obj_t* scrim = mk_obj(screen);
    lv_obj_set_size(scrim, 392, 304);
    lv_obj_align(scrim, rail_right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 rail_right ? -8 : 80, 0);
    lv_obj_set_style_bg_color(scrim, theme::c(0x000000), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_40, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scrim, close_color_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    R.color_scrim = scrim;

    lv_obj_t* pg = mk_obj(screen);
    lv_obj_set_size(pg, 330, 210);
    lv_obj_align(pg, LV_ALIGN_CENTER, rail_right ? -36 : 36, 0);
    lv_obj_set_style_bg_color(pg, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(pg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pg, 20, 0);
    lv_obj_set_style_pad_all(pg, 12, 0);
    lv_obj_set_style_border_width(pg, 1, 0);
    lv_obj_set_style_border_color(pg, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(pg, 24, 0);
    lv_obj_set_style_shadow_opa(pg, LV_OPA_50, 0);
    lv_obj_set_style_shadow_color(pg, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(pg, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(pg, 6, 0);
    lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
    R.color_page = pg;

    lv_obj_t* head = mk_obj(pg);
    lv_obj_set_size(head, LV_PCT(100), 26);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    mk_label(head, "选择颜色", 18, true, theme::TEXT);
    mk_close_button(head, close_color_cb);

    lv_obj_t* grid = mk_obj(pg);
    lv_obj_set_size(grid, LV_PCT(100), 150);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);
    for (int i = 0; i < 28; i++) {
        lv_obj_t* sw = mk_obj(grid);
        lv_obj_set_size(sw, 34, 34);
        lv_obj_set_style_radius(sw, 9, 0);
        lv_obj_set_style_bg_color(sw, theme::c(kPalette[i]), 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        lv_obj_set_style_border_color(sw, theme::c(theme::OUTLINE), 0);
        lv_obj_add_event_cb(sw, color_pick_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        R.color_sw[i] = sw;
    }
}

// 非官方耗材：点整张耗材卡直接进材料选择
void filament_card_cb(lv_event_t*) {
    if (!MR || g_sheet_tool < 0) return;
    PrinterState s = MR->snapshot();
    if (s.filaments[g_sheet_tool].official) return;
    sheet_to_picker_cb(nullptr);
}

lv_obj_t* mk_overlay(lv_obj_t* screen) {
    lv_obj_t* o = mk_obj(screen);
    lv_obj_set_size(o, 392, 304);
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";
    lv_obj_align(o, LV_ALIGN_LEFT_MID, rail_right ? 8 : 80, 0);
    lv_obj_set_style_bg_color(o, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 18, 0);
    lv_obj_set_style_pad_all(o, 12, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(o, 24, 0);
    lv_obj_set_style_shadow_opa(o, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(o, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, 6, 0);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

lv_obj_t* mk_close_button(lv_obj_t* parent, lv_event_cb_t cb) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 30, 30);
    lv_obj_set_style_radius(b, 999, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_t* ic = mk_icon(b, IC_CLOSE, 18, theme::TEXT);
    lv_obj_center(ic);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

void build_tool_sheet(lv_obj_t* screen) {
    lv_obj_t* sh = mk_overlay(screen);
    R.sheet = sh;

    lv_obj_t* head = mk_obj(sh);
    lv_obj_set_size(head, LV_PCT(100), 30);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* hl = mk_obj(head);
    lv_obj_set_height(hl, LV_PCT(100));
    lv_obj_set_flex_flow(hl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hl, 8, 0);
    R.sheet_title = mk_label(hl, "T1 喷嘴", 20, true, theme::TEXT);

    lv_obj_t* hr = mk_obj(head);
    lv_obj_set_height(hr, LV_PCT(100));
    lv_obj_set_flex_flow(hr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hr, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hr, 8, 0);

    R.sheet_nfc = mk_obj(hr);
    lv_obj_set_size(R.sheet_nfc, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_bg_color(R.sheet_nfc, theme::c(theme::PRIMARY_DIM), 0);
    lv_obj_set_style_bg_opa(R.sheet_nfc, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(R.sheet_nfc, 999, 0);
    lv_obj_set_style_pad_hor(R.sheet_nfc, 8, 0);
    lv_obj_set_flex_flow(R.sheet_nfc, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(R.sheet_nfc, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(R.sheet_nfc, 4, 0);
    mk_icon(R.sheet_nfc, IC_NFC, 13, theme::PRIMARY);
    mk_label(R.sheet_nfc, "NFC", 11, true, theme::PRIMARY);
    lv_obj_add_flag(R.sheet_nfc, LV_OBJ_FLAG_HIDDEN);
    mk_close_button(hr, close_sheet_cb);

    // 主体：左耗材 / 右温度
    lv_obj_t* body = mk_obj(sh);
    lv_obj_set_size(body, LV_PCT(100), 140);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(body, 8, 0);

    lv_obj_t* fc = mk_card(body, theme::SURFACE);
    lv_obj_set_height(fc, LV_PCT(100));
    lv_obj_set_flex_grow(fc, 1);
    lv_obj_set_style_pad_all(fc, 10, 0);
    lv_obj_set_flex_flow(fc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(fc, 2, 0);
    lv_obj_add_flag(fc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fc, filament_card_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* fr = mk_obj(fc);
    lv_obj_set_size(fr, LV_PCT(100), 30);
    lv_obj_set_flex_flow(fr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fr, 9, 0);
    lv_obj_remove_flag(fr, LV_OBJ_FLAG_CLICKABLE);
    R.sheet_swatch = mk_obj(fr);
    lv_obj_set_size(R.sheet_swatch, 28, 28);
    lv_obj_set_style_radius(R.sheet_swatch, 8, 0);
    lv_obj_remove_flag(R.sheet_swatch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(R.sheet_swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(R.sheet_swatch, 1, 0);
    lv_obj_set_style_border_color(R.sheet_swatch, theme::c(theme::OUTLINE), 0);
    R.sheet_mat = mk_label(fr, "未识别", 15, true, theme::TEXT);
    lv_obj_set_flex_grow(R.sheet_mat, 1);
    lv_label_set_long_mode(R.sheet_mat, LV_LABEL_LONG_MODE_DOTS);

    R.sheet_vendor = mk_label(fc, "—", 11, false, theme::MUTED);
    lv_obj_set_width(R.sheet_vendor, LV_PCT(100));
    lv_label_set_long_mode(R.sheet_vendor, LV_LABEL_LONG_MODE_DOTS);
    R.sheet_spec = mk_label(fc, "—", 11, false, theme::MUTED);
    lv_obj_set_width(R.sheet_spec, LV_PCT(100));
    lv_label_set_long_mode(R.sheet_spec, LV_LABEL_LONG_MODE_DOTS);
    R.sheet_spec2 = mk_label(fc, "—", 11, false, theme::MUTED);
    lv_obj_set_width(R.sheet_spec2, LV_PCT(100));
    lv_label_set_long_mode(R.sheet_spec2, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* spacer = mk_obj(fc);
    lv_obj_set_width(spacer, LV_PCT(100));
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_remove_flag(spacer, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* mrow = mk_obj(fc);
    lv_obj_set_size(mrow, LV_PCT(100), 26);
    lv_obj_set_flex_flow(mrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(mrow, LV_OBJ_FLAG_CLICKABLE);
    R.sheet_lock = mk_label(mrow, "官方 RFID 耗材，不可修改", 10, false, theme::MUTED);

    // 右：温度卡
    lv_obj_t* tc = mk_card(body, theme::SURFACE);
    lv_obj_set_size(tc, 160, LV_PCT(100));
    lv_obj_set_style_pad_all(tc, 10, 0);
    lv_obj_set_flex_flow(tc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tc, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_add_flag(tc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tc, open_temp_cb, LV_EVENT_CLICKED, nullptr);
    R.sheet_temp_card = tc;

    lv_obj_t* tl = mk_obj(tc);
    lv_obj_set_height(tl, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tl, 0, 0);
    lv_obj_remove_flag(tl, LV_OBJ_FLAG_CLICKABLE);
    mk_label(tl, "喷嘴温度", 11, false, theme::MUTED);
    R.sheet_temp_now = mk_label(tl, "--°C", 34, true, theme::TEXT);
    R.sheet_temp_target = mk_label(tl, "目标 0°C", 12, false, theme::MUTED);

    lv_obj_t* go = mk_obj(tc);
    lv_obj_set_size(go, LV_PCT(100), 34);
    lv_obj_remove_flag(go, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(go, theme::c(theme::PRIMARY_DIM), 0);
    lv_obj_set_style_bg_opa(go, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(go, 10, 0);
    lv_obj_set_flex_flow(go, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(go, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(go, 4, 0);
    mk_label(go, "调节", 12, true, theme::PRIMARY);
    mk_icon(go, IC_CHEVRON_RIGHT, 14, theme::PRIMARY);

    // 底部并排：左自动进料 / 右喷嘴参数
    lv_obj_t* brow = mk_obj(sh);
    lv_obj_set_size(brow, LV_PCT(100), 46);
    lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(brow, 8, 0);

    lv_obj_t* feed = mk_card(brow, theme::SURFACE);
    lv_obj_set_height(feed, LV_PCT(100));
    lv_obj_set_flex_grow(feed, 1);
    lv_obj_set_style_pad_ver(feed, 6, 0);
    lv_obj_set_flex_flow(feed, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(feed, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(feed, 6, 0);
    lv_obj_add_flag(feed, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(feed, feed_card_cb, LV_EVENT_CLICKED, nullptr);

    mk_label(feed, "自动预进料", 12, false, theme::TEXT);
    R.sheet_feed_hint = mk_label(feed, "—", 10, false, theme::MUTED);
    lv_obj_set_flex_grow(R.sheet_feed_hint, 1);
    R.sheet_feed_sw = mk_switch(feed, false, sheet_feed_cb, nullptr);

    lv_obj_t* nz = mk_card(brow, theme::SURFACE);
    lv_obj_set_size(nz, 172, LV_PCT(100));
    lv_obj_set_style_pad_ver(nz, 6, 0);
    lv_obj_set_flex_flow(nz, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nz, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(nz, 6, 0);
    lv_obj_add_flag(nz, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(nz, open_nozzle_cb, LV_EVENT_CLICKED, nullptr);
    mk_label(nz, "喷嘴", 12, false, theme::MUTED);
    R.sheet_nozzle_label = mk_label(nz, "Ø0.4 · 标准", 12, true, theme::TEXT);
    lv_obj_set_flex_grow(R.sheet_nozzle_label, 1);
    lv_label_set_long_mode(R.sheet_nozzle_label, LV_LABEL_LONG_MODE_DOTS);
    mk_icon(nz, IC_CHEVRON_RIGHT, 13, theme::MUTED);

    lv_obj_t* btns = mk_obj(sh);
    lv_obj_set_size(btns, LV_PCT(100), 46);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btns, 8, 0);
    mk_button(btns, "载入", true, load_filament_cb, nullptr, 46);
    lv_obj_t* b_unload = mk_button(btns, "退料", false, unload_filament_cb, nullptr, 46);
    lv_obj_t* b_done = mk_button(btns, "完成", false, close_sheet_cb, nullptr, 46);
    for (lv_obj_t* b : {b_unload, b_done})
        lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
}

void build_picker(lv_obj_t* screen) {
    lv_obj_t* pk = mk_overlay(screen);
    R.picker = pk;

    lv_obj_t* head = mk_obj(pk);
    lv_obj_set_size(head, LV_PCT(100), 30);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    R.picker_title = mk_label(head, "更换耗材", 18, true, theme::TEXT);
    mk_close_button(head, picker_back_cb);

    lv_obj_t* body = mk_obj(pk);
    lv_obj_set_size(body, LV_PCT(100), 244);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(body, 8, 0);

    // 左 1/4：品牌 + 颜色
    lv_obj_t* left = mk_obj(body);
    lv_obj_set_size(left, 92, LV_PCT(100));
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 4, 0);
    mk_label(left, "品牌", 11, false, theme::MUTED);

    R.picker_brand_list = mk_obj(left);
    lv_obj_set_width(R.picker_brand_list, LV_PCT(100));
    lv_obj_set_flex_grow(R.picker_brand_list, 1);
    lv_obj_set_flex_flow(R.picker_brand_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(R.picker_brand_list, 4, 0);
    lv_obj_add_flag(R.picker_brand_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(R.picker_brand_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(R.picker_brand_list, LV_SCROLLBAR_MODE_OFF);

    mk_label(left, "颜色", 11, false, theme::MUTED);
    R.picker_color = mk_obj(left);
    lv_obj_set_size(R.picker_color, LV_PCT(100), 42);
    lv_obj_set_style_radius(R.picker_color, 10, 0);
    lv_obj_set_style_bg_color(R.picker_color, theme::c(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(R.picker_color, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(R.picker_color, 1, 0);
    lv_obj_set_style_border_color(R.picker_color, theme::c(theme::OUTLINE), 0);
    lv_obj_add_event_cb(R.picker_color, color_card_cb, LV_EVENT_CLICKED, nullptr);

    // 右：具体耗材
    lv_obj_t* right = mk_obj(body);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 4, 0);

    lv_obj_t* rlabel = mk_row(right, 14);
    mk_label(rlabel, "耗材", 11, false, theme::MUTED);
    R.picker_count = mk_label(rlabel, "", 11, false, theme::MUTED);

    R.picker_list = mk_obj(right);
    lv_obj_set_width(R.picker_list, LV_PCT(100));
    lv_obj_set_flex_grow(R.picker_list, 1);
    lv_obj_set_flex_flow(R.picker_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(R.picker_list, 4, 0);
    lv_obj_set_style_pad_right(R.picker_list, 8, 0);
    lv_obj_add_flag(R.picker_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(R.picker_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(R.picker_list, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_style_width(R.picker_list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(R.picker_list, theme::c(theme::MUTED), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(R.picker_list, LV_OPA_40, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(R.picker_list, 999, LV_PART_SCROLLBAR);
}

void picker_back_cb(lv_event_t*) {
    if (R.picker) lv_obj_add_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.sheet) lv_obj_remove_flag(R.sheet, LV_OBJ_FLAG_HIDDEN);
}

void rebuild_picker() {
    if (!R.picker_list || !R.picker_brand_list) return;
    g_mat_opts = MR ? MR->snapshot().materials : std::vector<MaterialOption>();

    // 品牌列
    lv_obj_clean(R.picker_brand_list);
    for (size_t i = 0; i < g_brands.size(); i++) {
        lv_obj_t* c = mk_chip(R.picker_brand_list, vendor_text(g_brands[i]).c_str(),
                              brand_pick_cb, (void*)(intptr_t)i, 80, 26);
        lv_obj_set_style_bg_color(c, theme::c(theme::SURFACE), 0);
        lv_obj_t* lb = lv_obj_get_child(c, 0);
        if (lb) {
            lv_obj_set_style_text_color(lb, theme::c(theme::TEXT), 0);
            lv_obj_set_width(lb, LV_PCT(100));
            lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(lb, LV_LABEL_LONG_MODE_DOTS);
        }
        style_chip(c, g_brands[i] == g_pick_brand);
    }

    // 右侧：当前品牌下的型号；自定义品牌回退到通用参数表
    g_mat_view.clear();
    for (const auto& m : g_mat_opts)
        if (m.vendor == g_pick_brand) g_mat_view.push_back(m);
    if (g_mat_view.empty())
        for (const auto& m : g_mat_opts)
            if (m.vendor == "generic") g_mat_view.push_back(m);

    // 自定义类型置顶（Kexcelled ABS Basic 之类）
    std::vector<MaterialOption> extra;
    for (const auto& c : g_custom_mats) {
        bool dup = false;
        for (const auto& m : g_mat_view)
            if (m.type == c.type && sub_text(m.sub) == sub_text(c.sub)) dup = true;
        if (!dup) extra.push_back(c);
    }
    if (!extra.empty())
        g_mat_view.insert(g_mat_view.begin(), extra.begin(), extra.end());

    lv_obj_clean(R.picker_list);
    if (g_mat_view.empty()) {
        lv_obj_t* hint = mk_label(
            R.picker_list,
            "没有读到材料库\n(/server/files/config/snapmaker/filament_parameters.json)", 12,
            false, theme::MUTED);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    } else {
        for (size_t i = 0; i < g_mat_view.size(); i++) {
            const MaterialOption& m = g_mat_view[i];
            lv_obj_t* row = lv_button_create(R.picker_list);
            lv_obj_set_size(row, LV_PCT(100), 36);
            lv_obj_set_style_radius(row, 8, 0);
            lv_obj_set_style_bg_color(row, theme::c(theme::SURFACE), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_shadow_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 8, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            std::string name = m.type;
            std::string sub = sub_text(m.sub);
            if (!sub.empty()) name += " · " + sub;
            lv_obj_t* l = mk_label(row, name.c_str(), 13, true, theme::TEXT);
            lv_obj_set_flex_grow(l, 1);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
            if (m.custom) mk_label(row, "自定义", 10, false, theme::PRIMARY);
            lv_obj_add_event_cb(row, material_pick_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }
    if (R.picker_count) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d 种", (int)g_mat_view.size());
        lv_label_set_text(R.picker_count, buf);
    }
    refresh_picker_color();
}

void sheet_to_picker_cb(lv_event_t*) {
    if (!R.picker) return;
    load_brands();
    load_custom_materials();
    if (MR && g_sheet_tool >= 0) {
        PrinterState s = MR->snapshot();
        const FilamentInfo& f = s.filaments[g_sheet_tool];
        g_pick_brand = (f.present && !f.vendor.empty()) ? f.vendor : "generic";
        g_pick_color = f.present ? (f.argb & 0xFFFFFF) : 0xFFFFFF;
    } else {
        g_pick_brand = "generic";
        g_pick_color = 0xFFFFFF;
    }
    bool found = false;
    for (const auto& b : g_brands)
        if (b == g_pick_brand) found = true;
    if (!found) g_brands.push_back(g_pick_brand);
    rebuild_picker();
    if (R.sheet) lv_obj_add_flag(R.sheet, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(R.picker, LV_OBJ_FLAG_HIDDEN);
    if (R.picker_title) lv_label_set_text(R.picker_title, "更换耗材");
}

void update_sheet() {
    if (!MR || !R.sheet) return;
    if (lv_obj_has_flag(R.sheet, LV_OBJ_FLAG_HIDDEN)) return;
    if (g_sheet_tool < 0 || g_sheet_tool > 3) return;
    PrinterState s = MR->snapshot();
    FilamentInfo f = s.filaments[g_sheet_tool];
    if (g_fil_pending) {
        const FilamentInfo& real = s.filaments[g_sheet_tool];
        bool ok = real.present && real.material == g_fil_mat && real.sub == g_fil_sub;
        if (ok) {
            g_fil_pending = false;
        } else if (lv_tick_get() - g_fil_at > 8000) {
            g_fil_pending = false;
            notice_error("耗材修改未生效", "已恢复原来的耗材");
        }
    }
    if (g_fil_pending) {
        f.present = true;
        f.material = g_fil_mat;
        f.sub = g_fil_sub;
        f.vendor = g_fil_vendor;
        f.argb = g_fil_argb;
        f.official = false;
        f.nfc = false;
        f.card_type.clear();
        f.hotend_min = f.hotend_max = f.bed_temp = 0;
        f.diameter = 0;
    }
    char buf[128];

    snprintf(buf, sizeof(buf), "%.0f°C", s.tools[g_sheet_tool].temp);
    if (R.sheet_temp_now) {
        lv_label_set_text(R.sheet_temp_now, buf);
        uint32_t col = theme::TEXT;
        if (s.tools[g_sheet_tool].target > 0.5 &&
            s.tools[g_sheet_tool].target > s.tools[g_sheet_tool].temp + 2)
            col = theme::WARN;
        else if (s.tools[g_sheet_tool].target > 0.5)
            col = theme::SUCCESS;
        lv_obj_set_style_text_color(R.sheet_temp_now, theme::c(col), 0);
    }
    if (s.tools[g_sheet_tool].target > 0.5)
        snprintf(buf, sizeof(buf), "目标 %.0f°C", s.tools[g_sheet_tool].target);
    else
        snprintf(buf, sizeof(buf), "未加热");
    if (R.sheet_temp_target) lv_label_set_text(R.sheet_temp_target, buf);

    set_swatch(R.sheet_swatch, f);
    std::string name = f.present ? f.material : "未识别";
    std::string sub = sub_text(f.sub);
    if (f.present && !sub.empty()) name += " · " + sub;
    if (R.sheet_mat) lv_label_set_text(R.sheet_mat, name.c_str());

    if (R.sheet_vendor) {
        std::string v = f.present && !f.vendor.empty() ? f.vendor : "—";
        v += " · ";
        v += (s.nozzle_type[g_sheet_tool] == "high_flow" ? "高流" : "标准");
        lv_label_set_text(R.sheet_vendor, v.c_str());
    }

    // 第一行：识别方式 / 卡型 / 线径
    std::string l1;
    if (f.nfc) {
        l1 = "NFC " + f.card_type;
    } else if (f.present) {
        l1 = f.official ? "官方耗材" : "手动设置";
    } else {
        l1 = "未检测到耗材";
    }
    if (f.diameter > 0) {
        snprintf(buf, sizeof(buf), "%s · Ø%.2f", l1.c_str(), f.diameter);
        l1 = buf;
    }
    if (R.sheet_spec) lv_label_set_text(R.sheet_spec, l1.c_str());

    // 第二行：NFC 建议温度；没有卡片时说明来源
    std::string l2;
    if (f.hotend_max > 0) {
        snprintf(buf, sizeof(buf), "喷头 %d-%d°C", f.hotend_min, f.hotend_max);
        l2 = buf;
        if (f.bed_temp > 0)
            l2 += " · 热床 " + std::to_string(f.bed_temp) + "°C";
    } else if (f.present) {
        l2 = "无 RFID 数据";
    }
    if (R.sheet_spec2) lv_label_set_text(R.sheet_spec2, l2.c_str());

    if (R.sheet_nfc) {
        if (f.nfc) lv_obj_remove_flag(R.sheet_nfc, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.sheet_nfc, LV_OBJ_FLAG_HIDDEN);
    }
    // 官方 RFID 耗材不允许直接改配置（固件会拒绝），点卡片时的判断在回调里
    if (R.sheet_lock) {
        if (f.official && f.present) lv_obj_remove_flag(R.sheet_lock, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.sheet_lock, LV_OBJ_FLAG_HIDDEN);
    }
    bool auto_mode = s.feed[g_sheet_tool].auto_mode;
    if (g_feed_at != 0) {
        if (auto_mode == g_feed_pending) {
            g_feed_at = 0;  // 已经同步
        } else if (lv_tick_get() - g_feed_at > 8000) {
            g_feed_at = 0;  // 超时，恢复真实值
            notice_error("预进料开关未生效", "已恢复");
        } else {
            auto_mode = g_feed_pending;  // 消抖：先保持本地状态
        }
    }
    set_sw(R.sheet_feed_sw, auto_mode);
    if (R.sheet_feed_hint) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s%d", g_sheet_tool < 2 ? "左" : "右",
                 g_sheet_tool % 2 + 1);
        lv_label_set_text(R.sheet_feed_hint, buf);
    }
    double nd = s.nozzle_diameter[g_sheet_tool];
    bool hf = s.nozzle_type[g_sheet_tool] == "high_flow";
    nozzle_view(nd, hf);
    if (nd <= 0) nd = 0.4;
    if (R.sheet_nozzle_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Ø%.1f · %s", nd, hf ? "高流" : "标准");
        lv_label_set_text(R.sheet_nozzle_label, buf);
    }
}

// ---------------------------------------------------------------- pages

void build_home(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);

    lv_obj_t* head = mk_obj(p);
    lv_obj_set_size(head, LV_PCT(100), 26);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    mk_label(head, "Snapmaker U1", 20, true, theme::TEXT);

    lv_obj_t* chip = mk_obj(head);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_bg_color(chip, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chip, 999, 0);
    lv_obj_set_style_pad_hor(chip, 8, 0);
    R.conn_state = mk_label(chip, "连接中", 13, false, theme::MUTED);
    lv_obj_center(R.conn_state);

    // 大状态卡：点开打印控制浮层
    R.status_card = mk_card(p, theme::SURFACE_HI);
    lv_obj_set_size(R.status_card, LV_PCT(100), 190);
    lv_obj_set_flex_flow(R.status_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(R.status_card, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(R.status_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(R.status_card, open_print_sheet_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* left = mk_obj(R.status_card);
    lv_obj_remove_flag(left, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(left, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_pad_right(left, 8, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left, 2, 0);
    R.status_title = mk_label(left, "待机", 26, true, theme::TEXT);
    R.status_sub = mk_label(left, "准备就绪", 14, false, theme::MUTED);
    lv_obj_set_width(R.status_sub, LV_PCT(100));
    lv_label_set_long_mode(R.status_sub, LV_LABEL_LONG_MODE_DOTS);
    R.status_sub2 = mk_label(left, "", 13, false, theme::MUTED);
    lv_obj_set_width(R.status_sub2, LV_PCT(100));
    lv_label_set_long_mode(R.status_sub2, LV_LABEL_LONG_MODE_DOTS);
    mk_label(left, "速度 / Z-offset 在「打印控制」", 10, false, theme::MUTED);

    R.status_arc = lv_arc_create(R.status_card);
    lv_obj_set_size(R.status_arc, 104, 104);
    lv_arc_set_rotation(R.status_arc, 135);
    lv_arc_set_bg_angles(R.status_arc, 0, 270);
    lv_arc_set_range(R.status_arc, 0, 1000);
    lv_obj_set_style_arc_width(R.status_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(R.status_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(R.status_arc, theme::c(theme::OUTLINE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(R.status_arc, theme::c(theme::PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(R.status_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(R.status_arc, LV_OBJ_FLAG_CLICKABLE);
    R.status_pct = mk_label(R.status_arc, "0%", 20, true, theme::TEXT);
    lv_obj_center(R.status_pct);

    lv_obj_t* quick = mk_obj(p);
    lv_obj_set_size(quick, LV_PCT(100), 46);
    lv_obj_set_flex_flow(quick, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(quick, 8, 0);
    R.home_pause_btn = mk_button(quick, "暂停", false, action_cb, (void*)"PAUSE", 46);
    R.home_resume_btn = mk_button(quick, "继续", false, action_cb, (void*)"RESUME", 46);
    R.home_stop_btn = mk_button(quick, "停止", true, action_cb, (void*)"CANCEL_PRINT", 46);
    R.home_print_btn = mk_button(quick, "打印控制", false, open_print_sheet_cb, nullptr, 46);
}

// 点首页状态卡打开的打印控制浮层
void open_print_sheet_cb(lv_event_t*) {
    if (!R.print_sheet) return;
    lv_obj_remove_flag(R.print_sheet, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.print_sheet, -1);
}

void build_print_sheet(lv_obj_t* screen) {
    lv_obj_t* sh = mk_overlay(screen);
    R.print_sheet = sh;

    lv_obj_t* head = mk_obj(sh);
    lv_obj_set_size(head, LV_PCT(100), 30);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    mk_label(head, "打印控制", 20, true, theme::TEXT);
    mk_close_button(head, [](lv_event_t*) {
        if (R.print_sheet) lv_obj_add_flag(R.print_sheet, LV_OBJ_FLAG_HIDDEN);
    });

    lv_obj_t* body = mk_obj(sh);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);

    lv_obj_t* card = mk_card(body);
    lv_obj_set_size(card, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    R.print_arc = lv_arc_create(card);
    lv_obj_set_size(R.print_arc, 104, 104);
    lv_arc_set_rotation(R.print_arc, 135);
    lv_arc_set_bg_angles(R.print_arc, 0, 270);
    lv_arc_set_range(R.print_arc, 0, 1000);
    lv_obj_set_style_arc_width(R.print_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(R.print_arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(R.print_arc, theme::c(theme::OUTLINE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(R.print_arc, theme::c(theme::PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(R.print_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(R.print_arc, LV_OBJ_FLAG_CLICKABLE);
    R.print_pct = mk_label(R.print_arc, "0%", 24, true, theme::TEXT);
    lv_obj_center(R.print_pct);

    lv_obj_t* right = mk_obj(card);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 6, 0);
    mk_label(right, "正在打印", 13, false, theme::MUTED);
    R.print_file = mk_label(right, "—", 16, true, theme::TEXT);
    lv_label_set_long_mode(R.print_file, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(R.print_file, LV_PCT(100));
    R.print_meta = mk_label(right, "—", 13, false, theme::MUTED);

    lv_obj_t* tune1 = mk_obj(right);
    lv_obj_set_size(tune1, LV_PCT(100), 36);
    lv_obj_set_flex_flow(tune1, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tune1, 6, 0);
    lv_obj_set_flex_align(tune1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    mk_mini_button(tune1, "-", speed_step_cb, (void*)(intptr_t)-1);
    R.speed_label = mk_label(tune1, "100%", 14, true, theme::TEXT);
    lv_obj_set_width(R.speed_label, 52);
    lv_obj_set_style_text_align(R.speed_label, LV_TEXT_ALIGN_CENTER, 0);
    mk_mini_button(tune1, "+", speed_step_cb, (void*)(intptr_t)1);

    lv_obj_t* tune2 = mk_obj(right);
    lv_obj_set_size(tune2, LV_PCT(100), 36);
    lv_obj_set_flex_flow(tune2, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tune2, 6, 0);
    lv_obj_set_flex_align(tune2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    mk_mini_button(tune2, "Z-", z_step_cb, (void*)(intptr_t)-1);
    R.zoff_label = mk_label(tune2, "0.00", 14, true, theme::TEXT);
    lv_obj_set_width(R.zoff_label, 56);
    lv_obj_set_style_text_align(R.zoff_label, LV_TEXT_ALIGN_CENTER, 0);
    mk_mini_button(tune2, "Z+", z_step_cb, (void*)(intptr_t)1);

    R.print_temps = mk_label(right, "—", 12, false, theme::MUTED);

    lv_obj_t* btns = mk_obj(right);
    lv_obj_set_size(btns, LV_PCT(100), 40);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btns, 8, 0);
    mk_button(btns, "暂停", false, action_cb, (void*)"PAUSE", 40);
    mk_button(btns, "继续", false, action_cb, (void*)"RESUME", 40);
    mk_button(btns, "取消", true, action_cb, (void*)"CANCEL_PRINT", 40);
}

// ---------------------------------------------------------------- 控制页

void ctrl_tab_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i > 2) return;
    g_ctrl_tab = i;
    for (int j = 0; j < 3; j++) {
        if (!R.ctrl_tab[j]) continue;
        if (j == i) lv_obj_remove_flag(R.ctrl_tab[j], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.ctrl_tab[j], LV_OBJ_FLAG_HIDDEN);
        style_chip(R.ctrl_chip[j], j == i);
    }
}

void ctrl_step_cb(lv_event_t* e) {
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    if (step <= 0) return;
    g_move_step = step;
    for (int i = 0; i < 3; i++)
        style_chip(R.ctrl_step_chip[i], (i == 0 && step == 1) || (i == 1 && step == 10) ||
                                           (i == 2 && step == 50));
}

void move_axis_cb(lv_event_t* e) {
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    if (v == 99) {
        if (g_home_pending) return;
        if (!MR || !MR->send_homing()) {
            notice_error("回零未发送", "打印机未连接或只读模式");
            return;
        }
        g_home_pending = true;
        g_home_at = lv_tick_get();
        notice_show(false, false, IC_HOME, "正在回零…", "等待打印机完成", 300000);
        return;
    }
    if (g_home_pending) {
        notice_error("正在回零", "请等回零完成后再移动");
        return;
    }
    if (MR) {
        PrinterState s = MR->snapshot();
        const std::string& h = s.homed_axes;
        bool homed = h.find('x') != std::string::npos &&
                     h.find('y') != std::string::npos &&
                     h.find('z') != std::string::npos;
        if (!homed) {
            notice_error("请先回零", "移动控制需要先执行回零");
            return;
        }
    }
    int axis = v / 10;
    int dir = (v % 10) - 1;
    if (dir != -1 && dir != 1) return;
    char cmd[96];
    if (axis == 2)
        snprintf(cmd, sizeof(cmd), "G91\nG1 Z%d F600\nG90", dir * g_move_step);
    else
        snprintf(cmd, sizeof(cmd), "G91\nG1 %c%d F3000\nG90", axis == 0 ? 'X' : 'Y',
                 dir * g_move_step);
    send_action(cmd);
}

void build_control_nozzles(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);

    lv_obj_t* nrow = mk_obj(p);
    lv_obj_set_width(nrow, LV_PCT(100));
    lv_obj_set_flex_grow(nrow, 1);
    lv_obj_set_flex_flow(nrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(nrow, 6, 0);

    for (int i = 0; i < 4; i++) {
        lv_obj_t* card = mk_obj(nrow);
        style_card(card, theme::SURFACE);
        lv_obj_set_height(card, LV_PCT(100));
        lv_obj_set_flex_grow(card, 1);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_row(card, 2, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, theme::c(theme::OUTLINE), 0);
        lv_obj_set_style_border_opa(card, LV_OPA_60, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, nozzle_card_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        R.nozzle_card[i] = card;

        lv_obj_t* top = mk_obj(card);
        lv_obj_set_size(top, LV_PCT(100), 18);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(top, 5, 0);
        lv_obj_remove_flag(top, LV_OBJ_FLAG_CLICKABLE);

        R.nozzle_swatch[i] = mk_obj(top);
        lv_obj_set_size(R.nozzle_swatch[i], 16, 16);
        lv_obj_set_style_radius(R.nozzle_swatch[i], 5, 0);
        lv_obj_remove_flag(R.nozzle_swatch[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(R.nozzle_swatch[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(R.nozzle_swatch[i], 1, 0);
        lv_obj_set_style_border_color(R.nozzle_swatch[i], theme::c(theme::OUTLINE), 0);

        char tn[4];
        snprintf(tn, sizeof(tn), "T%d", i + 1);
        mk_label(top, tn, 13, true, theme::MUTED);

        R.nozzle_nfc[i] = mk_icon(top, IC_NFC, 14, theme::PRIMARY);
        lv_obj_add_flag(R.nozzle_nfc[i], LV_OBJ_FLAG_HIDDEN);

        R.nozzle_temp[i] = mk_label(card, "--°", 26, true, theme::TEXT);
        R.nozzle_target[i] = mk_label(card, "未加热", 11, false, theme::MUTED);
        lv_obj_set_width(R.nozzle_target[i], LV_PCT(100));
        lv_label_set_long_mode(R.nozzle_target[i], LV_LABEL_LONG_MODE_DOTS);

        lv_obj_t* mrow = mk_obj(card);
        lv_obj_remove_flag(mrow, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(mrow, LV_PCT(100), 20);
        lv_obj_set_flex_flow(mrow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(mrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(mrow, 3, 0);
        R.nozzle_mat[i] = mk_label(mrow, "—", 12, false, theme::MUTED);
        lv_obj_set_height(R.nozzle_mat[i], 20);
        lv_obj_set_flex_grow(R.nozzle_mat[i], 1);
        lv_label_set_long_mode(R.nozzle_mat[i], LV_LABEL_LONG_MODE_DOTS);

        lv_obj_t* fi = mk_obj(mrow);
        lv_obj_set_size(fi, 18, 20);
        lv_obj_remove_flag(fi, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* line = mk_obj(fi);
        lv_obj_set_size(line, 3, 4);
        lv_obj_set_style_radius(line, 1, 0);
        lv_obj_set_style_bg_color(line, theme::c(theme::MUTED), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* fh = mk_icon(fi, IC_TOOLHEAD, 14, theme::MUTED);
        lv_obj_align(fh, LV_ALIGN_BOTTOM_MID, 0, 0);
        R.nozzle_feed[i] = fi;
        R.nozzle_feed_line[i] = line;
        R.nozzle_feed_head[i] = fh;
    }

    lv_obj_t* bed = mk_card(p, theme::SURFACE);
    lv_obj_set_size(bed, LV_PCT(100), 58);
    lv_obj_set_style_pad_all(bed, 10, 0);
    lv_obj_set_flex_flow(bed, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bed, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bed, 8, 0);
    lv_obj_add_flag(bed, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bed, open_bed_temp_cb, LV_EVENT_CLICKED, nullptr);
    mk_icon(bed, IC_BED, 22, theme::PRIMARY);
    mk_label(bed, "热床", 13, false, theme::MUTED);
    lv_obj_t* sp = mk_obj(bed);
    lv_obj_remove_flag(sp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_grow(sp, 1);
    R.temp_bed = mk_label(bed, "--", 24, true, theme::TEXT);
    R.bed_target = mk_label(bed, "未加热", 11, false, theme::MUTED);
    mk_icon(bed, IC_CHEVRON_RIGHT, 18, theme::MUTED);

}

void build_control_move(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);

    lv_obj_t* sr = mk_obj(p);
    lv_obj_set_size(sr, LV_PCT(100), 34);
    lv_obj_set_flex_flow(sr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sr, 6, 0);
    mk_label(sr, "步进", 12, false, theme::MUTED);
    R.ctrl_step_chip[0] = mk_chip(sr, "1", ctrl_step_cb, (void*)(intptr_t)1, 0, 34);
    R.ctrl_step_chip[1] = mk_chip(sr, "10", ctrl_step_cb, (void*)(intptr_t)10, 0, 34);
    R.ctrl_step_chip[2] = mk_chip(sr, "50", ctrl_step_cb, (void*)(intptr_t)50, 0, 34);
    style_chip(R.ctrl_step_chip[1], true);

    lv_obj_t* move = mk_obj(p);
    lv_obj_set_width(move, LV_PCT(100));
    lv_obj_set_height(move, 0);
    lv_obj_set_flex_grow(move, 1);
    lv_obj_set_flex_flow(move, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(move, 8, 0);

    lv_obj_t* grid = mk_obj(move);
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_height(grid, LV_PCT(100));
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(grid, 6, 0);

    for (int r = 0; r < 3; r++) {
        lv_obj_t* row = mk_obj(grid);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 0);
        lv_obj_set_flex_grow(row, 1);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 6, 0);
        for (int c = 0; c < 3; c++) {
            int code = -1;
            const char* txt = "";
            if (r == 0 && c == 1) { code = 12; txt = "Y+"; }
            if (r == 1 && c == 0) { code = 0; txt = "X-"; }
            if (r == 1 && c == 1) { code = 99; txt = "回零"; }
            if (r == 1 && c == 2) { code = 2; txt = "X+"; }
            if (r == 2 && c == 1) { code = 10; txt = "Y-"; }
            if (code < 0) {
                lv_obj_t* blank = mk_obj(row);
                lv_obj_set_flex_grow(blank, 1);
                continue;
            }
            lv_obj_t* mb = mk_button(row, txt, code == 99, move_axis_cb,
                                     (void*)(intptr_t)code, 54);
            lv_obj_set_height(mb, LV_PCT(100));
            int bi = -1;
            if (code == 0) bi = 0;
            else if (code == 2) bi = 1;
            else if (code == 10) bi = 2;
            else if (code == 12) bi = 3;
            if (bi >= 0) R.move_btn[bi] = mb;
        }
    }

    lv_obj_t* zc = mk_obj(move);
    lv_obj_set_size(zc, 120, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(zc, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(zc, 6, 0);
    lv_obj_t* zp = mk_button(zc, "Z+", false, move_axis_cb, (void*)(intptr_t)22, 0);
    lv_obj_t* zm = mk_button(zc, "Z-", false, move_axis_cb, (void*)(intptr_t)20, 0);
    lv_obj_set_flex_grow(zp, 0);
    lv_obj_set_flex_grow(zm, 0);
    R.move_btn[5] = zp;
    R.move_btn[4] = zm;
    lv_obj_set_width(zp, LV_PCT(100));
    lv_obj_set_width(zm, LV_PCT(100));
    lv_obj_set_height(zp, 80);
    lv_obj_set_height(zm, 80);

    lv_obj_t* br = mk_obj(p);
    lv_obj_set_size(br, LV_PCT(100), 40);
    lv_obj_set_flex_flow(br, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(br, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    R.move_hint = mk_label(br, "未回零：请先点回零", 11, false, theme::MUTED);
    lv_obj_t* m84 = mk_button(br, "关闭电机", false, action_cb, (void*)"M84", 40);
    lv_obj_set_flex_grow(m84, 0);
    lv_obj_set_width(m84, 120);
}

const char* kFanName[3] = {"模型", "辅助", "循环"};
const char* kFanObj[3] = {nullptr, "cavity_fan", "circulation_fan"};

void fan_set_label(int idx, int pct) {
    if (idx < 0 || idx > 2 || !R.ctrl_fan_pct[idx]) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(R.ctrl_fan_pct[idx], buf);
}

void fan_send(int idx, int pct) {
    if (!MR || idx < 0 || idx > 2) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_fan_pending[idx] = true;
    g_fan_val[idx] = pct;
    g_fan_at[idx] = lv_tick_get();
    char cmd[64];
    if (idx == 0)
        snprintf(cmd, sizeof(cmd), "M106 S%d", pct * 255 / 100);
    else
        snprintf(cmd, sizeof(cmd), "SET_FAN_SPEED FAN=%s SPEED=%.2f", kFanObj[idx],
                 pct / 100.0);
    MR->send_gcode(cmd);
    fan_set_label(idx, pct);
}

void fan_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED && code != LV_EVENT_PRESSED &&
        code != LV_EVENT_RELEASED)
        return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t* sl = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int v = (int)lv_slider_get_value(sl);
    fan_set_label(idx, v);
    if (code == LV_EVENT_PRESSED) g_fan_dragging[idx] = true;
    else if (code == LV_EVENT_RELEASED) {
        g_fan_dragging[idx] = false;
        fan_send(idx, v);
    }
}

void fan_page_apply(int page) {
    if (page < 0) page = 0;
    if (page > 1) page = 1;
    g_fan_page = page;
    if (R.ctrl_fan_page0) {
        if (g_fan_page == 0) lv_obj_remove_flag(R.ctrl_fan_page0, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.ctrl_fan_page0, LV_OBJ_FLAG_HIDDEN);
    }
    if (R.ctrl_fan_page1) {
        if (g_fan_page == 1) lv_obj_remove_flag(R.ctrl_fan_page1, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.ctrl_fan_page1, LV_OBJ_FLAG_HIDDEN);
    }
    if (R.ctrl_fan_page_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / 2", g_fan_page + 1);
        lv_label_set_text(R.ctrl_fan_page_label, buf);
    }
}

void fan_page_cb(lv_event_t* e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    fan_page_apply(g_fan_page + d);
}

lv_obj_t* mk_fan_card(lv_obj_t* parent, int idx) {
    lv_obj_t* card = mk_card(parent, theme::SURFACE);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_row(card, 4, 0);

    lv_obj_t* top = mk_obj(card);
    lv_obj_set_size(top, LV_PCT(100), 28);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top, 8, 0);
    R.ctrl_fan_icon[idx] = mk_icon(top, IC_FAN, 22, theme::MUTED);
    mk_label(top, kFanName[idx], 15, true, theme::TEXT);
    lv_obj_t* sp = mk_obj(top);
    lv_obj_remove_flag(sp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_grow(sp, 1);
    R.ctrl_fan_pct[idx] = mk_label(top, "0%", 22, true, theme::TEXT);

    lv_obj_t* sl = lv_slider_create(card);
    lv_obj_set_size(sl, LV_PCT(100), 20);
    lv_slider_set_range(sl, 0, 100);
    lv_slider_set_value(sl, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, theme::c(theme::SURFACE_HI), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, 999, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, theme::c(theme::PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sl, 999, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, theme::c(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(sl, 999, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(sl, fan_slider_cb, LV_EVENT_ALL, (void*)(intptr_t)idx);
    R.ctrl_fan_slider[idx] = sl;
    return card;
}

void build_control_fan(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);

    R.ctrl_fan_page0 = mk_obj(p);
    lv_obj_set_width(R.ctrl_fan_page0, LV_PCT(100));
    lv_obj_set_flex_grow(R.ctrl_fan_page0, 1);
    lv_obj_set_flex_flow(R.ctrl_fan_page0, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(R.ctrl_fan_page0, 6, 0);
    mk_fan_card(R.ctrl_fan_page0, 0);
    mk_fan_card(R.ctrl_fan_page0, 1);

    R.ctrl_fan_page1 = mk_obj(p);
    lv_obj_set_width(R.ctrl_fan_page1, LV_PCT(100));
    lv_obj_set_flex_grow(R.ctrl_fan_page1, 1);
    lv_obj_set_flex_flow(R.ctrl_fan_page1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(R.ctrl_fan_page1, 6, 0);
    mk_fan_card(R.ctrl_fan_page1, 2);
    mk_label(R.ctrl_fan_page1, "循环风扇仅顶盖版本有", 11, false, theme::MUTED);
    lv_obj_add_flag(R.ctrl_fan_page1, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* fr = mk_obj(p);
    lv_obj_set_size(fr, LV_PCT(100), 40);
    lv_obj_set_flex_flow(fr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fr, 10, 0);
    mk_mini_button(fr, "<", fan_page_cb, (void*)(intptr_t)-1);
    R.ctrl_fan_page_label = mk_label(fr, "1 / 2", 13, true, theme::MUTED);
    mk_mini_button(fr, ">", fan_page_cb, (void*)(intptr_t)1);
}

void build_control(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 6, 0);

    lv_obj_t* tabs = mk_obj(p);
    lv_obj_set_size(tabs, LV_PCT(100), 34);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabs, 6, 0);
    const char* names[3] = {"喷嘴", "移动", "风扇"};
    for (int i = 0; i < 3; i++) {
        R.ctrl_chip[i] = mk_chip(tabs, names[i], ctrl_tab_cb, (void*)(intptr_t)i, 0, 34);
        style_chip(R.ctrl_chip[i], i == 0);
    }

    lv_obj_t* lb = lv_button_create(tabs);
    lv_obj_set_size(lb, 76, 34);
    lv_obj_set_style_radius(lb, 999, 0);
    lv_obj_set_style_bg_color(lb, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(lb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lb, 0, 0);
    lv_obj_set_style_shadow_width(lb, 0, 0);
    lv_obj_set_style_pad_all(lb, 0, 0);
    lv_obj_set_flex_flow(lb, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lb, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lb, 4, 0);
    R.ctrl_light_icon = mk_icon(lb, IC_LIGHT, 16, theme::MUTED);
    R.ctrl_light_text = mk_label(lb, "灯光", 11, false, theme::MUTED);
    lv_obj_add_event_cb(lb, ctrl_light_cb, LV_EVENT_CLICKED, nullptr);
    R.ctrl_light_btn = lb;

    R.ctrl_body = mk_obj(p);
    lv_obj_set_width(R.ctrl_body, LV_PCT(100));
    lv_obj_set_flex_grow(R.ctrl_body, 1);

    for (int i = 0; i < 3; i++) {
        R.ctrl_tab[i] = mk_obj(R.ctrl_body);
        lv_obj_set_size(R.ctrl_tab[i], LV_PCT(100), LV_PCT(100));
        if (i != 0) lv_obj_add_flag(R.ctrl_tab[i], LV_OBJ_FLAG_HIDDEN);
    }
    build_control_nozzles(R.ctrl_tab[0]);
    build_control_move(R.ctrl_tab[1]);
    build_control_fan(R.ctrl_tab[2]);
}

void build_maintenance(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 8, 0);

    // 净化器 / 滤芯
    lv_obj_t* c1 = mk_card(p);
    lv_obj_set_size(c1, LV_PCT(100), 162);
    lv_obj_set_flex_flow(c1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c1, 5, 0);

    lv_obj_t* h1 = mk_row(c1, 22);
    mk_label(h1, "净化器 / 滤芯", 14, true, theme::TEXT);
    R.pur_mode_line = mk_label(h1, "—", 12, false, theme::MUTED);

    lv_obj_t* l1 = mk_row(c1, 26);
    R.pur_hours = mk_label(l1, "滤芯 -- h", 20, true, theme::PRIMARY);
    R.pur_rpm = mk_label(l1, "转速 -- rpm", 12, false, theme::MUTED);

    R.filter_bar = lv_bar_create(c1);
    lv_obj_set_size(R.filter_bar, LV_PCT(100), 8);
    lv_bar_set_range(R.filter_bar, 0, 2000);
    lv_obj_set_style_radius(R.filter_bar, 999, LV_PART_MAIN);
    lv_obj_set_style_radius(R.filter_bar, 999, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(R.filter_bar, theme::c(theme::OUTLINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(R.filter_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(R.filter_bar, theme::c(theme::PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(R.filter_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(R.filter_bar, 0, 0);

    lv_obj_t* sp = mk_row(c1, 32);
    mk_label(sp, "风速", 13, false, theme::MUTED);
    lv_obj_t* spg = mk_obj(sp);
    lv_obj_set_size(spg, 138, LV_PCT(100));
    lv_obj_set_flex_flow(spg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(spg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(spg, 6, 0);
    R.pur_speed_dec = mk_mini_button(spg, "-", purifier_speed_cb, (void*)(intptr_t)-1);
    R.pur_speed = mk_label(spg, "--", 14, true, theme::TEXT);
    lv_obj_set_width(R.pur_speed, 46);
    lv_obj_set_style_text_align(R.pur_speed, LV_TEXT_ALIGN_CENTER, 0);
    R.pur_speed_inc = mk_mini_button(spg, "+", purifier_speed_cb, (void*)(intptr_t)1);

    lv_obj_t* md = mk_row(c1, 32);
    mk_label(md, "模式", 13, false, theme::MUTED);
    lv_obj_t* mdg = mk_obj(md);
    lv_obj_set_size(mdg, 226, LV_PCT(100));
    lv_obj_set_flex_flow(mdg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mdg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mdg, 6, 0);
    const char* modes[] = {"待机", "降温", "预热", "保温"};
    for (int i = 0; i < 4; i++)
        R.pur_mode_btn[i] = mk_chip(mdg, modes[i], purifier_mode_cb, (void*)(intptr_t)i, 52, 28);

    // 打印头：换头次数 / 喷嘴
    lv_obj_t* c2 = mk_card(p);
    lv_obj_set_size(c2, LV_PCT(100), 92);
    lv_obj_set_flex_flow(c2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c2, 4, 0);
    mk_label(c2, "打印头", 14, true, theme::TEXT);
    R.head_line = mk_label(c2, "换头次数 —", 13, false, theme::TEXT);
    R.head_nozzle_line = mk_label(c2, "喷嘴 —", 12, false, theme::MUTED);

    // 空打检测（耗材缠绕）
    lv_obj_t* c3 = mk_card(p);
    lv_obj_set_size(c3, LV_PCT(100), 92);
    lv_obj_set_flex_flow(c3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c3, 6, 0);
    lv_obj_t* h3 = mk_row(c3, 30);
    mk_label(h3, "空打检测", 14, true, theme::TEXT);
    R.ent_sw = mk_switch(h3, false, pref_switch_cb, (void*)"FILAMENT_ENTANGLE_DETECT");
    lv_obj_t* s3 = mk_row(c3, 30);
    mk_label(s3, "灵敏度", 13, false, theme::MUTED);
    lv_obj_t* g3 = mk_obj(s3);
    lv_obj_set_size(g3, 168, LV_PCT(100));
    lv_obj_set_flex_flow(g3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g3, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g3, 6, 0);
    const char* esens[] = {"低", "中", "高"};
    for (int i = 0; i < 3; i++)
        R.ent_seg[i] = mk_chip(g3, esens[i], entangle_sens_cb, (void*)(intptr_t)i, 52, 28);

    // 异物检测
    lv_obj_t* c4 = mk_card(p);
    lv_obj_set_size(c4, LV_PCT(100), 134);
    lv_obj_set_flex_flow(c4, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c4, 6, 0);
    lv_obj_t* h4 = mk_row(c4, 30);
    mk_label(h4, "异物检测", 14, true, theme::TEXT);
    R.def_sw = mk_switch(h4, false, defect_main_cb, nullptr);

    lv_obj_t* g4 = mk_row(c4, 34);
    const char* groups[] = {"空床", "面条", "残料", "喷嘴"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* cell = mk_obj(g4);
        lv_obj_set_height(cell, LV_PCT(100));
        lv_obj_set_flex_grow(cell, 1);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(cell, 4, 0);
        mk_label(cell, groups[i], 12, false, theme::MUTED);
        R.def_group_sw[i] = mk_switch(cell, false, defect_group_cb, (void*)(intptr_t)i);
    }

    lv_obj_t* s4 = mk_row(c4, 30);
    mk_label(s4, "灵敏度", 13, false, theme::MUTED);
    lv_obj_t* g5 = mk_obj(s4);
    lv_obj_set_size(g5, 110, LV_PCT(100));
    lv_obj_set_flex_flow(g5, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g5, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g5, 6, 0);
    R.def_seg[0] = mk_chip(g5, "低", defect_sens_cb, (void*)(intptr_t)0, 52, 28);
    R.def_seg[1] = mk_chip(g5, "高", defect_sens_cb, (void*)(intptr_t)1, 52, 28);

    lv_obj_t* acts = mk_obj(p);
    lv_obj_set_size(acts, LV_PCT(100), 50);
    lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(acts, 8, 0);
    mk_button(acts, "净化器自检", false, action_cb, (void*)"PURIFIER_TEST");
    mk_button(acts, "清洁喷嘴", false, action_cb, (void*)"ROUGHLY_CLEAN_NOZZLE");
    mk_button(acts, "床网校准", false, action_cb, (void*)"AUTO_BED_MESH_CALIBRATE");
    mk_button(acts, "滤芯清零", false, action_cb, (void*)"PURIFIER_RESET_FILTER");
}

void build_settings(lv_obj_t* p) {
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 8, 0);

    // 设备信息
    lv_obj_t* c2 = mk_card(p);
    lv_obj_set_size(c2, LV_PCT(100), 132);
    mk_label(c2, "设备信息", 14, true, theme::TEXT);
    R.dev_line = mk_label(c2, "—", 13, false, theme::MUTED);
    lv_obj_align(R.dev_line, LV_ALIGN_TOP_LEFT, 0, 24);
    R.dev_nozzle = mk_label(c2, "—", 13, false, theme::MUTED);
    lv_obj_align(R.dev_nozzle, LV_ALIGN_TOP_LEFT, 0, 46);
    R.gui_line = mk_label(c2, "原厂 GUI 偏好 · 自动映射 —", 11, false, theme::MUTED);
    lv_obj_align(R.gui_line, LV_ALIGN_TOP_LEFT, 0, 68);
    lv_obj_t* ver = mk_label(c2, "skreeu1 1.6.3 · LVGL + FreeType · 480×320", 11, false, 0x5A616B);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // 打印偏好（Klipper print_task_config）
    lv_obj_t* cp = mk_card(p);
    lv_obj_set_size(cp, LV_PCT(100), 200);
    lv_obj_set_flex_flow(cp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cp, 6, 0);
    mk_label(cp, "打印偏好", 14, true, theme::TEXT);

    struct Pref {
        const char* txt;
        lv_obj_t** slot;
        const char* key;
    };
    const Pref prefs[] = {
        {"自动调平", &R.pref_bedlevel_sw, "BED_LEVEL"},
        {"流量校准", &R.pref_flow_sw, "FLOW_CALIBRATE"},
        {"振动校准", &R.pref_shaper_sw, "SHAPER_CALIBRATE"},
        {"延时摄影", &R.pref_timelapse_sw, "TIME_LAPSE_CAMERA"},
        {"自动续料", &R.pref_replenish_sw, "AUTO_REPLENISH_FILAMENT"},
        {"忽略颜色", &R.pref_ignore_color_sw, "REPLENISH_IGNORE_COLOR"},
        {"结束关灯", &R.pref_end_led_sw, "END_LED_TURN_OFF"},
    };
    for (int i = 0; i < 7; i++) {
        lv_obj_t* row;
        if (i % 2 == 0) row = mk_row(cp, 34);
        else row = lv_obj_get_child(cp, lv_obj_get_child_count(cp) - 1);
        lv_obj_t* cell = mk_obj(row);
        lv_obj_set_height(cell, LV_PCT(100));
        lv_obj_set_flex_grow(cell, 1);
        if (i == 6) {
            // 最后一行只有一项：占左半格，别横跨整行
            lv_obj_set_flex_grow(cell, 0);
            lv_obj_set_width(cell, LV_PCT(50));
        }
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(cell, 6, 0);
        mk_label(cell, prefs[i].txt, 12, false, theme::MUTED);
        *prefs[i].slot = mk_switch(cell, false, pref_switch_cb, (void*)prefs[i].key);
    }

    // 运行统计
    lv_obj_t* c3 = mk_card(p);
    lv_obj_set_size(c3, LV_PCT(100), 92);
    mk_label(c3, "运行统计", 14, true, theme::TEXT);
    R.stat_line = mk_label(c3, "—", 14, false, theme::TEXT);
    lv_obj_align(R.stat_line, LV_ALIGN_TOP_LEFT, 0, 26);
    mk_label(c3, "数据来自 Moonraker 历史库", 11, false, theme::OUTLINE);
    lv_obj_t* sl = lv_obj_get_child(c3, 2);
    lv_obj_align(sl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // 连接
    lv_obj_t* c1 = mk_card(p);
    lv_obj_set_size(c1, LV_PCT(100), 84);
    mk_label(c1, "连接", 14, true, theme::TEXT);
    R.conn_line = mk_label(c1, "—", 13, false, theme::MUTED);
    lv_obj_align(R.conn_line, LV_ALIGN_TOP_LEFT, 0, 24);

    // 网络（点整张卡进入管理面板）
    lv_obj_t* cw = mk_card(p);
    lv_obj_set_size(cw, LV_PCT(100), 84);
    lv_obj_add_flag(cw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cw, net_card_cb, LV_EVENT_CLICKED, nullptr);
    mk_label(cw, "网络", 14, true, theme::TEXT);
    R.net_line = mk_label(cw, "—", 13, false, theme::MUTED);
    lv_obj_align(R.net_line, LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_t* ch = mk_icon(cw, IC_CHEVRON_RIGHT, 18, theme::MUTED);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t* acts = mk_obj(p);
    lv_obj_set_size(acts, LV_PCT(100), 50);
    lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(acts, 8, 0);
    mk_button(acts, "关灯", false, action_cb, (void*)"SET_LED LED=cavity_led WHITE=0");
    mk_button(acts, "开灯", false, action_cb, (void*)"SET_LED LED=cavity_led WHITE=1");
}


// ---------------------------------------------------------------- 网络

struct NetPick {
    std::string ssid;
    bool sae = false;
    bool secure = false;
};

const char* wifi_signal_icon(int rssi) {
    if (rssi >= -55) return IC_WIFI_4;
    if (rssi >= -67) return IC_WIFI_3;
    if (rssi >= -75) return IC_WIFI_2;
    return IC_WIFI_1;
}

void net_close_cb(lv_event_t*) {
    if (R.net_page) lv_obj_add_flag(R.net_page, LV_OBJ_FLAG_HIDDEN);
    if (R.print_sheet) lv_obj_add_flag(R.print_sheet, LV_OBJ_FLAG_HIDDEN);
    g_net_open = false;
}

void net_pw_close_cb(lv_event_t*) {
    if (R.net_pw_page) lv_obj_add_flag(R.net_pw_page, LV_OBJ_FLAG_HIDDEN);
    g_net_pw_open = false;
}

void net_pw_connect_cb(lv_event_t*) {
    if (!WIFI || !R.net_pw_ta) return;
    const char* pass = lv_textarea_get_text(R.net_pw_ta);
    if (!pass || !*pass) {
        show_toast("请输入 Wi-Fi 密码");
        return;
    }
    g_net_target = g_net_pw_ssid;
    WIFI->request_connect(g_net_pw_ssid, pass, g_net_pw_sae);
    net_pw_close_cb(nullptr);
    show_toast("正在连接…");
}

void net_pw_kb_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) net_pw_connect_cb(e);
    else if (code == LV_EVENT_CANCEL) net_pw_close_cb(e);
}

void open_net_pw(const std::string& ssid, bool sae) {
    if (!R.net_pw_page) return;
    g_net_pw_ssid = ssid;
    g_net_pw_sae = sae;
    g_net_pw_open = true;
    if (R.net_pw_title) lv_label_set_text(R.net_pw_title, ssid.c_str());
    if (R.net_pw_ta) lv_textarea_set_text(R.net_pw_ta, "");
    lv_obj_remove_flag(R.net_pw_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.net_pw_page, -1);
    if (R.net_pw_kb && R.net_pw_ta) lv_keyboard_set_textarea(R.net_pw_kb, R.net_pw_ta);
}

void net_row_del_cb(lv_event_t* e) {
    delete static_cast<NetPick*>(lv_event_get_user_data(e));
}

void net_row_cb(lv_event_t* e) {
    NetPick* pick = static_cast<NetPick*>(lv_event_get_user_data(e));
    if (!pick || !WIFI) return;
    if (pick->secure) {
        open_net_pw(pick->ssid, pick->sae);
    } else {
        g_net_target = pick->ssid;
        WIFI->request_connect(pick->ssid, "", false);
        show_toast("正在连接…");
    }
}

void net_rebuild_list(const WifiSnapshot& s) {
    if (!R.net_list) return;
    lv_obj_clean(R.net_list);

    if (s.networks.empty()) {
        lv_obj_t* hint = mk_label(R.net_list, s.scanning ? "正在扫描…" : "没有找到网络，点扫描重试",
                                  13, false, theme::MUTED);
        lv_obj_set_width(hint, LV_PCT(100));
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(hint, 24, 0);
        return;
    }

    for (const auto& n : s.networks) {
        bool conn = s.connected && n.ssid == s.ssid;
        lv_obj_t* row = lv_button_create(R.net_list);
        lv_obj_set_size(row, LV_PCT(100), 40);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_set_style_bg_color(row, theme::c(conn ? theme::SURFACE_HI : theme::SURFACE), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 8, 0);

        mk_icon(row, wifi_signal_icon(n.signal), 18,
                conn ? theme::PRIMARY : theme::MUTED);
        lv_obj_t* name = mk_label(row, n.ssid.c_str(), 14, conn, theme::TEXT);
        lv_obj_set_flex_grow(name, 1);
        lv_obj_set_style_max_width(name, 230, 0);
        lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
        if (n.secure) mk_icon(row, IC_LOCK, 14, theme::MUTED);
        if (conn) mk_icon(row, IC_CHECK_CIRCLE, 16, theme::PRIMARY);

        NetPick* pick = new NetPick{n.ssid, n.sae, n.secure};
        lv_obj_add_event_cb(row, net_row_cb, LV_EVENT_CLICKED, pick);
        lv_obj_add_event_cb(row, net_row_del_cb, LV_EVENT_DELETE, pick);
    }
}

void net_refresh_ui(const WifiSnapshot& s) {
    if (R.net_status) {
        if (s.restarting) {
            lv_label_set_text(R.net_status, "正在重启 Wi-Fi 服务…");
        } else if (!s.available) {
            lv_label_set_text(R.net_status, "Wi-Fi 服务未运行");
        } else if (s.connecting) {
            lv_label_set_text(R.net_status, "正在连接…");
        } else if (s.connected) {
            lv_label_set_text(R.net_status, s.ssid.empty() ? "已连接" : s.ssid.c_str());
        } else if (s.scanning) {
            lv_label_set_text(R.net_status, "正在扫描…");
        } else {
            lv_label_set_text(R.net_status, "未连接");
        }
    }
    if (R.net_sub) {
        char buf[128];
        if (s.restarting) {
            snprintf(buf, sizeof(buf), "正在重新拉起 wpa_supplicant");
        } else if (!s.available) {
            snprintf(buf, sizeof(buf), "点「重启服务」尝试恢复");
        } else if (s.connecting) {
            snprintf(buf, sizeof(buf), "%s", g_net_target.empty() ? "请稍候" : g_net_target.c_str());
        } else if (s.connected) {
            snprintf(buf, sizeof(buf), "%s · %d dBm",
                     s.ip.empty() ? "等待 IP" : s.ip.c_str(), s.rssi);
        } else {
            snprintf(buf, sizeof(buf), "点按下面的网络进行连接");
        }
        lv_label_set_text(R.net_sub, buf);
    }
    if (R.net_scan_btn) {
        if (s.scanning || s.connecting || !s.available)
            lv_obj_add_state(R.net_scan_btn, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(R.net_scan_btn, LV_STATE_DISABLED);
    }
    if (R.net_rec_btn) {
        lv_obj_t* lb = lv_obj_get_child(R.net_rec_btn, 0);
        if (lb) lv_label_set_text(lb, s.available ? "重连" : "重启服务");
    }
    if (R.net_dis_btn) {
        if (!s.connected) lv_obj_add_state(R.net_dis_btn, LV_STATE_DISABLED);
        else lv_obj_remove_state(R.net_dis_btn, LV_STATE_DISABLED);
    }

    bool dirty = s.scan_gen != g_net_scan_gen || s.connected != g_net_last_conn ||
                 s.ssid != g_net_last_ssid;
    if (dirty) {
        g_net_scan_gen = s.scan_gen;
        g_net_last_conn = s.connected;
        g_net_last_ssid = s.ssid;
        net_rebuild_list(s);
    }
}

void net_refresh_cb(lv_event_t*) {
    g_net_scan_at = lv_tick_get();
    if (WIFI) WIFI->request_scan();
    if (R.net_status) lv_label_set_text(R.net_status, "正在扫描…");
}

void net_disconnect_cb(lv_event_t*) {
    if (WIFI) WIFI->request_disconnect();
    show_toast("已断开 Wi-Fi");
}

void net_reconnect_cb(lv_event_t*) {
    if (!WIFI) return;
    WifiSnapshot s = WIFI->snapshot();
    if (s.restarting) {
        show_toast("Wi-Fi 服务正在重启…");
        return;
    }
    if (!s.available) {
        WIFI->request_service_restart();
        g_feed_dismissed = false;
        g_wifi_restart_seen = true;
        notice_show(false, false, IC_WIFI, "正在重启 Wi-Fi 服务…", "正在重新拉起 wpa_supplicant", 0);
        return;
    }
    WIFI->request_reconnect();
    show_toast("正在重新连接…");
}

void open_net_page() {
    if (!R.net_page) return;
    g_net_open = true;
    lv_obj_remove_flag(R.net_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.net_page, -1);
    if (!WIFI) return;
    WIFI->request_status();
    WifiSnapshot s = WIFI->snapshot();
    if (s.networks.empty() || lv_tick_get() - g_net_scan_at > 30000) {
        g_net_scan_at = lv_tick_get();
        WIFI->request_scan();
    }
    net_refresh_ui(s);
}

void net_card_cb(lv_event_t*) { open_net_page(); }

void build_net_page(lv_obj_t* screen) {
    lv_obj_t* pg = mk_overlay(screen);
    R.net_page = pg;

    lv_obj_t* head = mk_obj(pg);
    lv_obj_set_size(head, LV_PCT(100), 30);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* hl = mk_obj(head);
    lv_obj_set_height(hl, LV_PCT(100));
    lv_obj_set_flex_flow(hl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hl, 8, 0);
    mk_label(hl, "网络", 20, true, theme::TEXT);
    mk_close_button(head, net_close_cb);

    lv_obj_t* sc = mk_card(pg, theme::SURFACE);
    lv_obj_set_size(sc, LV_PCT(100), 58);
    R.net_status = mk_label(sc, "未连接", 15, true, theme::TEXT);
    R.net_sub = mk_label(sc, "点按下面的网络进行连接", 11, false, theme::MUTED);
    lv_obj_align(R.net_sub, LV_ALIGN_TOP_LEFT, 0, 24);

    lv_obj_t* list = mk_obj(pg);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    R.net_list = list;

    lv_obj_t* ft = mk_obj(pg);
    lv_obj_set_size(ft, LV_PCT(100), 38);
    lv_obj_set_flex_flow(ft, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ft, 8, 0);
    R.net_scan_btn = mk_button(ft, "扫描", false, net_refresh_cb, nullptr, 38);
    R.net_rec_btn = mk_button(ft, "重连", false, net_reconnect_cb, nullptr, 38);
    R.net_dis_btn = mk_button(ft, "断开", false, net_disconnect_cb, nullptr, 38);
    for (lv_obj_t* b : {R.net_scan_btn, R.net_rec_btn, R.net_dis_btn}) {
        lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, theme::c(theme::OUTLINE), 0);
    }

    // 密码浮层
    lv_obj_t* pw = mk_overlay(screen);
    R.net_pw_page = pw;

    lv_obj_t* ph = mk_obj(pw);
    lv_obj_set_size(ph, LV_PCT(100), 28);
    lv_obj_set_flex_flow(ph, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ph, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    R.net_pw_title = mk_label(ph, "Wi-Fi 密码", 17, true, theme::TEXT);
    mk_close_button(ph, net_pw_close_cb);

    lv_obj_t* ta = lv_textarea_create(pw);
    lv_obj_set_size(ta, LV_PCT(100), 38);
    lv_obj_set_style_bg_color(ta, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_radius(ta, 8, 0);
    lv_obj_set_style_pad_hor(ta, 8, 0);
    lv_obj_set_style_pad_ver(ta, 4, 0);
    lv_obj_set_style_text_font(ta, theme::font(15, false), 0);
    lv_obj_set_style_text_color(ta, theme::c(theme::TEXT), 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 63);
    lv_textarea_set_placeholder_text(ta, "密码");
    R.net_pw_ta = ta;

    lv_obj_t* kb = lv_keyboard_create(pw);
    lv_obj_set_size(kb, LV_PCT(100), 148);
    lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(kb, 0, 0);
    lv_obj_set_style_pad_row(kb, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_column(kb, 4, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, theme::c(theme::SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, theme::c(theme::TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, theme::c(theme::SURFACE_HI), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, net_pw_kb_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, net_pw_kb_cb, LV_EVENT_CANCEL, nullptr);
    R.net_pw_kb = kb;

    lv_obj_t* pft = mk_obj(pw);
    lv_obj_set_size(pft, LV_PCT(100), 34);
    lv_obj_set_flex_flow(pft, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(pft, 8, 0);
    lv_obj_t* cancel = mk_button(pft, "取消", false, net_pw_close_cb, nullptr, 34);
    lv_obj_set_style_bg_color(cancel, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_border_width(cancel, 1, 0);
    lv_obj_set_style_border_color(cancel, theme::c(theme::OUTLINE), 0);
    mk_button(pft, "连接", true, net_pw_connect_cb, nullptr, 34);
}


// ---------------------------------------------------------------- 进料/退料通知

bool feed_state_active(const std::string& st) {
    static const char* kActive[] = {
        "preload_prepare", "preload_feeding",
        "load_prepare", "load_homing", "load_picking", "load_heating",
        "load_feeding", "load_extruding", "load_flushing",
        "unload_prepare", "unload_homing", "unload_picking", "unload_heating",
        "unload_heat_finish", "unload_doing",
        "wait_insert",
        "manual_sta_prepare", "manual_sta_homing", "manual_sta_picking",
        "manual_sta_heating", "manual_sta_extruding", "manual_sta_flushing",
    };
    for (const char* a : kActive)
        if (st == a) return true;
    return false;
}

std::string feed_action(const std::string& st) {
    if (st.rfind("unload", 0) == 0) return "unload";
    if (st.rfind("load", 0) == 0) return "load";
    if (st.rfind("preload", 0) == 0) return "preload";
    if (st.rfind("manual", 0) == 0) return "manual";
    if (st == "wait_insert") return "wait";
    return "load";
}

std::string feed_state_text(const std::string& st) {
    if (st == "load_prepare" || st == "unload_prepare" || st == "preload_prepare")
        return "准备中";
    if (st == "load_homing" || st == "unload_homing") return "移动到换料位";
    if (st == "load_picking" || st == "unload_picking") return "切换喷嘴";
    if (st == "load_heating" || st == "unload_heating") return "加热喷嘴";
    if (st == "load_feeding" || st == "preload_feeding") return "送入耗材";
    if (st == "load_extruding") return "挤出耗材";
    if (st == "load_flushing") return "冲洗喷嘴";
    if (st == "unload_heat_finish") return "等待退料";
    if (st == "unload_doing") return "退出耗材";
    if (st == "wait_insert") return "等待插入耗材";
    if (st == "manual_sta_heating" || st == "manual_sta_extruding" ||
        st == "manual_sta_flushing")
        return "手动进料";
    return st;
}

std::string feed_title(const std::string& act, bool finished, bool failed, int tool) {
    std::string t = "T" + std::to_string(tool + 1) + " ";
    if (failed) t += (act == "unload" ? "退料失败" : "进料失败");
    else if (finished) t += (act == "unload" ? "退料完成" : "进料完成");
    else if (act == "unload") t += "退料中";
    else if (act == "preload") t += "预加载";
    else if (act == "wait") t += "等待耗材";
    else if (act == "manual") t += "手动进料";
    else t += "进料中";
    return t;
}

void feed_banner_set(bool error, bool success, const char* icon, const char* title,
                     const char* sub) {
    if (!R.feed_banner) return;
    uint32_t bg = error ? 0x3A1F1F : (success ? theme::SUCCESS_DIM : theme::SURFACE_HI);
    uint32_t fg = error ? theme::DANGER : (success ? theme::SUCCESS : theme::TEXT);
    lv_obj_set_style_bg_color(R.feed_banner, theme::c(bg), 0);
    lv_obj_set_style_border_color(R.feed_banner,
                                  theme::c(error ? theme::DANGER : theme::OUTLINE), 0);
    if (R.feed_banner_icon) {
        lv_label_set_text(R.feed_banner_icon, icon);
        lv_obj_set_style_text_color(R.feed_banner_icon, theme::c(fg), 0);
    }
    if (R.feed_banner_title) {
        lv_label_set_text(R.feed_banner_title, title);
        lv_obj_set_style_text_color(R.feed_banner_title, theme::c(fg), 0);
    }
    if (R.feed_banner_sub) {
        lv_label_set_text(R.feed_banner_sub, sub);
        lv_obj_set_style_text_color(R.feed_banner_sub,
                                    theme::c(error ? theme::DANGER : theme::MUTED), 0);
    }
    if (R.feed_banner_spin) {
        if (error || success) lv_obj_add_flag(R.feed_banner_spin, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(R.feed_banner_spin, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(R.feed_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(R.feed_banner, -1);
}

void feed_banner_hide() {
    if (R.feed_banner) lv_obj_add_flag(R.feed_banner, LV_OBJ_FLAG_HIDDEN);
    g_notice_until = 0;
}

void feed_banner_gesture_cb(lv_event_t*) {
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        feed_banner_hide();
        g_feed_dismissed = true;
    }
}

// 顶部通知是全局标准提示：进料状态、操作失败都走这里
void notice_show(bool error, bool success, const char* icon, const std::string& title,
                 const std::string& sub, uint32_t duration_ms) {
    feed_banner_set(error, success, icon, title.c_str(), sub.c_str());
    g_notice_until = duration_ms ? lv_tick_get() + duration_ms : 0;
}

void notice_error(const std::string& title, const std::string& sub) {
    g_feed_dismissed = false;
    notice_show(true, false, IC_CLOSE, title, sub, 5000);
}

void feed_tick(const PrinterState& s) {
    if (!R.feed_banner) return;
    uint32_t now = lv_tick_get();
    if (g_notice_until) {
        if ((int32_t)(g_notice_until - now) > 0) return;
        g_notice_until = 0;
    }
    int active = -1;
    int err = -1;
    std::string astate, estate;
    auto pick_active = [&](int i) {
        if (i < 0 || i > 3 || active >= 0) return;
        if (feed_state_active(s.feed[i].state)) {
            active = i;
            astate = s.feed[i].state;
        }
    };
    for (int i = 0; i < 4; i++) {
        const FeedChannel& f = s.feed[i];
        if (err < 0 && ((!f.error.empty() && f.error != "ok") ||
                        f.state.find("fail") != std::string::npos)) {
            err = i;
            estate = (!f.error.empty() && f.error != "ok") ? f.error : f.state;
        }
    }
    pick_active(g_feed_pref_tool);
    pick_active(g_sheet_tool);
    for (int i = 0; i < 4; i++) pick_active(i);

    if (err >= 0) {
        std::string key = std::to_string(err) + ":" + estate;
        if (key != g_feed_err_seen) g_feed_dismissed = false;
    }
    if (g_feed_dismissed) {
        if (active < 0) {
            g_feed_was_active = false;
            g_feed_local = false;
        }
        if (active < 0 && err < 0) g_feed_dismissed = false;
        if (g_feed_dismissed) return;
    }

    if (err >= 0) {
        std::string key = std::to_string(err) + ":" + estate;
        if (key != g_feed_err_seen) {
            g_feed_err_seen = key;
            std::string act = feed_action(astate);
            std::string title = feed_title(act, false, true, err);
            notice_show(true, false, IC_CLOSE, title, estate, 6000);
            g_feed_was_active = false;
            g_feed_local = false;
        }
    } else {
        g_feed_err_seen.clear();
    }
    if (g_notice_until && (int32_t)(g_notice_until - now) > 0) return;

    if (active >= 0) {
        std::string act = feed_action(astate);
        notice_show(false, false, act == "unload" ? IC_UNLOAD : IC_LOAD,
                    feed_title(act, false, false, active),
                    feed_state_text(astate), 0);
        g_feed_was_active = true;
        g_feed_active_tool = active;
        g_feed_active_state = astate;
        g_feed_local = false;
        g_feed_done_until = 0;
        return;
    }

    if (g_feed_was_active) {
        bool failed = g_feed_active_state.find("fail") != std::string::npos;
        std::string act = feed_action(g_feed_active_state);
        notice_show(failed, !failed,
                    failed ? IC_CLOSE : IC_CHECK_CIRCLE,
                    feed_title(act, true, failed, g_feed_active_tool),
                    failed ? "检查耗材和喷嘴" : "完成", failed ? 6000 : 2200);
        g_feed_done_until = now + (failed ? 6000 : 2200);
        g_feed_was_active = false;
        g_feed_pref_tool = -1;
        return;
    }

    if (g_feed_done_until && now < g_feed_done_until) return;

    if (g_feed_local) {
        if (now - g_feed_local_at < 15000) {
            std::string act = g_feed_local_kind == 2 ? "unload" : "load";
            notice_show(false, false, act == "unload" ? IC_UNLOAD : IC_LOAD,
                        feed_title(act, false, false, g_feed_local_tool),
                        "等待打印机…", 0);
            return;
        }
        g_feed_local = false;
    }
    feed_banner_hide();
}

void feed_icon_apply(int i, int mode) {
    if (i < 0 || i > 3) return;
    uint32_t line_c = theme::MUTED, head_c = theme::MUTED;
    if (mode == 1 || mode == 2) {
        line_c = theme::PRIMARY;
        head_c = theme::PRIMARY;
    } else if (mode == 4) {
        line_c = theme::DANGER;
        head_c = theme::DANGER;
    }
    if (R.nozzle_feed_line[i])
        lv_obj_set_style_bg_color(R.nozzle_feed_line[i], theme::c(line_c), 0);
    if (R.nozzle_feed_head[i])
        lv_obj_set_style_text_color(R.nozzle_feed_head[i], theme::c(head_c), 0);
    if (R.nozzle_feed_line[i])
        lv_obj_set_style_translate_y(R.nozzle_feed_line[i], mode == 1 ? 2 : 0, 0);
}

void feed_icons_tick(lv_timer_t*) {
    uint32_t t = lv_tick_get();
    int phase = (int)((t / 180) % 2);
    for (int i = 0; i < 4; i++) {
        int m = g_nozzle_feed_mode[i];
        if (!R.nozzle_feed_line[i]) continue;
        if (m == 2) lv_obj_set_style_translate_y(R.nozzle_feed_line[i], 1 + phase, 0);
        else if (m == 3) lv_obj_set_style_translate_y(R.nozzle_feed_line[i], 1 - phase, 0);
    }
}

bool any_overlay_visible() {
    auto vis = [](lv_obj_t* o) { return o && !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN); };
    return vis(R.sheet) || vis(R.picker) || vis(R.temp_page) || vis(R.scrim) ||
           vis(R.keypad) || vis(R.nz_page) || vis(R.color_page) || vis(R.net_page) ||
           vis(R.net_pw_page) || vis(R.print_sheet);
}

void update_feed_banner_pos() {
    if (!R.feed_banner) return;
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";
    (void)any_overlay_visible;
    int y = 6;
    if (!any_overlay_visible()) {
        if (active_page == 0) y = 42;       // 首页：别挡标题
        else if (active_page == 1) y = 48;  // 控制页：别挡子标签
    }
    lv_obj_align(R.feed_banner, LV_ALIGN_TOP_MID, rail_right ? -36 : 36, y);
}

void build_feed_banner(lv_obj_t* screen) {
    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    lv_obj_t* b = mk_obj(screen);
    lv_obj_set_size(b, 280, 42);
    lv_obj_align(b, LV_ALIGN_TOP_MID, rail_right ? -36 : 36, 6);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_pad_all(b, 10, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(b, 16, 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(b, theme::c(0x000000), 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 8, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, feed_banner_gesture_cb, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    R.feed_banner = b;

    R.feed_banner_icon = mk_icon(b, IC_LOAD, 18, theme::PRIMARY);

    lv_obj_t* col = mk_obj(b);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 0, 0);
    R.feed_banner_title = mk_label(col, "", 13, true, theme::TEXT);
    lv_obj_set_width(R.feed_banner_title, LV_PCT(100));
    lv_label_set_long_mode(R.feed_banner_title, LV_LABEL_LONG_MODE_DOTS);
    R.feed_banner_sub = mk_label(col, "", 10, false, theme::MUTED);
    lv_obj_set_width(R.feed_banner_sub, LV_PCT(100));
    lv_label_set_long_mode(R.feed_banner_sub, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* sp = lv_spinner_create(b);
    lv_obj_set_size(sp, 18, 18);
    lv_obj_set_style_arc_width(sp, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(sp, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sp, theme::c(theme::OUTLINE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, theme::c(theme::PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sp, LV_OPA_TRANSP, LV_PART_KNOB);
    R.feed_banner_spin = sp;
}

} // namespace

// ---------------------------------------------------------------- public

void ui_init(Moonraker* mr, lv_obj_t* screen, Wifi* wifi) {
    MR = mr;
    load_presets();
    load_bed_presets();
    load_brands();

    WIFI = wifi;
    lv_obj_set_style_bg_color(screen, theme::c(theme::BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    const char* rail_env = getenv("SKREEU1_RAIL");
    const bool rail_right = rail_env && std::string(rail_env) == "right";

    lv_obj_t* content = mk_obj(screen);
    lv_obj_set_size(content, 392, 304);
    lv_obj_align(content, LV_ALIGN_LEFT_MID, rail_right ? 8 : 80, 0);
    lv_obj_set_style_pad_all(content, 10, 0);

    pages[0] = mk_obj(content);
    pages[1] = mk_obj(content);
    pages[2] = mk_obj(content);
    pages[3] = mk_obj(content);
    for (int i = 0; i < 4; i++) {
        lv_obj_set_size(pages[i], LV_PCT(100), LV_PCT(100));
        lv_obj_add_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    build_home(pages[0]);
    build_control(pages[1]);
    build_maintenance(pages[2]);
    build_settings(pages[3]);
    // navigation rail (left, floating pill)
    lv_obj_t* nav = mk_obj(screen);
    lv_obj_set_size(nav, 64, 304);
    lv_obj_align(nav, rail_right ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, rail_right ? -8 : 8, 0);
    lv_obj_set_style_bg_color(nav, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nav, 20, 0);
    lv_obj_set_style_pad_all(nav, 4, 0);
    lv_obj_set_style_pad_row(nav, 4, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char* icons[4] = {IC_HOME, IC_CONTROL, IC_WRENCH, IC_COG};
    const char* names[4] = {"首页", "控制", "维护", "设置"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = lv_button_create(nav);
        lv_obj_set_size(b, 56, 56);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_pad_all(b, 2, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, 0, 0);

        lv_obj_t* ic = mk_icon(b, icons[i], 24, theme::MUTED);
        lv_obj_t* lb = mk_label(b, names[i], 11, false, theme::MUTED);
        (void)ic;
        (void)lb;
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        nav_btns[i] = b;
    }

    // 叠在内容区上的喷嘴详情 / 材料选择面板
    build_tool_sheet(screen);
    build_picker(screen);
    build_color_page(screen);
    build_temp_page(screen);
    build_keypad(screen);
    build_nozzle_page(screen);
    build_print_sheet(screen);
    build_net_page(screen);
    build_feed_banner(screen);
    lv_timer_create([](lv_timer_t* t) { feed_icons_tick(t); }, 60, nullptr);

    // 轻提示（操作回退时用）
    lv_obj_t* toast = mk_obj(screen);
    lv_obj_set_size(toast, LV_SIZE_CONTENT, 34);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, rail_right ? -36 : 36, -18);
    lv_obj_set_style_bg_color(toast, theme::c(theme::SURFACE_HI), 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast, 999, 0);
    lv_obj_set_style_pad_hor(toast, 14, 0);
    lv_obj_set_style_border_width(toast, 1, 0);
    lv_obj_set_style_border_color(toast, theme::c(theme::OUTLINE), 0);
    lv_obj_set_style_shadow_width(toast, 14, 0);
    lv_obj_set_style_shadow_opa(toast, LV_OPA_40, 0);
    lv_obj_set_style_shadow_color(toast, theme::c(0x000000), 0);
    R.toast_label = mk_label(toast, "", 12, false, theme::TEXT);
    lv_obj_center(R.toast_label);
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    R.toast = toast;

    ui_show(0);
    style_nav(0);
}

void ui_scroll_active(int dy) {
    if (active_page < 0 || active_page > 3 || !pages[active_page]) return;
    lv_obj_scroll_by(pages[active_page], 0, -dy, LV_ANIM_OFF);
}

void ui_open_tool(int tool) {
    open_tool_sheet(tool);
}

void ui_open_picker() {
    if (g_sheet_tool < 0) g_sheet_tool = 0;
    sheet_to_picker_cb(nullptr);
}

void ui_open_temp() {
    open_temp_cb(nullptr);
}

void ui_open_bed_temp() {
    open_temp_page(1);
}

void ui_open_net() {
    open_net_page();
}

void ui_open_print() {
    open_print_sheet_cb(nullptr);
}

void ui_fan_page(int p) {
    fan_page_apply(p);
}

void ui_ctrl_tab(int i) {
    if (i < 0 || i > 2) return;
    g_ctrl_tab = i;
    for (int j = 0; j < 3; j++) {
        if (!R.ctrl_tab[j]) continue;
        if (j == i) lv_obj_remove_flag(R.ctrl_tab[j], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(R.ctrl_tab[j], LV_OBJ_FLAG_HIDDEN);
        style_chip(R.ctrl_chip[j], j == i);
    }
}

void ui_open_keypad(int slot) {
    const int* arr = (g_temp_kind == 1) ? g_bed_presets : g_presets;
    open_keypad(slot >= 0, slot,
                slot >= 0 ? arr[slot] : (g_temp_kind == 1 ? 60 : 200));
}

void ui_open_nozzle() {
    open_nozzle_cb(nullptr);
}

void ui_open_color() {
    open_color_page();
}

void ui_toast(const char* msg) {
    show_toast(msg);
}

void ui_show(int page) {
    if (page < 0 || page > 3) return;
    active_page = page;
    hide_sheets();
    for (int i = 0; i < 4; i++) {
        if (!pages[i]) continue;
        if (i == page) lv_obj_remove_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    style_nav(page);
    update_feed_banner_pos();
}

void ui_tick() {
    if (!MR) return;
    PrinterState s = MR->snapshot();

    if (WIFI) {
        if (lv_tick_get() - g_net_req_at > 3000) {
            g_net_req_at = lv_tick_get();
            WIFI->request_status();
        }
        WifiSnapshot w = WIFI->snapshot();
        if (R.net_line) {
            char buf[128];
            if (w.restarting) snprintf(buf, sizeof(buf), "Wi-Fi 重启中…");
            else if (!w.available) snprintf(buf, sizeof(buf), "Wi-Fi 服务未运行");
            else if (w.connected)
                snprintf(buf, sizeof(buf), "%s%s%s", w.ssid.c_str(),
                         w.ip.empty() ? "" : " · ", w.ip.c_str());
            else snprintf(buf, sizeof(buf), "未连接 · 点按管理");
            lv_label_set_text(R.net_line, buf);
        }
        if (!w.error.empty() && w.error != g_net_last_error) {
            notice_error("Wi-Fi 操作失败", w.error);
            g_net_last_error = w.error;
        } else if (w.error.empty()) {
            g_net_last_error.clear();
        }
        if (w.restarting && !g_wifi_restart_seen) {
            g_wifi_restart_seen = true;
            notice_show(false, false, IC_WIFI, "正在重启 Wi-Fi 服务…", "请稍候", 0);
        } else if (!w.restarting && g_wifi_restart_seen) {
            g_wifi_restart_seen = false;
            if (w.available)
                notice_show(false, true, IC_CHECK_CIRCLE, "Wi-Fi 已恢复",
                            w.ssid.empty() ? "服务已重新启动" : w.ssid, 3000);
            else
                notice_error("Wi-Fi 恢复失败", "请检查路由器或重启打印机");
        }
        if (g_net_open) net_refresh_ui(w);
    }

    if (R.toast && g_toast_until && lv_tick_get() > g_toast_until) {
        lv_obj_add_flag(R.toast, LV_OBJ_FLAG_HIDDEN);
        g_toast_until = 0;
    }

    // connection chip
    if (s.connected) {
        if (R.conn_state)
            lv_label_set_text(R.conn_state, MR->config().read_only ? "只读" : "已连接");
        if (R.conn_state) lv_obj_set_style_text_color(R.conn_state, theme::c(theme::SUCCESS), 0);
    } else {
        if (R.conn_state) lv_label_set_text(R.conn_state, "未连接");
        if (R.conn_state) lv_obj_set_style_text_color(R.conn_state, theme::c(theme::DANGER), 0);
    }

    // status card
    bool printing = (s.print_state == "printing");
    bool paused = (s.print_state == "paused");
    set_disabled(R.home_pause_btn, !printing);
    set_disabled(R.home_resume_btn, !paused);
    set_disabled(R.home_stop_btn, !(printing || paused));
    const char* state_txt = printing ? "打印中" : (paused ? "已暂停" : "待机");

    if (R.status_title) lv_label_set_text(R.status_title, state_txt);
    if (R.status_sub) {
        if (printing || paused) {
            std::string name = short_name(s.filename);
            lv_label_set_text(R.status_sub, name.c_str());
        } else {
            lv_label_set_text(R.status_sub, s.connected ? "准备就绪" : "检查 Moonraker 连接");
        }
    }
    if (R.status_sub2) {
        char buf[64];
        if (printing || paused) {
            snprintf(buf, sizeof(buf), "第 %d / %d 层 · 已用 %s", s.layer, s.layers,
                     fmt_time(s.print_duration).c_str());
            lv_label_set_text(R.status_sub2, buf);
        } else {
            lv_label_set_text(R.status_sub2, "");
        }
    }
    if (R.status_arc) {
        lv_arc_set_value(R.status_arc, (int)(s.progress * 1000));
        lv_obj_set_style_arc_color(R.status_arc,
                                   theme::c(printing ? theme::SUCCESS : theme::PRIMARY),
                                   LV_PART_INDICATOR);
    }
    if (R.status_pct) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)(s.progress * 100));
        lv_label_set_text(R.status_pct, printing || paused ? buf : "--");
    }
    if (R.status_card) {
        lv_obj_set_style_bg_color(
            R.status_card, theme::c(printing ? theme::SUCCESS_DIM : theme::SURFACE_HI), 0);
    }

    // temps：每个喷嘴一张卡片（温度 / 颜色 / 类型 / NFC）
    for (int i = 0; i < 4; i++) {
        const FilamentInfo& f = s.filaments[i];
        if (R.nozzle_temp[i]) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f°", s.tools[i].temp);
            lv_label_set_text(R.nozzle_temp[i], buf);
            uint32_t col = theme::TEXT;
            if (s.tools[i].target > 0.5 && s.tools[i].target > s.tools[i].temp + 2) col = theme::WARN;
            else if (s.tools[i].target > 0.5) col = theme::SUCCESS;
            lv_obj_set_style_text_color(R.nozzle_temp[i], theme::c(col), 0);
        }
        if (R.nozzle_target[i]) {
            char buf[24];
            if (s.tools[i].target > 0.5) {
                snprintf(buf, sizeof(buf), "目标 %.0f°", s.tools[i].target);
                lv_obj_set_style_text_color(
                    R.nozzle_target[i],
                    theme::c(s.tools[i].temp + 2 >= s.tools[i].target ? theme::SUCCESS : theme::WARN),
                    0);
            } else {
                snprintf(buf, sizeof(buf), "未加热");
                lv_obj_set_style_text_color(R.nozzle_target[i], theme::c(theme::MUTED), 0);
            }
            lv_label_set_text(R.nozzle_target[i], buf);
        }
        set_swatch(R.nozzle_swatch[i], f);
        if (R.nozzle_mat[i]) {
            std::string name;
            if (f.present) {
                name = f.material;
                std::string sub = sub_text(f.sub);
                if (!sub.empty()) name += " " + sub;
            } else {
                name = "未识别";
            }
            lv_label_set_text(R.nozzle_mat[i], name.c_str());
        }
        if (R.nozzle_nfc[i]) {
            if (f.nfc) lv_obj_remove_flag(R.nozzle_nfc[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(R.nozzle_nfc[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (R.temp_bed) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f°C", s.bed_temp);
        lv_label_set_text(R.temp_bed, buf);
        uint32_t col = theme::TEXT;
        if (s.bed_target > 0.5 && s.bed_target > s.bed_temp + 2) col = theme::WARN;
        else if (s.bed_target > 0.5) col = theme::SUCCESS;
        lv_obj_set_style_text_color(R.temp_bed, theme::c(col), 0);
    }
    if (R.bed_target) {
        char buf[24];
        if (s.bed_target > 0.5) {
            snprintf(buf, sizeof(buf), "目标 %.0f°", s.bed_target);
            lv_obj_set_style_text_color(R.bed_target, theme::c(theme::WARN), 0);
        } else {
            snprintf(buf, sizeof(buf), "未加热");
            lv_obj_set_style_text_color(R.bed_target, theme::c(theme::MUTED), 0);
        }
        lv_label_set_text(R.bed_target, buf);
    }
    bool light = s.light_on;
    if (g_light_pending) {
        if (s.light_on == g_light_val) {
            g_light_pending = false;
        } else if (lv_tick_get() - g_light_at > 8000) {
            g_light_pending = false;
            notice_error("灯光状态未确认", "已恢复");
        } else {
            light = g_light_val;
        }
    }
    apply_light_ui(light);
    update_feed_banner_pos();
    if (g_temp_pending) {
        double actual = g_temp_pending_kind == 1
                            ? s.bed_target
                            : (g_temp_pending_tool >= 0 ? s.tools[g_temp_pending_tool].target : 0);
        if (fabs(actual - (double)g_temp_pending_val) < 1.0) {
            g_temp_pending = false;
        } else if (lv_tick_get() - g_temp_pending_at > 8000) {
            g_temp_pending = false;
            notice_error("温度设置未生效", "已恢复目标温度");
        }
    }
    const double fan_state[3] = {s.fan_speed, s.fan_aux_speed, s.fan_circ_speed};
    for (int fi = 0; fi < 3; fi++) {
        int actual = (int)lround(fan_state[fi] * 100.0);
        if (g_fan_pending[fi]) {
            if (abs(actual - g_fan_val[fi]) <= 1) {
                g_fan_pending[fi] = false;
            } else if (lv_tick_get() - g_fan_at[fi] > 8000) {
                g_fan_pending[fi] = false;
                notice_error("风扇设置未生效", std::string(kFanName[fi]) + " 已恢复实际转速");
            }
        }
        int show = g_fan_pending[fi] ? g_fan_val[fi] : actual;
        fan_set_label(fi, show);
        if (R.ctrl_fan_icon[fi])
            lv_obj_set_style_text_color(
                R.ctrl_fan_icon[fi],
                theme::c(show > 0 ? theme::PRIMARY : theme::MUTED), 0);
        if (R.ctrl_fan_slider[fi] && !g_fan_dragging[fi] && !g_fan_pending[fi] &&
            (int)lv_slider_get_value(R.ctrl_fan_slider[fi]) != actual)
            lv_slider_set_value(R.ctrl_fan_slider[fi], actual, LV_ANIM_OFF);
    }

    std::string cmd_err = MR->take_command_error();
    if (!cmd_err.empty()) {
        if (g_home_pending) g_home_pending = false;
        notice_error("命令失败", cmd_err);
    }
    {
        const std::string& h = s.homed_axes;
        bool homed = h.find('x') != std::string::npos &&
                     h.find('y') != std::string::npos &&
                     h.find('z') != std::string::npos;
        if (g_home_pending) {
            bool hok = false;
            std::string herr;
            if (MR && MR->take_homing_result(hok, herr)) {
                g_home_pending = false;
                if (hok) {
                    notice_show(false, true, IC_CHECK_CIRCLE, "回零完成", "", 2500);
                } else {
                    notice_error("回零失败", herr.empty() ? "请检查打印机" : herr);
                }
            } else if (lv_tick_get() - g_home_at > 300000) {
                g_home_pending = false;
                notice_error("回零超时", "请检查打印机");
            }
        }
        for (int i = 0; i < 6; i++)
            set_disabled(R.move_btn[i], !homed || g_home_pending);
        if (R.move_hint) {
            const char* txt = g_home_pending ? "回零中…"
                                            : (homed ? "已回零" : "未回零：请先点回零");
            lv_label_set_text(R.move_hint, txt);
            lv_obj_set_style_text_color(
                R.move_hint,
                theme::c(g_home_pending ? theme::WARN : (homed ? theme::SUCCESS : theme::MUTED)),
                0);
        }
    }
    for (int i = 0; i < 4; i++) {
        const FeedChannel& fc = s.feed[i];
        int fm = 0;
        if (!fc.error.empty() && fc.error != "ok") fm = 4;
        else if (fc.state.find("fail") != std::string::npos) fm = 4;
        else if (fc.state == "load_finish") fm = 1;
        else if (fc.state.rfind("load", 0) == 0 && feed_state_active(fc.state)) fm = 2;
        else if (fc.state.rfind("unload", 0) == 0 && feed_state_active(fc.state)) fm = 3;
        if (fm != g_nozzle_feed_mode[i]) {
            g_nozzle_feed_mode[i] = fm;
            feed_icon_apply(i, fm);
        }
    }
    feed_tick(s);
    update_sheet();
    update_temp_page();
    update_nozzle_page();

    // print page
    if (R.print_arc) lv_arc_set_value(R.print_arc, (int)(s.progress * 1000));
    if (R.print_pct) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)(s.progress * 100));
        lv_label_set_text(R.print_pct, buf);
    }
    if (R.print_file) {
        std::string name = s.filename.empty() ? "没有正在打印的文件" : short_name(s.filename, 26);
        lv_label_set_text(R.print_file, name.c_str());
    }
    if (R.print_meta) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%s · 已用 %s · 耗材 %.0f mm",
                 printing ? "打印中" : (paused ? "已暂停" : "待机"),
                 fmt_time(s.print_duration).c_str(), s.filament_used);
        lv_label_set_text(R.print_meta, buf);
    }

    set_sw(R.pref_replenish_sw, s.pref_auto_replenish);
    set_sw(R.pref_ignore_color_sw, s.pref_replenish_ignore_color);
    set_sw(R.pref_end_led_sw, s.pref_end_led_off);

    // print tuning
    if (R.print_temps) {
        char buf[96];
        snprintf(buf, sizeof(buf), "喷嘴 %.0f/%.0f/%.0f/%.0f°C · 热床 %.0f°C", s.tools[0].temp,
                 s.tools[1].temp, s.tools[2].temp, s.tools[3].temp, s.bed_temp);
        lv_label_set_text(R.print_temps, buf);
    }
    if (R.speed_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)lround(s.speed_factor * 100));
        lv_label_set_text(R.speed_label, buf);
    }
    if (R.zoff_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%+.2f", s.z_offset);
        lv_label_set_text(R.zoff_label, buf);
    }

    // maintenance
    if (R.pur_hours) {
        char buf[48];
        snprintf(buf, sizeof(buf), "滤芯 %s", fmt_hours(s.filter_hours).c_str());
        lv_label_set_text(R.pur_hours, buf);
    }
    if (R.filter_bar) {
        int tenths = (int)(s.filter_hours * 10);
        if (tenths > 2000) tenths = 2000;
        lv_bar_set_value(R.filter_bar, tenths, LV_ANIM_OFF);
        uint32_t col = s.filter_hours >= 180 ? theme::DANGER
                       : s.filter_hours >= 150 ? theme::WARN
                                               : theme::PRIMARY;
        lv_obj_set_style_bg_color(R.filter_bar, theme::c(col), LV_PART_INDICATOR);
    }
    if (R.pur_rpm) {
        char buf[64];
        snprintf(buf, sizeof(buf), "转速 %.0f rpm%s", s.purifier_rpm,
                 s.purifier_present ? "" : " · 未检测到");
        lv_label_set_text(R.pur_rpm, buf);
    }
    if (R.pur_speed) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%%", s.purifier_inner_speed);
        lv_label_set_text(R.pur_speed, buf);
    }
    if (R.pur_mode_line) {
        static const char* mode_names[] = {"待机", "降温", "预热", "保温"};
        int m = s.purifier_mode;
        char buf[32];
        if (!s.purifier_present) snprintf(buf, sizeof(buf), "未检测到");
        else snprintf(buf, sizeof(buf), "当前 %s", mode_names[(m >= 0 && m <= 3) ? m : 0]);
        lv_label_set_text(R.pur_mode_line, buf);
    }
    for (int i = 0; i < 4; i++) style_chip(R.pur_mode_btn[i], s.purifier_present && s.purifier_mode == i);
    // 没检测到净化器时把控件压暗，避免误以为可点
    lv_opa_t pur_opa = s.purifier_present ? LV_OPA_COVER : LV_OPA_40;
    for (int i = 0; i < 4; i++) lv_obj_set_style_opa(R.pur_mode_btn[i], pur_opa, 0);
    if (R.pur_speed_dec) lv_obj_set_style_opa(R.pur_speed_dec, pur_opa, 0);
    if (R.pur_speed_inc) lv_obj_set_style_opa(R.pur_speed_inc, pur_opa, 0);

    if (R.head_line) {
        char buf[96];
        snprintf(buf, sizeof(buf), "换头次数  %d / %d / %d / %d", s.switch_count[0],
                 s.switch_count[1], s.switch_count[2], s.switch_count[3]);
        lv_label_set_text(R.head_line, buf);
    }
    if (R.head_nozzle_line) {
        char buf[128];
        auto nt = [&](int i) { return s.nozzle_type[i] == "high_flow" ? "高流" : "标准"; };
        snprintf(buf, sizeof(buf), "喷嘴 %.1f %s · %.1f %s · %.1f %s · %.1f %s",
                 s.nozzle_diameter[0], nt(0), s.nozzle_diameter[1], nt(1),
                 s.nozzle_diameter[2], nt(2), s.nozzle_diameter[3], nt(3));
        lv_label_set_text(R.head_nozzle_line, buf);
    }

    // 空打检测 / 异物检测
    set_sw(R.ent_sw, s.pref_entangle_detect);
    int es = s.pref_entangle_sens == "high" ? 2 : (s.pref_entangle_sens == "low" ? 0 : 1);
    set_chip_row(R.ent_seg, 3, es);

    set_sw(R.def_sw, s.defect_main);
    set_sw(R.def_group_sw[0], s.defect_clean_bed);
    set_sw(R.def_group_sw[1], s.defect_noodle);
    set_sw(R.def_group_sw[2], s.defect_residue);
    set_sw(R.def_group_sw[3], s.defect_nozzle);
    set_chip_row(R.def_seg, 2, s.defect_sens == "high" ? 1 : 0);

    // 打印偏好
    set_sw(R.pref_bedlevel_sw, s.pref_auto_bed_leveling);
    set_sw(R.pref_flow_sw, s.pref_flow_calibrate);
    set_sw(R.pref_shaper_sw, s.pref_shaper_calibrate);
    set_sw(R.pref_timelapse_sw, s.pref_time_lapse);

    // settings: device + stats
    if (R.dev_line) {
        char buf[160];
        snprintf(buf, sizeof(buf), "%s · SN %s", s.machine_type.empty() ? "—" : s.machine_type.c_str(),
                 s.serial.empty() ? "—" : s.serial.c_str());
        lv_label_set_text(R.dev_line, buf);
    }
    if (R.dev_nozzle) {
        char buf[120];
        snprintf(buf, sizeof(buf), "固件 %s · 喷嘴 %.1f/%.1f/%.1f/%.1f",
                 s.fw_version.empty() ? "—" : s.fw_version.c_str(), s.nozzle_diameter[0],
                 s.nozzle_diameter[1], s.nozzle_diameter[2], s.nozzle_diameter[3]);
        lv_label_set_text(R.dev_nozzle, buf);
    }
    if (R.gui_line) {
        char buf[96];
        snprintf(buf, sizeof(buf), "原厂 GUI 偏好 · 自动映射 %s",
                 s.gui_auto_color_mapping ? "开" : "关");
        lv_label_set_text(R.gui_line, buf);
    }
    if (R.stat_line) {
        char buf[160];
        snprintf(buf, sizeof(buf), "累计打印 %.1f h · 任务 %d · 耗材 %.0f m",
                 s.total_print_time / 3600.0, s.total_jobs, s.total_filament / 1000.0);
        lv_label_set_text(R.stat_line, buf);
    }
    if (R.conn_line) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s:%d · %s%s", MR->config().host.c_str(),
                 MR->config().port, s.connected ? "已连接" : "未连接",
                 MR->config().read_only ? " · 只读" : "");
        lv_label_set_text(R.conn_line, buf);
    }
}

#pragma once

#include <lvgl.h>

class Moonraker;
class Wifi;

void ui_init(Moonraker* mr, lv_obj_t* screen, Wifi* wifi = nullptr);
void ui_tick();
void ui_show(int page);
void ui_scroll_active(int dy);  // 截图/调试用：滚动当前页
void ui_open_tool(int tool);    // 截图/调试用：打开喷嘴详情
void ui_open_picker();          // 截图/调试用：打开材料选择
void ui_open_temp();            // 截图/调试用：打开半屏调温页（喷嘴）
void ui_open_bed_temp();        // 截图/调试用：打开半屏调温页（热床）
void ui_open_net();             // 截图/调试用：打开网络面板
void ui_open_print();           // 截图/调试用：打开打印控制浮层
void ui_ctrl_tab(int i);        // 截图/调试用：切换控制页子标签
void ui_open_keypad(int slot);  // 截图/调试用：打开数字键盘（slot>=0 编辑快捷温度）
void ui_open_nozzle();          // 截图/调试用：打开喷嘴参数弹层
void ui_open_color();           // 截图/调试用：打开颜色选择面板
void ui_toast(const char* msg); // 截图/调试用：显示提示条

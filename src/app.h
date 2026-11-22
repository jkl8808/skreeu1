#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ToolState {
    double temp = 0;
    double target = 0;
};

struct FilamentInfo {
    bool detected = false;   // motion sensor sees filament
    bool sensor_present = false;
    bool present = false;    // RFID / spool data present
    std::string vendor;
    std::string material;
    std::string sub;
    uint32_t argb = 0xFFFFFF;
    bool official = false;

    // NFC / RFID 卡片读到的扩展信息（filament_detect.info）
    bool nfc = false;        // 读到卡片（CARD_TYPE 非空）
    std::string card_type;   // M1 …
    int hotend_min = 0;      // 建议喷头温度下限
    int hotend_max = 0;      // 建议喷头温度上限
    int bed_temp = 0;        // 建议热床温度
    double diameter = 0;     // mm
    int weight = 0;          // g
};

// 打印机上的 G-code 文件（/server/files/list）
struct GcodeFile {
    std::string name;      // 相对 gcodes 的路径
    double modified = 0;   // unix 时间
    double size = 0;       // 字节
};

// G-code 元数据里的一个虚拟耗材位（T0..T3）
struct GcodeFilament {
    std::string type;          // PLA / PETG / NONE
    std::string name;          // Snapmaker PLA SnapSpeed @U1
    uint32_t argb = 0xFFFFFF;
    double weight_g = 0;
    double used_mm = 0;
    double nozzle_diameter = 0.4;
    int nozzle_temp = 0;
    std::string volume_type = "standard";
    bool used = false;
};

// /server/files/metadata 的常用字段，raw 保留完整 JSON 用于开始打印
struct GcodeMeta {
    bool ok = false;
    std::string filename;
    std::string slicer;
    std::string slicer_version;
    double estimated_time = 0;     // 秒
    int layer_count = 0;
    double layer_height = 0;
    double object_height = 0;
    double nozzle_diameter = 0;
    double filament_total = 0;     // mm
    double filament_weight_total = 0;  // g
    int filament_count = 0;
    GcodeFilament filaments[4];
    std::string thumb_path;        // .thumbs/xxx-96x96.png
    std::string raw;               // metadata result 原始 JSON
};

// 可选耗材，来自 printer_data/config/snapmaker/filament_parameters.json
struct MaterialOption {
    std::string type;    // PLA
    std::string sub;     // SnapSpeed（generic 表示无子类型）
    std::string vendor;  // Snapmaker / generic
    bool soft = false;
    bool custom = false; // 来自 skreeu1_types.cfg 的自定义类型
};

// Snapmaker 自动进料模块（Klipper `filament_feed left/right`）的一个通道
struct FeedChannel {
    bool module = false;     // 模块在线
    bool detected = false;   // 通道检测到耗材
    bool auto_mode = true;   // !disable_auto
    std::string state;       // channel_state（load_finish / preload_finish / none ...）
    std::string error;       // channel_error（ok / ...）
};

struct PrinterState {
    bool connected = false;
    std::string klippy = "connecting";
    std::string print_state = "standby";
    std::string filename;
    double progress = 0;          // 0..1
    double print_duration = 0;    // s
    double filament_used = 0;     // mm
    int layer = 0;
    int layers = 0;

    ToolState tools[4];
    double bed_temp = 0;
    double bed_target = 0;
    double fan_speed = 0;       // 模型冷却
    double fan_aux_speed = 0;   // 辅助（cavity_fan）
    double fan_circ_speed = 0;  // 循环（circulation_fan）
    std::string homed_axes;     // toolhead.homed_axes，如 "xyz"
    bool light_on = false;   // led cavity_led

    // purifier (Snapmaker U1)
    bool purifier_present = false;
    double filter_hours = 0;
    double purifier_rpm = 0;
    int purifier_mode = 0;

    // defect detection (Snapmaker U1)
    bool defect_main = false;
    bool defect_clean_bed = false;
    bool defect_noodle = false;
    bool defect_residue = false;
    bool defect_nozzle = false;
    std::string defect_sens = "low";
    double purifier_inner_speed = 0;    // 0..100 %
    double purifier_exhaust_speed = 0;  // 0..100 %
    int purifier_delay = 180;           // minutes
    int purifier_work = 0;              // minutes

    FilamentInfo filaments[4];
    double speed_factor = 1.0;
    double z_offset = 0.0;

    // Snapmaker Klipper 扩展对象
    int switch_count[4] = {0, 0, 0, 0};                 // extruder*.switch_count
    std::string nozzle_type[4] = {"standard", "standard", "standard", "standard"};
    FeedChannel feed[4];

    // 可选耗材目录（filament_parameters.json，启动时拉一次）
    std::vector<MaterialOption> materials;

    // 打印偏好（Klipper `print_task_config`，原厂 GUI 同一份数据）
    bool pref_auto_bed_leveling = false;
    bool pref_flow_calibrate = false;
    bool pref_shaper_calibrate = false;
    bool pref_time_lapse = false;
    bool pref_auto_replenish = false;
    bool pref_replenish_ignore_color = false;
    bool pref_entangle_detect = false;
    std::string pref_entangle_sens = "medium";
    bool pref_end_led_off = false;

    // GUI 自己的偏好（gui_config.json，非 Klipper）
    bool gui_auto_color_mapping = true;

    // device info (product_info.json)
    std::string device_name;
    std::string machine_type;
    std::string serial;
    std::string fw_version;
    double nozzle_diameter[4] = {0, 0, 0, 0};

    // run statistics (Moonraker history totals)
    double total_print_time = 0;
    int total_jobs = 0;
    double total_filament = 0;

    std::string message;
};

struct Config {
#ifdef SKREEU1_DEVICE
    // 设备上由 lmd 直接拉起，没有命令行参数，所以默认值要能直接连本机
    std::string host = "127.0.0.1";
    std::string font_dir = "/userdata/skreeu1/assets/fonts";
#else
    std::string host = "127.0.0.1";
    std::string font_dir = "assets/fonts";
#endif
    int port = 7125;
    bool read_only = false;
    std::string product_info = "/oem/printer_data/config/snapmaker/product_info.json";
};

class Moonraker {
public:
    explicit Moonraker(Config cfg);

    void start();
    void stop();

    PrinterState snapshot();
    bool send_gcode(const std::string& script);
    bool send_homing();

    // 打印文件：列表 / 元数据 / 带耗材映射开始打印
    std::vector<GcodeFile> list_gcode_files();
    bool gcode_metadata(const std::string& filename, GcodeMeta& out);
    bool start_print(const GcodeMeta& meta, const int map[4]);
    bool fetch_thumb(const std::string& rel_path, std::string& out,
                     long* status_out = nullptr);
    bool take_homing_result(bool& ok, std::string& error);
    std::string take_command_error();
    const Config& config() const { return cfg_; }

private:
    void loop();
    void poll_once();
    void poll_history();
    void poll_gui_config();
    void load_product_info();
    void load_materials();
    void send_loop();

    Config cfg_;
    std::thread cmd_thread_;
    std::mutex cmd_mu_;
    std::condition_variable cmd_cv_;
    struct CmdItem {
        std::string script;
        bool homing = false;
    };
    std::deque<CmdItem> cmd_queue_;
    bool cmd_quit_ = false;
    std::mutex cmd_err_mu_;
    std::string cmd_error_msg_;
    std::mutex home_mu_;
    bool home_done_ = false;
    bool home_ok_ = false;
    std::string home_error_;
    std::mutex mu_;
    PrinterState st_;
    bool running_ = false;
};

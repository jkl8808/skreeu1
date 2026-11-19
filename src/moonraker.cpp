#include "moonraker.h"

#include "hv/json.hpp"

#include <curl/curl.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

using json = nlohmann::json;

namespace {

size_t write_cb(void* data, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(data), size * nmemb);
    return size * nmemb;
}

bool http_get(const std::string& url, std::string& body, long timeout_s = 4) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK && status >= 200 && status < 300;
}

bool http_post(const std::string& url, std::string& body, long timeout_s = 5) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return rc == CURLE_OK && status >= 200 && status < 300;
}

double dget(const json& j, const char* k, double def = 0.0) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_number()) return def;
    return it->get<double>();
}

bool bget(const json& j, const char* k, bool def = false) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_boolean()) return def;
    return it->get<bool>();
}

} // namespace

Moonraker::Moonraker(Config cfg) : cfg_(std::move(cfg)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    load_product_info();
    load_materials();
}

void Moonraker::load_product_info() {
    json j;
    // 首选 Moonraker /machine/system_info：切片软件（Snapmaker Orca）读的也是这里。
    // 好处是 Docker 连远端打印机时同样能拿到真实喷嘴信息。
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/machine/system_info";
    if (http_get(url, body)) {
        try {
            j = json::parse(body).at("result").at("system_info").at("product_info");
        } catch (...) {
            j = json();
        }
    }
    // 回退：本地 product_info.json（离线/无 Moonraker 时）
    if (!j.is_object() || j.empty()) {
        FILE* f = fopen(cfg_.product_info.c_str(), "r");
        if (!f) return;
        std::string local;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) local.append(buf, n);
        fclose(f);
        try {
            j = json::parse(local);
        } catch (...) {
            return;
        }
    }
    try {
        std::lock_guard<std::mutex> lock(mu_);
        if (j.contains("device_name")) st_.device_name = j.value("device_name", st_.device_name);
        if (j.contains("machine_type")) st_.machine_type = j.value("machine_type", st_.machine_type);
        if (j.contains("serial_number")) st_.serial = j.value("serial_number", st_.serial);
        if (j.contains("firmware_version"))
            st_.fw_version = j.value("firmware_version", st_.fw_version);
        if (j.contains("nozzle_diameter") && j["nozzle_diameter"].is_array()) {
            for (int i = 0; i < 4 && i < (int)j["nozzle_diameter"].size(); i++)
                st_.nozzle_diameter[i] = j["nozzle_diameter"][i].get<double>();
        }
    } catch (...) {
    }
}

void Moonraker::load_materials() {
    // 原厂材料库：类型 -> vendor_* -> sub_*，用来做"更换耗材"列表。
    // 文件在 printer_data/config/snapmaker 下，可通过 Moonraker 的 files API 取。
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/files/config/snapmaker/filament_parameters.json";
    if (!http_get(url, body, 8)) return;
    try {
        json j = json::parse(body);
        std::vector<MaterialOption> list;
        for (auto t = j.begin(); t != j.end(); ++t) {
            if (!t->is_object()) continue;  // version / 数值项
            for (auto v = t->begin(); v != t->end(); ++v) {
                if (!v->is_object()) continue;
                std::string vendor = v.key();
                if (vendor.rfind("vendor_", 0) == 0) vendor = vendor.substr(7);
                for (auto s = v->begin(); s != v->end(); ++s) {
                    if (!s->is_object()) continue;
                    std::string sub = s.key();
                    if (sub.rfind("sub_", 0) == 0) sub = sub.substr(4);
                    MaterialOption o;
                    o.type = t.key();
                    o.vendor = vendor;
                    o.sub = sub;
                    o.soft = bget(*s, "is_soft");
                    list.push_back(std::move(o));
                }
            }
        }
        std::lock_guard<std::mutex> lock(mu_);
        st_.materials = std::move(list);
    } catch (...) {
    }
}

void Moonraker::start() {
    running_ = true;
    std::thread([this] { loop(); }).detach();
}

void Moonraker::stop() {
    running_ = false;
}

PrinterState Moonraker::snapshot() {
    std::lock_guard<std::mutex> lock(mu_);
    return st_;
}

bool Moonraker::send_gcode(const std::string& script) {
    if (cfg_.read_only) return false;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    char* esc = curl_easy_escape(curl, script.c_str(), (int)script.size());
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/printer/gcode/script?script=" + (esc ? esc : "");
    curl_free(esc);
    std::string body;
    bool ok = http_post(url, body);
    curl_easy_cleanup(curl);
    return ok;
}

void Moonraker::poll_history() {
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/history/totals";
    if (!http_get(url, body)) return;
    try {
        json j = json::parse(body);
        const json& t = j.at("result").at("job_totals");
        std::lock_guard<std::mutex> lock(mu_);
        st_.total_print_time = dget(t, "total_print_time");
        st_.total_jobs = (int)dget(t, "total_jobs");
        st_.total_filament = dget(t, "total_filament_used");
    } catch (...) {
    }
}

void Moonraker::poll_gui_config() {
    // gui_config.json 是原厂 GUI 自己的偏好文件（不属于 Klipper）。
    // 只读取需要的字段；写入仍由原厂 GUI / 我们后续的设置页决定。
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/files/config/gui_config.json";
    if (!http_get(url, body)) return;
    try {
        json j = json::parse(body);
        if (j.contains("system") && j["system"].is_object()) {
            const json& sys = j["system"];
            bool mapping = true;
            auto it = sys.find("auto_color_mapping");
            if (it != sys.end() && it->is_number()) mapping = it->get<int>() != 0;
            std::lock_guard<std::mutex> lock(mu_);
            st_.gui_auto_color_mapping = mapping;
        }
    } catch (...) {
    }
}

void Moonraker::poll_once() {
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/printer/objects/query?print_stats&virtual_sdcard&display_status"
                      "&toolhead&extruder&extruder1&extruder2&extruder3&heater_bed"
                      "&purifier&defect_detection&gcode_move&filament_detect"
                      "&print_task_config"
                      "&filament_feed%20left&filament_feed%20right"
                      "&filament_motion_sensor%20e0_filament"
                      "&filament_motion_sensor%20e1_filament"
                      "&filament_motion_sensor%20e2_filament"
                      "&filament_motion_sensor%20e3_filament"
                      "&led%20cavity_led";

    PrinterState next;
    if (!http_get(url, body)) {
        std::lock_guard<std::mutex> lock(mu_);
        st_.connected = false;
        st_.klippy = "offline";
        st_.message = "无法连接 " + cfg_.host + ":" + std::to_string(cfg_.port);
        return;
    }

    try {
        json j = json::parse(body);
        const json& st = j.at("result").at("status");

        next.connected = true;
        next.klippy = "ready";

        if (st.contains("print_stats")) {
            const json& ps = st["print_stats"];
            next.print_state = ps.value("state", "standby");
            next.filename = ps.value("filename", "");
            next.print_duration = dget(ps, "print_duration");
            next.filament_used = dget(ps, "filament_used");
            if (ps.contains("info") && ps["info"].is_object()) {
                next.layer = (int)dget(ps["info"], "current_layer");
                next.layers = (int)dget(ps["info"], "total_layer");
            }
        }
        if (st.contains("display_status")) {
            next.progress = dget(st["display_status"], "progress");
        }

        const char* ex[] = {"extruder", "extruder1", "extruder2", "extruder3"};
        for (int i = 0; i < 4; i++) {
            if (st.contains(ex[i]) && st[ex[i]].is_object()) {
                const json& e = st[ex[i]];
                next.tools[i].temp = dget(e, "temperature");
                next.tools[i].target = dget(e, "target");
                next.switch_count[i] = (int)dget(e, "switch_count");
                double nd = dget(e, "nozzle_diameter");
                if (nd > 0) next.nozzle_diameter[i] = nd;
                if (e.contains("nozzle_volume_type") && e["nozzle_volume_type"].is_string())
                    next.nozzle_type[i] = e["nozzle_volume_type"].get<std::string>();
            }
        }
        if (st.contains("heater_bed")) {
            next.bed_temp = dget(st["heater_bed"], "temperature");
            next.bed_target = dget(st["heater_bed"], "target");
        }

        if (st.contains("purifier") && st["purifier"].is_object()) {
            const json& p = st["purifier"];
            next.purifier_present = bget(p, "power_detected");
            next.filter_hours = dget(p, "inner_fan_work_time") / 3600.0;
            next.purifier_rpm = dget(p, "inner_fan_rpm");
            next.purifier_mode = (int)dget(p, "mode");
            if (p.contains("inner_fan") && p["inner_fan"].is_object()) {
                const json& f = p["inner_fan"];
                next.purifier_inner_speed = dget(f, "speed") * 100.0;
                next.purifier_delay = (int)dget(f, "delay", 180);
            }
            if (p.contains("exhaust_fan") && p["exhaust_fan"].is_object())
                next.purifier_exhaust_speed = dget(p["exhaust_fan"], "speed") * 100.0;
        }

        if (st.contains("defect_detection") && st["defect_detection"].is_object()) {
            const json& d = st["defect_detection"];
            next.defect_main = bget(d, "main_enable");
            auto group_on = [&](const char* k) {
                return d.contains(k) && d[k].is_object() && bget(d[k], "enable");
            };
            next.defect_clean_bed = group_on("clean_bed");
            next.defect_noodle = group_on("noodle");
            next.defect_residue = group_on("residue");
            next.defect_nozzle = group_on("nozzle");
            if (d.contains("clean_bed") && d["clean_bed"].is_object())
                next.defect_sens = d["clean_bed"].value("sensitivity", "low");
        }

        if (st.contains("led cavity_led") && st["led cavity_led"].is_object()) {
            const json& l = st["led cavity_led"];
            if (l.contains("color_data") && l["color_data"].is_array() &&
                !l["color_data"].empty() && l["color_data"][0].is_array()) {
                for (const auto& c : l["color_data"][0]) {
                    if (c.is_number() && c.get<double>() > 0.01) {
                        next.light_on = true;
                        break;
                    }
                }
            }
        }

        if (st.contains("gcode_move") && st["gcode_move"].is_object()) {
            const json& g = st["gcode_move"];
            next.speed_factor = dget(g, "speed_factor", 1.0);
            if (g.contains("homing_origin") && g["homing_origin"].is_array() &&
                g["homing_origin"].size() >= 3) {
                next.z_offset = g["homing_origin"][2].get<double>();
            }
        }

        if (st.contains("filament_detect") && st["filament_detect"].is_object()) {
            const json& fd = st["filament_detect"];
            if (fd.contains("info") && fd["info"].is_array()) {
                const json& arr = fd["info"];
                for (int i = 0; i < 4 && i < (int)arr.size(); i++) {
                    const json& f = arr[i];
                    if (!f.is_object()) continue;
                    auto& out = next.filaments[i];
                    std::string type = f.value("MAIN_TYPE", "NONE");
                    std::string sub = f.value("SUB_TYPE", "NONE");
                    std::string vendor = f.value("VENDOR", "NONE");
                    if (vendor == "NONE") vendor = f.value("MANUFACTURER", "NONE");
                    out.present = (type != "NONE" && !type.empty());
                    out.material = type;
                    out.sub = (sub == "NONE") ? "" : sub;
                    out.vendor = (vendor == "NONE") ? "" : vendor;
                    out.argb = (uint32_t)f.value("ARGB_COLOR", 0xFFFFFF);
                    out.official = f.value("OFFICIAL", false);
                    out.card_type = f.value("CARD_TYPE", std::string());
                    out.nfc = !out.card_type.empty();
                    out.hotend_min = (int)dget(f, "HOTEND_MIN_TEMP");
                    out.hotend_max = (int)dget(f, "HOTEND_MAX_TEMP");
                    out.bed_temp = (int)dget(f, "BED_TEMP");
                    out.diameter = dget(f, "DIAMETER") / 100.0;
                    out.weight = (int)dget(f, "WEIGHT");
                }
            }
        }

        const char* sensors[] = {"filament_motion_sensor e0_filament",
                                 "filament_motion_sensor e1_filament",
                                 "filament_motion_sensor e2_filament",
                                 "filament_motion_sensor e3_filament"};
        for (int i = 0; i < 4; i++) {
            // Klipper 对不存在的对象也会返回 {}，必须检查字段本身
            if (st.contains(sensors[i]) && st[sensors[i]].is_object() &&
                st[sensors[i]].contains("filament_detected")) {
                next.filaments[i].sensor_present = true;
                next.filaments[i].detected = bget(st[sensors[i]], "filament_detected");
            }
        }

        if (st.contains("print_task_config") && st["print_task_config"].is_object()) {
            const json& c = st["print_task_config"];
            next.pref_auto_bed_leveling = bget(c, "auto_bed_leveling");
            next.pref_flow_calibrate = bget(c, "flow_calibrate");
            next.pref_shaper_calibrate = bget(c, "shaper_calibrate");
            next.pref_time_lapse = bget(c, "time_lapse_camera");
            next.pref_auto_replenish = bget(c, "auto_replenish_filament");
            next.pref_replenish_ignore_color = bget(c, "replenish_ignore_color");
            next.pref_entangle_detect = bget(c, "filament_entangle_detect");
            next.pref_end_led_off = bget(c, "end_led_turn_off");
            if (c.contains("filament_entangle_sen") && c["filament_entangle_sen"].is_string())
                next.pref_entangle_sens = c["filament_entangle_sen"].get<std::string>();

            // 耗材信息回退：RFID 缺数据时用 print_task_config（原厂 GUI 显示的就是这份）
            auto at = [&](const char* k, int i) -> std::string {
                auto it = c.find(k);
                if (it == c.end() || !it->is_array() || i >= (int)it->size()) return "";
                const json& v = (*it)[i];
                return v.is_string() ? v.get<std::string>() : "";
            };
            auto color_at = [&](int i) -> uint32_t {
                std::string s = at("filament_color_rgba", i);
                if (s.size() < 6) return 0;
                return (uint32_t)strtoul(s.substr(0, 6).c_str(), nullptr, 16);
            };
            auto official_at = [&](int i) -> bool {
                auto it = c.find("filament_official");
                if (it == c.end() || !it->is_array() || i >= (int)it->size()) return false;
                return (*it)[i].is_boolean() && (*it)[i].get<bool>();
            };
            for (int i = 0; i < 4; i++) {
                next.filaments[i].official = official_at(i);
                std::string type = at("filament_type", i);
                if (type.empty() || type == "NONE") continue;
                auto& out = next.filaments[i];
                if (!out.present) {
                    out.present = true;
                    out.material = type;
                    out.sub = at("filament_sub_type", i);
                    out.vendor = at("filament_vendor", i);
                    out.argb = color_at(i);
                }
            }
        }

        // filament_feed left -> e0/e1, right -> e2/e3；每侧 2 个通道
        const char* feed_sides[] = {"filament_feed left", "filament_feed right"};
        for (int s = 0; s < 2; s++) {
            if (!st.contains(feed_sides[s]) || !st[feed_sides[s]].is_object()) continue;
            const json& side = st[feed_sides[s]];
            for (int ch = 0; ch < 2; ch++) {
                int idx = s * 2 + ch;
                std::string key = "extruder" + std::to_string(idx);
                if (!side.contains(key) || !side[key].is_object()) continue;
                const json& f = side[key];
                auto& out = next.feed[idx];
                out.module = bget(f, "module_exist");
                out.detected = bget(f, "filament_detected");
                out.auto_mode = !bget(f, "disable_auto");
                out.state = f.value("channel_state", "none");
                out.error = f.value("channel_error", "ok");
            }
        }

        next.message.clear();
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(mu_);
        st_.connected = false;
        st_.klippy = "error";
        st_.message = std::string("解析失败: ") + e.what();
        return;
    }

    std::lock_guard<std::mutex> lock(mu_);
    // 保留设备信息与统计（它们不是每轮轮询都更新的）
    next.device_name = st_.device_name;
    next.machine_type = st_.machine_type;
    next.serial = st_.serial;
    next.fw_version = st_.fw_version;
    for (int i = 0; i < 4; i++)
        if (next.nozzle_diameter[i] <= 0) next.nozzle_diameter[i] = st_.nozzle_diameter[i];
    next.total_print_time = st_.total_print_time;
    next.total_jobs = st_.total_jobs;
    next.total_filament = st_.total_filament;
    next.materials = st_.materials;
    st_ = std::move(next);
}

void Moonraker::loop() {
    int tick = 0;
    while (running_) {
        poll_once();
        if (tick % 15 == 0) {
            poll_history();
            poll_gui_config();
            load_product_info();  // 换喷嘴后 product_info 会变
            load_materials();     // 材料库很小，顺手刷新
        }
        tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

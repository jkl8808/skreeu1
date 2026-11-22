#include "moonraker.h"

#include "hv/json.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <cctype>
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

bool http_get(const std::string& url, std::string& body, long timeout_s = 4,
              long* status_out = nullptr) {
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
    if (status_out) *status_out = (rc == CURLE_OK) ? status : 0;
    return rc == CURLE_OK && status >= 200 && status < 300;
}

long command_timeout(const std::string& script) {
    auto has = [&](const char* s) { return script.find(s) != std::string::npos; };
    if (has("G28") || has("G29") || has("G34") || has("SCREWS_TILT") ||
        has("BED_MESH_CALIBRATE") || has("PID_CALIBRATE") || has("SHAPER_CALIBRATE"))
        return 300;   // 回零/校准可能几十秒到几分钟
    if (has("AUTO_FEEDING") || has("FEED_AUTO") || has("MANUAL_FEEDING") || has("M600"))
        return 600;   // 进料/退料可能几分钟
    if (has("SDCARD_PRINT_FILE")) return 60;  // 载入文件 + 预检查
    return 15;
}

std::string http_error_message(const std::string& body) {
    try {
        json j = json::parse(body);
        if (j.contains("error")) {
            const json& e = j["error"];
            if (e.is_object() && e.contains("message") && e["message"].is_string())
                return e["message"].get<std::string>();
            if (e.is_string()) return e.get<std::string>();
        }
        if (j.contains("result") && j["result"].is_string() &&
            j["result"].get<std::string>() != "ok")
            return j["result"].get<std::string>();
    } catch (...) {
    }
    return {};
}

bool http_post(const std::string& url, std::string& body, long timeout_s,
               long* status_out = nullptr, int* curl_out = nullptr) {
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
    if (status_out) *status_out = status;
    if (curl_out) *curl_out = (int)rc;
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

std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

std::string encode_path(const std::string& p) {
    std::string out;
    size_t start = 0;
    while (start <= p.size()) {
        size_t slash = p.find('/', start);
        std::string seg = p.substr(start, slash == std::string::npos ? std::string::npos
                                                                     : slash - start);
        if (!out.empty()) out += '/';
        out += url_encode(seg);
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return out;
}

std::string trim_ws(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split_semicolon(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ';') {
            out.push_back(trim_ws(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(trim_ws(cur));
    return out;
}

std::vector<std::string> str_list(const json& j, const char* key) {
    std::vector<std::string> out;
    auto it = j.find(key);
    if (it == j.end()) return out;
    if (it->is_array()) {
        for (const auto& v : *it) {
            if (v.is_string()) out.push_back(trim_ws(v.get<std::string>()));
            else if (v.is_number()) out.push_back(v.dump());
        }
    } else if (it->is_string()) {
        out = split_semicolon(it->get<std::string>());
    }
    return out;
}

std::vector<double> num_list(const json& j, const char* key) {
    std::vector<double> out;
    auto it = j.find(key);
    if (it == j.end()) return out;
    auto add = [&](const json& v) {
        if (v.is_number()) out.push_back(v.get<double>());
        else if (v.is_string()) {
            try { out.push_back(std::stod(trim_ws(v.get<std::string>()))); } catch (...) {}
        }
    };
    if (it->is_array()) {
        for (const auto& v : *it) add(v);
    } else if (it->is_string()) {
        for (const auto& part : split_semicolon(it->get<std::string>())) {
            try { out.push_back(std::stod(part)); } catch (...) {}
        }
    } else {
        add(*it);
    }
    return out;
}

uint32_t color_value(const std::string& s) {
    std::string v = trim_ws(s);
    if (!v.empty() && v[0] == '#') v = v.substr(1);
    if (v.size() == 8) v = v.substr(2);  // AARRGGBB
    if (v.size() < 6) return 0xFFFFFF;
    return (uint32_t)strtoul(v.substr(0, 6).c_str(), nullptr, 16);
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
    cmd_thread_ = std::thread([this] { send_loop(); });
}

void Moonraker::stop() {
    running_ = false;
    {
        std::lock_guard<std::mutex> lk(cmd_mu_);
        cmd_quit_ = true;
    }
    cmd_cv_.notify_all();
    if (cmd_thread_.joinable()) cmd_thread_.join();
}

std::string Moonraker::take_command_error() {
    std::lock_guard<std::mutex> lk(cmd_err_mu_);
    std::string msg = std::move(cmd_error_msg_);
    cmd_error_msg_.clear();
    return msg;
}

PrinterState Moonraker::snapshot() {
    std::lock_guard<std::mutex> lock(mu_);
    return st_;
}

bool Moonraker::send_homing() {
    if (cfg_.read_only) return false;
    {
        std::lock_guard<std::mutex> lk(cmd_mu_);
        if (cmd_quit_) return false;
        cmd_queue_.push_back(CmdItem{"G28", true});
    }
    cmd_cv_.notify_one();
    return true;
}

bool Moonraker::take_homing_result(bool& ok, std::string& error) {
    std::lock_guard<std::mutex> lk(home_mu_);
    if (!home_done_) return false;
    ok = home_ok_;
    error = home_error_;
    home_done_ = false;
    home_error_.clear();
    return true;
}

bool Moonraker::send_gcode(const std::string& script) {
    if (cfg_.read_only) return false;
    {
        std::lock_guard<std::mutex> lk(cmd_mu_);
        if (cmd_quit_) return false;
        cmd_queue_.push_back(CmdItem{script, false});
    }
    cmd_cv_.notify_one();
    return true;   // 入队即返回，真正的 HTTP 请求在后台线程做
}

// 后台串行发送 G-code：避免 SET_PRINT_FILAMENT_CONFIG 这类慢命令
// 把 LVGL 主线程卡住（选耗材要等 1 秒才响应就是它导致的）。
void Moonraker::send_loop() {
    for (;;) {
        CmdItem item;
        {
            std::unique_lock<std::mutex> lk(cmd_mu_);
            cmd_cv_.wait(lk, [this] { return cmd_quit_ || !cmd_queue_.empty(); });
            if (cmd_queue_.empty()) {
                if (cmd_quit_) return;
                continue;
            }
            item = std::move(cmd_queue_.front());
            cmd_queue_.pop_front();
        }
        const std::string& script = item.script;
        CURL* curl = curl_easy_init();
        if (!curl) continue;
        char* esc = curl_easy_escape(curl, script.c_str(), (int)script.size());
        std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                          "/printer/gcode/script?script=" + (esc ? esc : "");
        curl_free(esc);
        std::string body;
        long status = 0;
        int ccode = 0;
        bool ok = http_post(url, body, command_timeout(script), &status, &ccode);
        std::string msg;
        if (!ok) {
            if (ccode == CURLE_OPERATION_TIMEDOUT) {
                msg = "命令超时，打印机可能仍在执行";
            } else if (status >= 400) {
                msg = http_error_message(body);
                if (msg.empty()) msg = "HTTP " + std::to_string(status);
            } else {
                msg = "无法连接打印机";
            }
            if (!item.homing) {
                std::lock_guard<std::mutex> lk(cmd_err_mu_);
                cmd_error_msg_ = msg;
            }
        }
        if (item.homing) {
            std::lock_guard<std::mutex> lk(home_mu_);
            home_done_ = true;
            home_ok_ = ok;
            home_error_ = ok ? "" : msg;
        }
        curl_easy_cleanup(curl);
    }
}

std::vector<GcodeFile> Moonraker::list_gcode_files() {
    std::vector<GcodeFile> files;
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/files/list?root=gcodes";
    if (!http_get(url, body, 8)) return files;
    try {
        json j = json::parse(body);
        const json& arr = j.at("result");
        for (const auto& it : arr) {
            if (!it.is_object()) continue;
            std::string path = it.value("path", "");
            if (path.empty()) continue;
            std::string lower = path;
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            if (lower.size() < 6 || lower.substr(lower.size() - 6) != ".gcode") continue;
            GcodeFile f;
            f.name = path;
            f.modified = dget(it, "modified");
            f.size = dget(it, "size");
            files.push_back(std::move(f));
        }
    } catch (...) {
        return {};
    }
    std::sort(files.begin(), files.end(), [](const GcodeFile& a, const GcodeFile& b) {
        if (a.modified != b.modified) return a.modified > b.modified;
        return a.name < b.name;
    });
    return files;
}

bool Moonraker::gcode_metadata(const std::string& filename, GcodeMeta& out) {
    out = GcodeMeta();
    std::string body;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/files/metadata?filename=" + url_encode(filename);
    if (!http_get(url, body, 8)) return false;
    try {
        json j = json::parse(body).at("result");
        if (!j.is_object()) return false;

        out.ok = true;
        out.filename = j.value("filename", filename);
        if (out.filename.empty()) out.filename = filename;
        out.slicer = j.value("slicer", "");
        out.slicer_version = j.value("slicer_version", "");
        out.estimated_time = dget(j, "estimated_time");
        out.layer_count = (int)dget(j, "layer_count");
        out.layer_height = dget(j, "layer_height");
        out.object_height = dget(j, "object_height");
        out.nozzle_diameter = dget(j, "nozzle_diameter");
        out.filament_total = dget(j, "filament_total");
        out.filament_weight_total = dget(j, "filament_weight_total");

        auto th = j.find("thumbnails");
        if (th != j.end() && th->is_array()) {
            int best_d = 9999;
            for (const auto& t : *th) {
                if (!t.is_object()) continue;
                std::string rp = t.value("relative_path", "");
                if (rp.empty()) continue;
                int w = (int)dget(t, "width");
                int d = std::abs(w - 96);
                if (out.thumb_path.empty() || d < best_d) {
                    out.thumb_path = rp;
                    best_d = d;
                }
            }
        }
        out.raw = j.dump();

        auto types = str_list(j, "filament_type");
        auto colors = str_list(j, "filament_colors");
        if (colors.empty()) colors = str_list(j, "filament_colour");
        auto weights = num_list(j, "filament_weight");
        if (weights.empty()) weights = num_list(j, "filament_weights");
        auto used_mm = num_list(j, "filament_used_mm");
        auto dia = num_list(j, "nozzle_diameter_list");
        auto temp = num_list(j, "nozzle_temp");
        auto vol = str_list(j, "filament_volume_type");

        int n = (int)types.size();
        auto grow = [&](size_t sz) { if ((int)sz > n) n = (int)sz; };
        grow(colors.size()); grow(weights.size()); grow(used_mm.size());
        grow(dia.size()); grow(temp.size()); grow(vol.size());
        if (n > 4) n = 4;
        out.filament_count = n;
        for (int i = 0; i < n; i++) {
            GcodeFilament& f = out.filaments[i];
            if (i < (int)types.size()) f.type = types[i];
            if (f.type.empty() || f.type == "NONE") f.type = "—";
            if (i < (int)colors.size()) f.argb = color_value(colors[i]);
            if (i < (int)weights.size()) f.weight_g = weights[i];
            if (i < (int)used_mm.size()) f.used_mm = used_mm[i];
            if (i < (int)dia.size()) f.nozzle_diameter = dia[i];
            else f.nozzle_diameter = out.nozzle_diameter;
            if (i < (int)temp.size()) f.nozzle_temp = (int)temp[i];
            if (i < (int)vol.size() && !vol[i].empty()) f.volume_type = vol[i];
            f.used = f.weight_g > 0.0001 || f.used_mm > 0.0001;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Moonraker::start_print(const GcodeMeta& meta, const int map[4]) {
    if (cfg_.read_only || meta.raw.empty()) return false;
    json m;
    try {
        m = json::parse(meta.raw);
    } catch (...) {
        return false;
    }

    auto esc = [](std::string v) {
        std::string out;
        for (char c : v) {
            if (c == '"') out += '\\';
            out += c;
        }
        return out;
    };
    // Klipper 的参数用双引号包起来，数组里的字符串必须用单引号，
    // 否则 FILAMENT_TYPE="["PLA"]" 会把 G-code 参数提前截断。
    auto py_repr = [](const json& v) -> std::string {
        if (!v.is_string()) return v.dump();
        std::string out;
        for (char c : v.get<std::string>()) {
            if (c == '\\' || c == '\'') out += '\\';
            out += c;
        }
        return "'" + out + "'";
    };
    auto py_list = [&](const json& v) -> std::string {
        std::string out = "[";
        if (v.is_array()) {
            bool first = true;
            for (const auto& item : v) {
                if (!first) out += ",";
                first = false;
                out += py_repr(item);
            }
        }
        out += "]";
        return out;
    };
    auto list_str = [&](const json& v) -> std::string {
        if (v.is_array()) {
            for (const auto& item : v)
                if (item.is_string()) return py_list(v);
        }
        return v.dump();
    };
    auto add_param = [](std::string& script, const char* key, const std::string& val) {
        script += " ";
        script += key;
        script += "=\"";
        script += val;
        script += "\"";
    };

    std::string script = "SDCARD_PRINT_FILE_WITH_PARAMETERS FILENAME=\"" + esc(meta.filename) + "\"";

    char mt[64];
    snprintf(mt, sizeof(mt), "[[0,%d],[1,%d],[2,%d],[3,%d]]",
             map[0], map[1], map[2], map[3]);
    add_param(script, "MAP_TABLE", mt);

    auto opt_num = [&](const char* key, const char* meta_key) {
        auto it = m.find(meta_key);
        if (it != m.end() && it->is_number()) add_param(script, key, it->dump());
    };
    auto opt_list = [&](const char* key, const char* meta_key) {
        auto it = m.find(meta_key);
        if (it == m.end()) return;
        if (it->is_array()) add_param(script, key, list_str(*it));
        else if (it->is_string()) {
            auto parts = split_semicolon(it->get<std::string>());
            if (parts.size() > 1) {
                json arr = json::array();
                for (const auto& p : parts) arr.push_back(p);
                add_param(script, key, list_str(arr));
            }
        }
    };

    opt_num("LINE_WIDTH", "line_width");
    opt_num("LAYER_HEIGHT", "layer_height");
    opt_list("OUTER_WALL_SPEED_LIST", "outer_wall_speed_list");
    opt_list("OUTER_WALL_ACCELERATION_LIST", "outer_wall_acceleration_list");
    opt_list("NOZZLE_DIAMETER_LIST", "nozzle_diameter_list");
    opt_list("NOZZLE_TEMP", "nozzle_temp");
    opt_list("FILAMENT_FLOW_RATIO", "filament_flow_ratio");
    opt_list("FILAMENT_DIAMETER", "filament_diameter");
    opt_list("FILAMENT_MAX_VOLUMETRIC_SPEED", "filament_max_volumetric_speed");
    opt_list("FILAMENT_VOLUME_TYPE", "filament_volume_type");
    opt_list("PROCESS_FLOW_SUPPORT", "process_flow_support");

    // filament_type 在 Moonraker 里是分号分隔字符串
    auto types = m.find("filament_type");
    if (types != m.end()) {
        json arr = json::array();
        if (types->is_array()) {
            for (const auto& v : *types)
                arr.push_back(v.is_string() ? v.get<std::string>() : "NONE");
        } else if (types->is_string()) {
            for (const auto& p : split_semicolon(types->get<std::string>()))
                arr.push_back(p.empty() ? "NONE" : p);
        }
        add_param(script, "FILAMENT_TYPE", py_list(arr));
    }

    // filament_used_g 对应 Moonraker 的 filament_weight
    auto w = m.find("filament_weight");
    if (w == m.end()) w = m.find("filament_weights");
    if (w != m.end() && w->is_array()) add_param(script, "FILAMENT_USED_G", list_str(*w));
    auto um = m.find("filament_used_mm");
    if (um != m.end() && um->is_array()) add_param(script, "FILAMENT_USED_MM", list_str(*um));

    if (m.contains("slicer") && m["slicer"].is_string())
        add_param(script, "SLICER", m["slicer"].get<std::string>());
    if (m.contains("slicer_version") && m["slicer_version"].is_string())
        add_param(script, "SLICER_VERSION", m["slicer_version"].get<std::string>());

    return send_gcode(script);
}

bool Moonraker::fetch_thumb(const std::string& rel_path, std::string& out,
                            long* status_out) {
    if (rel_path.empty()) return false;
    std::string url = "http://" + cfg_.host + ":" + std::to_string(cfg_.port) +
                      "/server/files/gcodes/" + encode_path(rel_path);
    return http_get(url, out, 6, status_out);
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
                      "&led%20cavity_led&fan"
                      "&fan_generic%20cavity_fan&fan_generic%20circulation_fan";

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

        if (st.contains("toolhead") && st["toolhead"].is_object())
            next.homed_axes = st["toolhead"].value("homed_axes", "");

        if (st.contains("fan") && st["fan"].is_object())
            next.fan_speed = dget(st["fan"], "speed");
        if (st.contains("fan_generic cavity_fan") &&
            st["fan_generic cavity_fan"].is_object())
            next.fan_aux_speed = dget(st["fan_generic cavity_fan"], "speed");
        if (st.contains("fan_generic circulation_fan") &&
            st["fan_generic circulation_fan"].is_object())
            next.fan_circ_speed = dget(st["fan_generic circulation_fan"], "speed");

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

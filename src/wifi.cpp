#include "wifi.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::string run_cmd(const std::vector<std::string>& args, int timeout_ms) {
    if (args.empty()) return {};
    int fds[2];
    if (pipe(fds) != 0) return {};
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return {};
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(fds[1]);
    std::string out;
    char buf[4096];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        int wait_ms = 100;
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            kill(pid, SIGKILL);
            break;
        }
        struct pollfd p = {fds[0], POLLIN, 0};
        int pr = poll(&p, 1, wait_ms);
        if (pr < 0) break;
        if (pr == 0) continue;
        ssize_t n = read(fds[0], buf, sizeof buf);
        if (n <= 0) break;
        if (out.size() < 256 * 1024) out.append(buf, (size_t)n);
    }
    close(fds[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    return out;   // 调用方按输出内容判断成功/失败
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t p = line.find('\t', start);
        if (p == std::string::npos) {
            out.push_back(line.substr(start));
            break;
        }
        out.push_back(line.substr(start, p - start));
        start = p + 1;
    }
    return out;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

/* wpa_cli 会把不可打印字符转义成 \xNN，反斜杠转义成两个反斜杠 */
std::string decode_ssid(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == 'x' && i + 3 < s.size() &&
                isxdigit((unsigned char)s[i + 2]) && isxdigit((unsigned char)s[i + 3])) {
                char hex[3] = {s[i + 2], s[i + 3], 0};
                out.push_back((char)strtol(hex, nullptr, 16));
                i += 3;
            } else {
                out.push_back(s[i + 1]);
                i++;
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

// wpa_cli 的 ssid/psk 参数是带引号的 C 字符串
std::string encode_wpa(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::map<std::string, std::string> parse_kv(const std::string& out) {
    std::map<std::string, std::string> kv;
    size_t start = 0;
    while (start < out.size()) {
        size_t end = out.find('\n', start);
        std::string line = out.substr(start, end == std::string::npos ? std::string::npos
                                                                     : end - start);
        size_t eq = line.find('=');
        if (eq != std::string::npos) kv[line.substr(0, eq)] = trim(line.substr(eq + 1));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return kv;
}

std::string find_wpa_cli() {
    const char* candidates[] = {"/usr/sbin/wpa_cli", "/sbin/wpa_cli", "wpa_cli"};
    for (const char* c : candidates)
        if (access(c, X_OK) == 0) return c;
    return "wpa_cli";
}

std::string find_ip() {
    const char* candidates[] = {"/usr/sbin/ip", "/sbin/ip", "ip"};
    for (const char* c : candidates)
        if (access(c, X_OK) == 0) return c;
    return "ip";
}

} // namespace

Wifi::Wifi() {
    const char* iface = getenv("SKREEU1_WIFI_IF");
    iface_ = iface && *iface ? iface : "wlan0";
    wpa_cli_ = find_wpa_cli();
    const char* conf = getenv("SKREEU1_WPA_CONF");
    conf_ = conf && *conf ? conf
                          : "/home/lava/printer_data/gui/wpa_supplicant.conf";
    mock_ = getenv("SKREEU1_WIFI_MOCK") != nullptr;

    if (mock_) {
        snap_.available = true;
        snap_.connected = true;
        snap_.ssid = "Q6K";
        snap_.ip = "192.168.6.10";
        snap_.state = "COMPLETED";
        snap_.rssi = -28;
        snap_.networks = {
            {"Q6K", -28, true, false},
            {"klaser", -45, true, false},
            {"FC\xe4\xb8\x89\xe9\x87\x91", -56, true, false},
            {"217", -57, true, false},
            {"HUAWEI-J1CJ54", -59, true, false},
            {"iKuai-415", -61, true, false},
        };
        snap_.scan_gen = 1;
    }
}

Wifi::~Wifi() { stop(); }

void Wifi::start() {
    if (mock_ || worker_.joinable()) return;
    worker_ = std::thread([this] { worker_loop(); });
    request_status();
}

void Wifi::stop() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!worker_.joinable()) return;
        quit_ = true;
        cv_.notify_all();
    }
    worker_.join();
}

void Wifi::enqueue(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (quit_) return;
        tasks_.push_back(std::move(fn));
        cv_.notify_all();
    }
}

void Wifi::worker_loop() {
    for (;;) {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this] { return quit_ || !tasks_.empty(); });
            if (quit_ && tasks_.empty()) return;
            fn = std::move(tasks_.front());
            tasks_.pop_front();
        }
        try {
            fn();
        } catch (...) {
        }
    }
}

void Wifi::request_status() {
    if (mock_ || status_busy_.exchange(true)) return;
    enqueue([this] {
        do_status();
        status_busy_ = false;
    });
}

void Wifi::request_scan() {
    if (mock_ || scan_busy_.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.scanning = true;
    }
    enqueue([this] {
        do_scan();
        scan_busy_ = false;
    });
}

void Wifi::request_connect(const std::string& ssid, const std::string& password, bool sae) {
    if (mock_ || connect_busy_.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.connecting = true;
        snap_.error.clear();
    }
    enqueue([this, ssid, password, sae] {
        do_connect(ssid, password, sae);
        connect_busy_ = false;
    });
}

void Wifi::request_disconnect() {
    if (mock_) return;
    enqueue([this] { do_disconnect(); });
}

void Wifi::request_reconnect() {
    if (mock_) return;
    enqueue([this] { do_reconnect(); });
}

WifiSnapshot Wifi::snapshot() {
    std::lock_guard<std::mutex> lk(snap_mu_);
    return snap_;
}

void Wifi::do_status() {
    if (access("/var/run/wpa_supplicant", F_OK) != 0 && access(wpa_cli_.c_str(), X_OK) != 0) {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.available = false;
        snap_.connected = false;
        return;
    }
    std::string out = run_cmd({wpa_cli_, "-i", iface_, "status"}, 5000);
    auto kv = parse_kv(out);
    bool ok = !kv.empty() && kv.count("wpa_state");
    std::lock_guard<std::mutex> lk(snap_mu_);
    snap_.available = ok;
    if (!ok) {
        snap_.connected = false;
        snap_.state = "UNAVAILABLE";
        return;
    }
    snap_.state = kv["wpa_state"];
    snap_.connected = snap_.state == "COMPLETED";
    snap_.ssid = decode_ssid(kv.count("ssid") ? kv["ssid"] : "");
    snap_.ip = kv.count("ip_address") ? kv["ip_address"] : "";
    if (snap_.connected && snap_.ip.empty()) {
        std::string ip_out = run_cmd({find_ip(), "-4", "addr", "show", "dev", iface_}, 3000);
        size_t p = ip_out.find(" inet ");
        if (p != std::string::npos) {
            size_t e = ip_out.find('/', p + 6);
            if (e != std::string::npos) snap_.ip = ip_out.substr(p + 6, e - (p + 6));
        }
    }
    if (snap_.connected) {
        std::string sig = run_cmd({wpa_cli_, "-i", iface_, "signal_poll"}, 3000);
        auto skv = parse_kv(sig);
        if (skv.count("RSSI")) snap_.rssi = atoi(skv["RSSI"].c_str());
    }
}

void Wifi::do_scan() {
    std::string scan = run_cmd({wpa_cli_, "-i", iface_, "scan"}, 8000);
    (void)scan;   // 已经在扫描时会返回 FAIL，直接等结果即可
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
    std::string out = run_cmd({wpa_cli_, "-i", iface_, "scan_results"}, 5000);

    std::map<std::string, WifiNetwork> by_ssid;
    size_t start = 0;
    while (start < out.size()) {
        size_t end = out.find('\n', start);
        std::string line = out.substr(start, end == std::string::npos ? std::string::npos
                                                                     : end - start);
        if (end == std::string::npos) start = out.size();
        else start = end + 1;
        auto f = split_tabs(trim(line));
        if (f.size() < 5) continue;
        if (f[0].find(':') == std::string::npos) continue;   // 表头
        std::string ssid = decode_ssid(f[4]);
        if (ssid.empty()) continue;                            // 隐藏网络
        WifiNetwork n;
        n.ssid = ssid;
        n.signal = atoi(f[2].c_str());
        std::string flags = f[3];
        n.secure = flags.find("WPA") != std::string::npos ||
                   flags.find("RSN") != std::string::npos ||
                   flags.find("WEP") != std::string::npos ||
                   flags.find("SAE") != std::string::npos;
        n.sae = flags.find("SAE") != std::string::npos &&
                flags.find("PSK") == std::string::npos;
        auto it = by_ssid.find(ssid);
        if (it == by_ssid.end() || n.signal > it->second.signal) by_ssid[ssid] = n;
    }

    std::vector<WifiNetwork> nets;
    nets.reserve(by_ssid.size());
    for (auto& kvp : by_ssid) nets.push_back(kvp.second);
    std::sort(nets.begin(), nets.end(),
              [](const WifiNetwork& a, const WifiNetwork& b) { return a.signal > b.signal; });
    if (nets.size() > 32) nets.resize(32);

    {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.scanning = false;
        snap_.networks = std::move(nets);
        snap_.scan_gen++;
    }
    do_status();
}

void Wifi::do_connect(std::string ssid, std::string password, bool sae) {
    std::string networks = run_cmd({wpa_cli_, "-i", iface_, "list_networks"}, 5000);
    std::string id;
    std::string prev_id;
    bool existed = false;
    size_t start = 0;
    while (start < networks.size()) {
        size_t end = networks.find('\n', start);
        std::string line = networks.substr(start, end == std::string::npos ? std::string::npos
                                                                          : end - start);
        if (end == std::string::npos) start = networks.size();
        else start = end + 1;
        auto f = split_tabs(trim(line));
        if (f.size() < 2) continue;
        if (f[0].empty() || !isdigit((unsigned char)f[0][0])) continue;
        if (f.size() >= 4 && f[3].find("CURRENT") != std::string::npos) prev_id = f[0];
        if (decode_ssid(f[1]) == ssid) {
            id = f[0];
            existed = true;
        }
    }
    const std::string bak = conf_ + ".skreeu1.bak";
    run_cmd({"cp", "-f", conf_, bak}, 5000);
    bool have_bak = access(bak.c_str(), R_OK) == 0;

    if (!existed) {
        std::string out = trim(run_cmd({wpa_cli_, "-i", iface_, "add_network"}, 5000));
        size_t nl = out.rfind('\n');
        if (nl != std::string::npos) out = out.substr(nl + 1);
        id = trim(out);
        if (id.empty() || !isdigit((unsigned char)id[0])) {
            std::lock_guard<std::mutex> lk(snap_mu_);
            snap_.connecting = false;
            snap_.error = "无法创建 Wi-Fi 配置";
            return;
        }
    }

    run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "ssid", encode_wpa(ssid)}, 5000);
    if (password.empty()) {
        run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "key_mgmt", "NONE"}, 5000);
    } else {
        run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "psk", encode_wpa(password)}, 5000);
        run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "key_mgmt",
                 sae ? "SAE" : "WPA-PSK"}, 5000);
        run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "ieee80211w",
                 sae ? "2" : "0"}, 5000);
    }
    run_cmd({wpa_cli_, "-i", iface_, "set_network", id, "scan_ssid", "1"}, 5000);
    run_cmd({wpa_cli_, "-i", iface_, "enable_network", id}, 5000);
    run_cmd({wpa_cli_, "-i", iface_, "select_network", id}, 5000);

    bool ok = false;
    std::string ip;
    for (int i = 0; i < 40; i++) {   // 最多等 20 秒
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        do_status();
        WifiSnapshot s;
        {
            std::lock_guard<std::mutex> lk(snap_mu_);
            s = snap_;
        }
        if (s.connected) {
            ok = true;
            ip = s.ip;
            if (!ip.empty()) break;
        }
    }

    if (!ok) {
        if (!existed) run_cmd({wpa_cli_, "-i", iface_, "remove_network", id}, 5000);
        // 恢复连接前的配置文件，保证失败后还能回到原来的网络
        if (have_bak) {
            run_cmd({"cp", "-f", bak, conf_}, 5000);
            run_cmd({wpa_cli_, "-i", iface_, "reconfigure"}, 8000);
        } else if (!prev_id.empty() && prev_id != id) {
            run_cmd({wpa_cli_, "-i", iface_, "select_network", prev_id}, 5000);
        }
        {
            std::lock_guard<std::mutex> lk(snap_mu_);
            snap_.connecting = false;
            snap_.error = "连接失败：密码错误或信号太弱";
        }
        do_status();
        return;
    }

    // 只有确认连上了才写盘，避免把旧配置写坏
    run_cmd({wpa_cli_, "-i", iface_, "save_config"}, 5000);
    if (access(conf_.c_str(), R_OK) == 0) run_cmd({"cp", "-f", conf_, bak}, 5000);
    {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.connecting = false;
        snap_.error.clear();
    }
    do_status();
}

void Wifi::do_disconnect() {
    run_cmd({wpa_cli_, "-i", iface_, "disconnect"}, 5000);
    do_status();
}

void Wifi::do_reconnect() {
    run_cmd({wpa_cli_, "-i", iface_, "reconfigure"}, 8000);
    run_cmd({wpa_cli_, "-i", iface_, "reconnect"}, 5000);
    do_status();
    WifiSnapshot s = snapshot();
    if (!s.available) {
        std::lock_guard<std::mutex> lk(snap_mu_);
        snap_.error = "Wi-Fi 服务未运行，请重启打印机";
    }
}

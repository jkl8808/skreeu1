#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct WifiNetwork {
    std::string ssid;
    int signal = -99;   // dBm
    bool secure = false;
    bool sae = false;   // 只支持 WPA3-SAE（没有 WPA2-PSK 回落）
};

struct WifiSnapshot {
    bool available = false;   // wpa_cli / wpa_supplicant 可用
    bool connected = false;   // wpa_state=COMPLETED
    bool scanning = false;
    bool connecting = false;
    std::string ssid;
    std::string ip;
    std::string state;
    std::string error;
    int rssi = 0;
    uint32_t scan_gen = 0;    // 每次扫描结果更新时 +1
    std::vector<WifiNetwork> networks;
};

// 通过 wpa_cli 管理 wlan0。所有命令都在后台线程里串行执行，
// 不会阻塞 LVGL 主循环。
class Wifi {
public:
    Wifi();
    ~Wifi();

    void start();
    void stop();

    void request_status();
    void request_scan();
    void request_connect(const std::string& ssid, const std::string& password, bool sae);
    void request_disconnect();

    WifiSnapshot snapshot();

private:
    void enqueue(std::function<void()> fn);
    void worker_loop();
    void do_status();
    void do_scan();
    void do_connect(std::string ssid, std::string password, bool sae);
    void do_disconnect();

    std::string iface_;
    std::string wpa_cli_;
    bool mock_ = false;

    std::thread worker_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    bool quit_ = false;

    std::atomic<bool> status_busy_{false};
    std::atomic<bool> scan_busy_{false};
    std::atomic<bool> connect_busy_{false};

    std::mutex snap_mu_;
    WifiSnapshot snap_;
};

# SKREEU1 兼容层：原厂功能是怎么接进来的

SKREEU1 不依赖 Snapmaker 的闭源 GUI，而是直接复用打印机上已有的两层接口：

| 层 | 位置 | 用途 |
| --- | --- | --- |
| Klipper 对象 | `printer/objects/query` | 读状态（温度、打印进度、偏好、检测、净化器、喷嘴） |
| G-code 命令 | `printer/gcode/script` | 写状态（开关、灵敏度、风速、喷嘴类型） |
| `gui_config.json` | `/server/files/config/gui_config.json` | 原厂 GUI 自己的偏好（目前只读展示） |

所有读写都走 Moonraker HTTP，因此 SKREEU1 可以跑在任何能访问 `:7125` 的机器上，
也天然兼容"不依赖 Klipper 的功能"——这些功能本来就有自己的 Klipper 模块。

## 1. 轮询的对象

`src/moonraker.cpp::poll_once()`

```
toolhead extruder extruder1 extruder2 extruder3 heater_bed
purifier defect_detection gcode_move filament_detect
print_task_config
filament_feed left filament_feed right
filament_motion_sensor e0_filament .. e3_filament
```

注意两个坑：

- Klipper 对不存在的对象也返回 `{}`，不能只判断 key 是否存在，要检查字段
  （例如 `filament_detected`）。
- `[filament_feed left]` 这种带后缀的 section 走 `load_config_prefix`，
  `get_status()` 返回的是 `{"extruder0": {...}, "extruder1": {...}}`。

## 2. 对象 → UI 字段

| Klipper | SKREEU1 | 说明 |
| --- | --- | --- |
| `extruder*.switch_count` | `switch_count[4]` | 换头次数 |
| `extruder*.nozzle_diameter` / `nozzle_volume_type` | `nozzle_diameter[]` / `nozzle_type[]` | 喷嘴直径 / 标准·高流 |
| `print_task_config.auto_bed_leveling` … | `pref_*` | 打印偏好整套 |
| `print_task_config.filament_*` | `filaments[]` 回退 | RFID 没数据时用这份（原厂 GUI 也用它） |
| `filament_feed left/right` | `feed[4]` | `disable_auto` 取反成"自动进料" |
| `filament_detect.info[]` | `filaments[].nfc/card_type/hotend_*/bed_temp/diameter/weight` | NFC 卡片信息（`CARD_TYPE` 非空即识别到） |
| `filament_motion_sensor eN_filament` | `filaments[n].detected` | 有传感器就用传感器 |
| `defect_detection.*` | `defect_*` | 总开关 + 4 分组 + 灵敏度 |
| `purifier.*` | `purifier_*` | 风速 / 模式 / 滤芯时间 / rpm |

## 3. 写入命令

| UI | G-code |
| --- | --- |
| 打印偏好开关 | `SET_PRINT_PREFERENCES BED_LEVEL=… FLOW_CALIBRATE=… SHAPER_CALIBRATE=… TIME_LAPSE_CAMERA=…` |
| 自动续料 / 忽略颜色 / 结束关灯 | `SET_PRINT_PREFERENCES AUTO_REPLENISH_FILAMENT=… REPLENISH_IGNORE_COLOR=… END_LED_TURN_OFF=…` |
| 空打检测 | `SET_PRINT_PREFERENCES FILAMENT_ENTANGLE_DETECT=0|1 FILAMENT_ENTANGLE_SEN=low|medium|high` |
| 自动进料 T1–T4 | `FEED_AUTO MODULE=left|right CHANNEL=0|1 AUTO=0|1`（在喷嘴详情页，按当前喷嘴） |
| 异物检测 | `DEFECT_DETECTION_CONFIG MAIN_ENABLE=… CLEAN_BED_ENABLE=… NOODLE_ENABLE=… RESIDUE_ENABLE=… NOZZLE_ENABLE=… SENSITIVITY=low|high` |
| 净化器风速 | `SET_PURIFIER FAN=inner SPEED=0..100` |
| 净化器模式 | `SET_PURIFIER_MODE MODE=0..3`（待机/降温/预热/保温） |
| 喷嘴类型 | `SET_NOZZLE_PROPERTIES EXTRUDER=extruder[1..3] VOLUME_TYPE=standard|high_flow`（喷嘴详情页） |
| 喷嘴温度 | `M104 S<temp>`（T1）/ `M104 T<n> S<temp>`（T2–T4） |
| 热床温度 | `M140 S<temp>`（首页热床卡 → 调温浮层，滑块上限 120°C） |
| 快捷温度 | UI 本地配置：喷嘴 `skreeu1_temps.cfg`（`SKREEU1_TEMPS`），热床 `skreeu1_bed_temps.cfg`（`SKREEU1_BED_TEMPS`），各自 6 个 |
| 载入耗材 | `AUTO_FEEDING EXTRUDER=<0..3>` |
| 退料 | `AUTO_FEEDING EXTRUDER=<0..3> UNLOAD=1 STAGE=prepare` 然后 `STAGE=doing` |
| 更换耗材 | `SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=<0..3> VENDOR=… FILAMENT_TYPE=… FILAMENT_SUBTYPE=… [FILAMENT_COLOR_RGBA=RRGGBBAA]` |
| 腔体灯开关 | `SET_LED LED=cavity_led WHITE=0|1`；状态读 `led cavity_led.color_data`（首页顶部「灯光」） |
| Wi-Fi 扫描/连接 | `wpa_cli -i wlan0 scan` / `scan_results` / `list_networks` / `add_network` / `set_network` / `select_network` / `save_config` / `disconnect`（配置由 `dhcpcd` 处理地址） |

约束（固件里写死的）：

- 打印 / 暂停中不能改喷嘴参数，UI 里直接不发命令。
- 官方 RFID 耗材（`filament_official=true`）不能被 `SET_PRINT_FILAMENT_CONFIG` 覆盖，
  固件会报 `official filament, not configurable!`；UI 里点耗材卡不会打开更换列表。
- `VENDOR` 是自由字符串：不在 `filament_parameters.json` 里的品牌会按
  `generic_<TYPE>_<SUBTYPE>` 回退参数（`filament_parameters.py::_search_filament_param_value`），
  所以自定义品牌是安全的，只是流量/温度用通用值。品牌列表在 `skreeu1_brands.cfg`。
- 净化器 `power_detected: false` 时 SPEED>0 会报错，UI 里直接禁用风速按钮。
- `printer.control.print_speed` 不带参数会以 `S=0` 下发并让 Klipper shutdown，
  所以所有控制类命令必须带有效数值。

## 4. 只读模式

`--readonly` 只跳过所有写命令（`Moonraker::send_gcode` 直接返回），
轮询照常，所以可以安全地对着真机跑。建议先在桌面上用只读模式看一遍。
真机上的 `/userdata/skreeu1/start.sh` 默认是**可写**的，需要只读时用
`SKREEU1_READONLY=1 /userdata/skreeu1/start.sh`。

## 5. 本地验证

对着真机时建议先只读：

```bash
./build/skreeu1 --host 192.168.1.50 --port 7125 --readonly

# 无头截图，用来对比 UI 改动
./build/skreeu1 --shot out.png --page 3 --scroll 200 --host 192.168.1.50 --readonly
./build/skreeu1 --shot sheet.png --page 0 --sheet 2 --picker --host 192.168.1.50 --readonly
```

材料选择列表来自打印机的 `config/snapmaker/filament_parameters.json`
（Moonraker 的 `/server/files/config/...`）。想在没有打印机的情况下开发，
可以自己搭一个返回同样的 Klipper 对象和 G-code 接口的模拟 Moonraker。

## 6. 还没接上的

摄像头 / 延时摄影、OTA、局域网访问码、云端账号仍然只在 unisrv 的 MQTT 里
（真机 `127.0.0.1:3893`），SKREEU1 目前没有 MQTT 客户端。

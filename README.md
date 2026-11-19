# skreeu1 — Snapmaker U1 触屏界面

[![build](https://github.com/jkl8808/skreeu1/actions/workflows/build.yml/badge.svg)](https://github.com/jkl8808/skreeu1/actions/workflows/build.yml)

给 Snapmaker U1 那块 3.5"（480×320）屏幕写的第三方界面：LVGL 9.4 + C++17，
Material You 深色风格，直接对接打印机自带的 Moonraker / Klipper 扩展对象。

A Material You style touch UI for the Snapmaker U1's 3.5" panel, built with LVGL 9.4 and C++17.
It talks to Moonraker directly and keeps the stock services (unisrv / MQTT) running.

> 非官方项目，与 Snapmaker 无关联。会上传/替换屏幕上的 GUI 进程，动手前请先看完
> [风险与恢复](#风险与恢复)。目前只在 **U1 固件 2.1.0** 上实测过。

## 特性

- **4 页导航**：首页 / 打印 / 维护 / 设置，左侧悬浮胶囊导航（可用 `SKREEU1_RAIL=right` 换到右侧）
- **首页**：打印状态卡、热床卡（点开调温）、4 个喷嘴大卡片（温度、目标、耗材颜色/类型、NFC 标识）
- **喷嘴详情**：耗材卡、温度卡、自动进料、喷嘴参数（0.2/0.4/0.6/0.8 + 标准/高流）、载入/退料
- **调温浮层**：竖向滑块（松手才下发）、数字键盘、6 个可编辑快捷温度；喷嘴 `M104`、热床 `M140`
- **更换耗材**：品牌列 + 28 色调色板 + 型号列表，支持自定义品牌和自定义类型
- **打印页**：进度环、速度倍率、Z-offset、暂停/继续/取消
- **维护页**：净化器、换头次数、喷嘴、空打检测、异物检测
- **设置页**：设备信息（SN / 固件 / 喷嘴，从打印机读取）、7 项打印偏好、运行统计、灯光
- 所有写操作都是**乐观更新**：先用本地值显示，轮询确认不上就 8 秒后弹提示并回退
- 中文用 FreeType 运行时渲染，不预烘焙字库；整屏强制重绘约 2ms/帧，空闲 CPU ≈1%

## 截图

| 首页 | 喷嘴详情 | 调温 |
| --- | --- | --- |
| ![首页](docs/screenshots/home.png) | ![喷嘴详情](docs/screenshots/nozzle.png) | ![调温](docs/screenshots/temp.png) |
| **更换耗材** | **打印** | **维护** |
| ![更换耗材](docs/screenshots/materials.png) | ![打印](docs/screenshots/print.png) | ![维护](docs/screenshots/maintenance.png) |

设置页：[docs/screenshots/settings.png](docs/screenshots/settings.png)

> 设备信息（机型、SN、固件、喷嘴）优先从 Moonraker 的 `/machine/system_info` 读取，
> 和 Snapmaker Orca 用的是同一份数据；仓库里的 `assets/product_info.json` 只是离线开发时的回退示例。

## 风险与恢复

接管屏幕的方式是：把 skreeu1 只读 bind mount 到 `/usr/bin/gui`，再让 lmd 重新拉起 GUI 子进程。
**lmd / unisrv / 3893 MQTT / 7125 Moonraker 都不停**，所以官方 App 和切片软件仍然能连。

但请知悉：

- 需要 root SSH；不同固件版本路径可能不一样，升级固件后可能需要重新适配。
- 打印机重启后会回到**原厂 GUI**，需要再执行一次 `start.sh`。
- `stop.sh` 随时可以恢复原厂界面；它不会动你的打印配置和耗材数据。
- 本项目直接下发 G-code（例如 `M104` / `M140` / `AUTO_FEEDING`），请在有人的时候测试。
- 作者不对任何设备损坏、打印失败或保修问题负责，使用风险自负。

## 构建（桌面模拟器）

依赖：CMake ≥ 3.16、C++17 编译器、SDL2、FreeType、libpng、libcurl。
LVGL 以 submodule 形式固定在 `v9.4.0`：

```bash
git clone --recurse-submodules https://github.com/jkl8808/skreeu1.git
cd skreeu1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

或者用 Docker（不必装一堆 dev 包）：

```bash
docker build -t skreeu1-dev:1 -f docker/Dockerfile.dev .
docker run --rm --network host -v $PWD:/src -w /src skreeu1-dev:1 \
  bash -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j'
```

## 运行

桌面版是 SDL2 窗口 + 鼠标，可以连任何 Moonraker：

```bash
# 连真机（只读，最安全）
./build/skreeu1 --host 192.168.1.50 --port 7125 --readonly

# 可写模式（会真的下发 G-code）
./build/skreeu1 --host 192.168.1.50 --port 7125

# Xvfb + VNC，适合远程看 480×320 的实际效果
scripts/run_vnc.sh --host 192.168.1.50 --readonly

# 无头截图（等 1.8s 填状态后出图）
./build/skreeu1 --shot out.png --page 0 --host 192.168.1.50 --readonly
```

| 参数 / 环境变量 | 含义 |
| --- | --- |
| `--host` / `SKREEU1_HOST` | Moonraker 主机（默认 `127.0.0.1`） |
| `--port` / `SKREEU1_PORT` | Moonraker 端口（默认 `7125`） |
| `--readonly` / `SKREEU1_READONLY=1` | 只轮询，所有写命令都不下发 |
| `SKREEU1_FONTS` | 字体目录（默认 `assets/fonts`；设备上是 `/userdata/skreeu1/assets/fonts`） |
| `SKREEU1_PRODUCT_INFO` | 离线时回退用的 `product_info.json` 路径（默认 `/oem/printer_data/config/snapmaker/product_info.json`） |
| `SKREEU1_RAIL=right` | 导航栏换到右侧 |
| `--shot out.png --page N [--scroll N]` | 无头渲染一张 PNG |
| `--sheet N` | 截图时打开第 N 个喷嘴（0–3）的详情 |
| `--temp` | 配合 `--sheet`，打开喷嘴调温浮层 |
| `--bed` | 打开热床调温浮层 |
| `--keypad [--slot N]` | 打开数字键盘（`--slot` 为快捷温度槽位） |
| `--picker` | 打开材料选择 |
| `--nozzle` | 打开喷嘴参数弹层 |
| `--color` / `--toast` | 打开颜色面板 / 显示提示条 |
| `--bench N` | 整屏重绘基准（平均 ms/帧、等效 fps） |

## 交叉编译（aarch64 / Buildroot）

目标机是 glibc 2.38 + gcc 12。链接时用机器上真正在跑的
`libfreetype.so.6` / `libpng16.so.16` / `libcurl.so.4`，
`libstdc++` / `libgcc` 静态链入，产物约 1.4MB。

这些库有版权，**不放在仓库里**，先从自己的打印机拷一份：

```bash
scripts/fetch_sysroot.sh root@192.168.1.50
```

然后编译：

```bash
docker build -t skreeu1-cross:1 -f docker/Dockerfile.cross .
docker run --rm --user $(id -u):$(id -g) -e HOME=/tmp \
  -v $PWD:/src -w /src skreeu1-cross:1
```

产物在 `build-aarch64/skreeu1`。

## 安装到打印机

```bash
# 拷贝二进制 + 字体 + 启停脚本到 /userdata/skreeu1/
scripts/install_device.sh root@192.168.1.50

# 只更新二进制（字体已经拷过）
scripts/install_device.sh root@192.168.1.50 --no-fonts

# 装完直接接管屏幕
scripts/install_device.sh root@192.168.1.50 --start
```

在打印机上手动控制：

```sh
/userdata/skreeu1/start.sh    # skreeu1 接管屏幕，官方服务继续运行
/userdata/skreeu1/stop.sh     # 恢复原厂 GUI
SKREEU1_READONLY=1 /userdata/skreeu1/start.sh   # 只读模式
```

`start.sh` 的流程是：先让 lmd 正常启动（原厂 GUI，官方服务全部起来），
再把 skreeu1 只读 bind 到 `/usr/bin/gui`，最后杀掉当前 gui 子进程让 lmd 拉起 skreeu1。

程序会每 2 秒清一次 `fbcon` / backlight 的空闲 blank，避免系统把面板断电后
「触摸有反应但屏幕是黑的」；不会改 `brightness`，保留原厂亮度设置。

设备上没有 SDL 视频驱动，所以真机走 LVGL 自带的 `lv_linux_fbdev`
（`/dev/fb0`，480×320×32）+ `lv_evdev`（`/dev/input/event0`）。
可以用 `SKREEU1_FB` / `SKREEU1_TOUCH` 覆盖设备节点。

## 操作说明

### 首页

- 点热床卡 → 热床调温浮层
- 点任意喷嘴卡 → 喷嘴详情
- 状态卡显示当前打印进度、文件名、用时、耗材

### 喷嘴详情

左侧耗材卡（颜色、类型、厂商、NFC 卡型与建议温度），右侧温度卡，下面自动进料和喷嘴参数，
底部是载入 / 退料 / 完成。

- 点温度卡 → 调温浮层
- 点「喷嘴」→ 改直径（0.2/0.4/0.6/0.8）和流量（标准/高流），打印中不允许改
- 点耗材卡（非官方 RFID）→ 更换耗材：左侧品牌列、左下颜色块（28 色）、右侧型号列表

### 调温浮层

- 竖向滑块拖动时数字实时跟随，**松手才下发** `M104`（喷嘴）或 `M140`（热床）
- 点大字数字 → 数字键盘手动输入
- 6 个快捷温度：点击立即设温，长按弹键盘改数值
- 滑块上限：喷嘴 300°C，热床 120°C

### 配置文件

设备上都在 `/userdata/skreeu1/`，桌面版默认读当前目录：

| 文件 | 内容 | 覆盖变量 |
| --- | --- | --- |
| `skreeu1_brands.cfg` | 品牌列表，一行一个（`generic` 显示为「通用」） | `SKREEU1_BRANDS` |
| `skreeu1_types.cfg` | 自定义类型，一行 `TYPE` 或 `TYPE SUBTYPE`，`#` 注释 | `SKREEU1_TYPES` |
| `skreeu1_temps.cfg` | 喷嘴 6 个快捷温度 | `SKREEU1_TEMPS` |
| `skreeu1_bed_temps.cfg` | 热床 6 个快捷温度 | `SKREEU1_BED_TEMPS` |

改品牌/类型不需要重启，下次打开更换界面时重读；快捷温度启动时读。

## 自定义耗材小工具

`tools/filament_tool.py` 在电脑上运行：通过 SSH 改打印机的品牌/类型列表，
通过 Moonraker 把某个喷嘴设成这卷耗材，可选同时生成 Snapmaker Orca 的用户预设。

```bash
export SKREEU1_HOST=192.168.1.50       # 或者每次传 --host

# 看现有品牌/类型/喷嘴状态
python3 tools/filament_tool.py list

# 加品牌 + 类型，设到 T3，并生成 Orca 的 4 个喷嘴预设
python3 tools/filament_tool.py add --vendor Kexcelled --type ABS --sub Basic \
    --color "#FFFFFF" --tool 2 --orca

# 不带参数进入交互模式
python3 tools/filament_tool.py
```

| 环境变量 | 含义 |
| --- | --- |
| `SKREEU1_HOST` | 打印机地址（必填） |
| `SKREEU1_SSH_USER` | SSH 用户，默认 `root` |
| `SKREEU1_SSH_PASS` | SSH 密码；不填会在运行时提示输入 |
| `SKREEU1_PORT` | Moonraker 端口，默认 `7125` |

打印机侧和切片软件侧的数据怎么对应，见
[docs/custom_filament.md](docs/custom_filament.md)。

## 原厂功能是怎么接进来的

没有逆向闭源 GUI，只是把原厂已经在用的 Klipper 对象和 G-code 命令接到 LVGL 上。
完整的对象/命令映射、固件里的约束（官方 RFID 不可改、打印中不能改喷嘴参数等）见
[docs/compat.md](docs/compat.md)。

## 目录结构

```
src/            UI、主题、Moonraker 客户端、fbdev/evdev 入口
assets/fonts/   Noto Sans CJK SC + Material Design Icons
device/         打印机上的 start.sh / stop.sh
scripts/        交叉编译、安装、取 sysroot、VNC、截图
tools/          filament_tool.py（自定义耗材）
docs/           兼容层说明、自定义耗材说明、截图
lvgl/           LVGL 9.4.0（git submodule）
third_party/    nlohmann/json
```

## 已知限制

- 摄像头 / 延时摄影、OTA、局域网访问码、云端账号还没接（这些在 unisrv 的 MQTT 里）。
- 打印机必须处于 LAN 模式，Snapmaker Orca 才能直接连上。
- 只在 U1 / 固件 2.1.0 上验证过；其他固件版本的对象名和命令可能不同。
- 界面固定按 480×320 设计，没有做其他分辨率适配。

## 许可证

本项目代码使用 [MIT](LICENSE)。第三方组件（LVGL、nlohmann/json、Noto CJK、Material Design Icons）
按各自许可证分发，详见 [THIRD_PARTY.md](THIRD_PARTY.md)。

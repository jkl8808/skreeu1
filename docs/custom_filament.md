# 自定义耗材：打印机侧 + 切片软件侧

目标：让打印机（skreeu1）里设置的品牌/类型/颜色，和 Snapmaker Orca 里的耗材预设对应起来，
这样设备页能看到真实耗材，切片列表里也能直接选到它。

两边是**两套独立的数据**，靠字段约定对应：

| 打印机 `print_task_config` | Orca 用户预设 | 说明 |
| --- | --- | --- |
| `filament_vendor`（如 `Kexcelled`） | `filament_vendor: ["Kexcelled"]` | 厂商名，用于匹配预设 |
| `filament_type`（如 `ABS`） | `filament_type: ["ABS"]` | 材料类型 |
| `filament_sub_type`（如 `Basic`） | 预设名里包含 `Basic`（或 `filament_subtype`） | 子类型/系列 |
| `filament_color_rgba`（如 `FFFFFFFF`） | `default_filament_colour: ["#FFFFFF"]` | 颜色，同步时带到槽位 |
| `extruder*.nozzle_diameter` | `compatible_printers` / 机型喷嘴变体 | 0.2/0.4/0.6/0.8 分开 |
| `filament_exist`（进料传感器） | 槽位是否参与同步 | 没料的槽不会同步 |
| `filament_official`（官方 RFID） | 无对应 | 官方耗材固件不允许改 |

---

## 一、打印机侧（skreeu1）

### 1. 加品牌

文件：`/userdata/skreeu1/skreeu1_brands.cfg`，一行一个品牌：

```
generic
Snapmaker
Polymaker
eSun
Kexcelled
```

`generic` 会显示成“通用”。改完不需要重启，打开“更换耗材”界面时重新读取。

### 2. 加类型/子类型

文件：`/userdata/skreeu1/skreeu1_types.cfg`，一行一个，`TYPE` 或 `TYPE SUBTYPE`，
`#` 开头为注释：

```
# 自定义类型
ABS Basic
PLA Silk
PA-CF CF10
```

这些会出现在右侧耗材列表的**最顶部**，并带橙色「自定义」角标。

### 3. 在界面上设置某个喷嘴的耗材

首页 → 点 T1–T4 → 左侧耗材卡（非官方 RFID 才能点）→
选品牌 → 左下角选颜色 → 点右侧型号。下发：

```
SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER=<n> \
  VENDOR=<品牌> FILAMENT_TYPE=<类型> FILAMENT_SUBTYPE=<子类型> \
  FILAMENT_COLOR_RGBA=RRGGBBAA
```

写入位置：

- 运行时：Klipper 对象 `print_task_config`
- 持久化：`/oem/printer_data/config/snapmaker/print_task.json`
- 上次打印快照：`print_task_2.json`

### 4. 打印机侧参数从哪来

- `snapmaker/filament_parameters.json`：厂商/类型/子类型目录（界面列表用）。
- `filament_para_standard_02/04/06/08.json`、`filament_para_high_flow_04.json`：
  Klipper 实际用的温度/流量参数，键是 `vendor_type_sub_参数`。
  自定义品牌没有条目时回退到 `generic_类型_子类型`，所以是安全的。
- 这些 `system` / OTA 文件由固件管理，**不要手改**，升级会被覆盖。

### 5. 相关 UI 配置文件

| 文件 | 内容 |
| --- | --- |
| `/userdata/skreeu1/skreeu1_brands.cfg` | 品牌列表 |
| `/userdata/skreeu1/skreeu1_types.cfg` | 自定义类型列表 |
| `/userdata/skreeu1/skreeu1_temps.cfg` | 6 个快捷温度（所有喷嘴共用） |

可用 `SKREEU1_BRANDS` / `SKREEU1_TYPES` / `SKREEU1_TEMPS` 覆盖路径。

---

## 二、电脑侧（Snapmaker Orca 2.4.x）

### 1. 用户预设放哪里

| 系统 | 目录 |
| --- | --- |
| Linux | `~/.config/Snapmaker_Orca/user/default/filament/` |
| Windows | `%APPDATA%\Snapmaker_Orca\user\default\filament\` |
| macOS | `~/Library/Application Support/Snapmaker_Orca/user/default/filament/` |

**不要改** `system/`、`ota/`、`resources/profiles/` 里的系统耗材：
它们由 OTA 配置包管理，会在下次“安装配置包”时被覆盖。

### 2. 一个用户耗材预设的最小结构

每个喷嘴一个文件，文件名 = `name` + `.json`：

```json
{
  "type": "filament",
  "from": "User",
  "instantiation": "true",
  "name": "Kexcelled ABS Basic @U1 0.4 nozzle",
  "filament_settings_id": ["Kexcelled ABS Basic @U1 0.4 nozzle"],
  "setting_id": "Kx9aBcDeFgHiJkL1",
  "version": "2.4.0.2",
  "inherits": "Snapmaker ABS @U1 0.4 nozzle",
  "filament_vendor": ["Kexcelled"],
  "filament_type": ["ABS"],
  "default_filament_colour": ["#FFFFFF"],
  "compatible_printers": ["Snapmaker U1 (0.4 nozzle)"]
}
```

要点：

- `from` 必须是 `"User"`（大写 U）；
- 必须有 `filament_settings_id`，否则会被**静默忽略**（这是最容易踩的坑）；
- `setting_id` 每个预设唯一（随便一个不重复的字符串即可）；
- `inherits` 用同喷嘴的系统预设，如 `Snapmaker ABS @U1 0.4 nozzle`；
- `compatible_printers` 必须是完整的机型变体名，如 `Snapmaker U1 (0.4 nozzle)`；
- 0.2/0.4/0.6/0.8 各一份，`inherits` 和 `compatible_printers` 跟着改；
- 颜色 `default_filament_colour` 用 `#RRGGBB`。

### 3. 在 Orca 里选它

重启 Orca（用户预设只在启动时扫描）→ `Prepare` → 左侧 `Filaments` 下拉 →
`User presets` → `Custom` → `Kexcelled ABS Basic ...`。

### 4. 设备页 / 同步

- `Device` → 往下滚 → `Filament`：显示打印机发来的 4 个槽位（品牌/类型/颜色/匹配到的预设）。
- 该区域的刷新/同步按钮 `Sync Filament Information`：把打印机的**类型和颜色**
  同步到当前工程的 4 个耗材槽。
- 匹配规则：先按 `filament_vendor` + `filament_type` 找预设并尽量按颜色挑，
  找不到该品牌就回退到 Orca 自带的 `Generic <类型>` 预设。
  所以自定义品牌要么在 Orca 里有同名 vendor 的预设，要么就显示 Generic。

> 同步是单向的：打印机 → 切片。Orca 不会反过来改打印机的耗材配置。

---

## 三、小工具：一条命令搞定两边

`tools/filament_tool.py`（在电脑上跑，Python 3.8+，通过本机 `ssh` 连打印机）：

```bash
export SKREEU1_HOST=192.168.1.50      # 打印机地址，也可以每次传 --host

# 看现状
python3 tools/filament_tool.py list

# 加品牌+类型，设到 T3，并生成 Orca 的 4 个喷嘴预设
python3 tools/filament_tool.py add --vendor Kexcelled --type ABS --sub Basic \
    --color "#FFFFFF" --tool 2 --orca

# 只加品牌 / 只加类型 / 只设喷嘴
python3 tools/filament_tool.py brand add Kexcelled
python3 tools/filament_tool.py type add "ABS Basic"
python3 tools/filament_tool.py set --tool 2 --vendor Kexcelled --type ABS --sub Basic --color "#FFFFFF"

# 交互模式
python3 tools/filament_tool.py
```

- 品牌/类型写到打印机的 `/userdata/skreeu1/skreeu1_brands.cfg` / `skreeu1_types.cfg`；
- `set` 直接走 Moonraker `SET_PRINT_FILAMENT_CONFIG`（自动跳过官方 RFID 槽）；
- `--orca` 会写 `~/.config/Snapmaker_Orca/user/default/filament/` 下的用户预设，
  并按 0.2/0.4/0.6/0.8 自动找对应的 `Snapmaker <类型> @U1 <喷嘴> nozzle` 作为父预设；
- 带空格的品牌（如 `Bambu Lab`）会自动加引号，避免 G-code 参数被空格截断。

连接参数：`SKREEU1_HOST`（必填）/ `SKREEU1_PORT` / `SKREEU1_SSH_USER` / `SKREEU1_SSH_PASS`
（密码不填会在运行时提示输入）。

## 四、一次完整的对接流程

1. **打印机**：`skreeu1_brands.cfg` 加品牌，`skreeu1_types.cfg` 加 `ABS Basic`。
2. **打印机**：首页点喷嘴 → 选 Kexcelled → 选颜色 → 点 `ABS · Basic`。
3. **电脑**：把 4 个用户预设 JSON 放进 Orca 的 `user/default/filament/`。
4. **电脑**：重启 Orca，`Prepare` 里就能选到 `Kexcelled ABS Basic @U1 <喷嘴>`；
   `Device → Filament` 能看到打印机的槽位，点同步把类型/颜色带进工程。
5. 切片、上传、打印。

### 常见问题

| 现象 | 原因 |
| --- | --- |
| Orca 列表里看不到自定义耗材 | 缺 `filament_settings_id`、`from` 不是 `User`、或没重启 |
| 设备页显示 Generic ABS | Orca 里没有对应厂商预设，回退到 Generic |
| 打印机的品牌/类型改了，Orca 没变 | 同步是打印机→切片单向；且需要刷新/重连 |
| 自定义类型没出现在 skreeu1 列表 | `skreeu1_types.cfg` 格式不对（一行 `TYPE` 或 `TYPE SUBTYPE`） |
| 改完 skreeu1 配置没生效 | 品牌/类型每次打开界面重读；快捷温度启动时读；必要时重启 skreeu1 |
| Orca 连不上打印机 | 打印机必须在 LAN 模式；账号登录只解决云端身份 |

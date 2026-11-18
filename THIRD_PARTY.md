# 第三方组件

本项目源码（`src/`、`scripts/`、`tools/`、`device/`、文档）使用 [MIT](LICENSE)。
下面这些第三方组件按各自许可证分发。

| 组件 | 位置 | 许可证 |
| --- | --- | --- |
| [LVGL](https://github.com/lvgl/lvgl) 9.4.0 | `lvgl/`（git submodule） | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 | `third_party/hv/json.hpp` | MIT |
| [Noto Sans CJK SC](https://github.com/notofonts/noto-cjk) | `assets/fonts/NotoSansCJKsc-*.otf` | SIL OFL 1.1（见 `assets/fonts/LICENSE-OFL-1.1.txt`） |
| [Material Design Icons](https://pictogrammers.com/docs/general/license/) | `assets/fonts/materialdesignicons-webfont.ttf` | Apache 2.0（见 `assets/fonts/LICENSE-Apache-2.0.txt`） |

## 说明

- `sysroot/lib/` 里的库**不进仓库**：它们是打印机固件里的
  `libfreetype` / `libpng` / `libcurl`，只用于交叉链接。
  用 `scripts/fetch_sysroot.sh <user@host>` 从自己的机器上拷。
- 固件里的 `filament_parameters.json`、`product_info.json` 等数据属于 Snapmaker，
  仓库里只保留了脱敏后的示例字段，不含真实序列号。
- Snapmaker、Snapmaker U1、Orca 等名称和商标归各自所有者。本项目是非官方项目，
  与 Snapmaker 没有关联。

#!/usr/bin/env python3
"""
skreeu1 自定义耗材小工具

给打印机（skreeu1）添加自定义品牌 / 类型，并把某个喷嘴设成这卷耗材；
可选同时在 Snapmaker Orca 的用户目录里生成对应的切片预设。

用法：
  python3 tools/filament_tool.py list
  python3 tools/filament_tool.py brand add Kexcelled
  python3 tools/filament_tool.py type add "ABS Basic"
  python3 tools/filament_tool.py set --tool 3 --vendor Kexcelled --type ABS --sub Basic --color "#FFFFFF"
  python3 tools/filament_tool.py add --vendor Kexcelled --type ABS --sub Basic --color "#FFFFFF" --tool 3 --orca
  python3 tools/filament_tool.py            # 交互模式

连接参数（环境变量或参数）：
  SKREEU1_HOST       打印机地址（必填，或用 --host）
  SKREEU1_SSH_USER   SSH 用户，默认 root
  SKREEU1_SSH_PASS   SSH 密码（不填则运行时提示输入）
  SKREEU1_PORT       Moonraker 端口，默认 7125

依赖：本机 ssh（密码通过 SSH_ASKPASS 传入）、Python 3.8+。
"""

import argparse
import getpass
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

BRANDS_PATH = "/userdata/skreeu1/skreeu1_brands.cfg"
TYPES_PATH = "/userdata/skreeu1/skreeu1_types.cfg"

DEFAULT_BRANDS = [
    "generic", "Snapmaker", "Polymaker", "eSun", "SUNLU", "Bambu Lab",
    "Prusament", "Creality", "Anycubic", "Elegoo", "Overture", "Jayo",
]

B62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def b62(data: bytes, length: int) -> str:
    n = int.from_bytes(hashlib.sha256(data).digest(), "big")
    out = []
    for _ in range(length):
        out.append(B62[n % 62])
        n //= 62
    return "".join(out)


def die(msg: str) -> "NoReturn":
    print("错误:", msg, file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------- SSH

class Ssh:
    def __init__(self, host: str, user: str, password: str):
        self.host = host
        self.user = user
        self.password = password
        self._tmp = None

    def _askpass(self) -> str:
        if self._tmp is None:
            self._tmp = tempfile.mkdtemp(prefix="skreeu1-askpass-")
            path = os.path.join(self._tmp, "askpass.sh")
            with open(path, "w") as f:
                f.write('#!/bin/sh\nprintf \'%s\\n\' "$SKREEU1_SSH_PASS"\n')
            os.chmod(path, 0o700)
        return os.path.join(self._tmp, "askpass.sh")

    def run(self, remote_cmd: str, stdin: str = "") -> str:
        env = os.environ.copy()
        env["SKREEU1_SSH_PASS"] = self.password
        env["SSH_ASKPASS"] = self._askpass()
        env["SSH_ASKPASS_REQUIRE"] = "force"
        env.pop("DISPLAY", None)
        cmd = [
            "ssh",
            "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null",
            "-o", "PreferredAuthentications=password",
            "-o", "PubkeyAuthentication=no",
            "-o", "NumberOfPasswordPrompts=1",
            "-o", "LogLevel=ERROR",
            "-o", "ConnectTimeout=8",
            f"{self.user}@{self.host}",
            remote_cmd,
        ]
        p = subprocess.run(cmd, input=stdin, text=True, capture_output=True,
                           env=env, start_new_session=True)
        if p.returncode != 0:
            die(f"SSH 失败: {p.stderr.strip() or p.returncode}")
        return p.stdout

    def python(self, code: str) -> str:
        return self.run("python3 -", code)

    def close(self):
        if self._tmp:
            shutil.rmtree(self._tmp, ignore_errors=True)


# ---------------------------------------------------------------- printer files

def brand_add(ssh: Ssh, name: str) -> list:
    name = name.strip()
    if not name:
        die("品牌不能为空")
    code = f"""
import json, os
p = {BRANDS_PATH!r}
defaults = {json.dumps(DEFAULT_BRANDS, ensure_ascii=False)}
name = {json.dumps(name, ensure_ascii=False)}
lines = []
if os.path.exists(p):
    lines = [x.strip() for x in open(p, encoding='utf-8') if x.strip()]
else:
    lines = list(defaults)
if not any(x.lower() == name.lower() for x in lines):
    lines.append(name)
with open(p, 'w', encoding='utf-8') as f:
    f.write('\\n'.join(lines) + '\\n')
print(json.dumps(lines, ensure_ascii=False))
"""
    out = ssh.python(code)
    return json.loads(out.strip().splitlines()[-1])


def type_add(ssh: Ssh, entry: str) -> list:
    entry = " ".join(entry.split())
    if not entry:
        die("类型不能为空")
    code = f"""
import json, os
p = {TYPES_PATH!r}
entry = {json.dumps(entry, ensure_ascii=False)}
lines = []
if os.path.exists(p):
    lines = [x.rstrip('\\n') for x in open(p, encoding='utf-8')]
norm = [x.strip() for x in lines]
if not any(x.lower() == entry.lower() for x in norm):
    lines.append(entry)
with open(p, 'w', encoding='utf-8') as f:
    f.write('\\n'.join(lines) + '\\n')
print(json.dumps([x for x in (y.strip() for y in lines) if x and not x.startswith('#')], ensure_ascii=False))
"""
    out = ssh.python(code)
    return json.loads(out.strip().splitlines()[-1])


def read_files(ssh: Ssh):
    code = f"""
import json, os
def read(p):
    if not os.path.exists(p):
        return None
    return [x.strip() for x in open(p, encoding='utf-8')
            if x.strip() and not x.strip().startswith('#')]
print(json.dumps({{'brands': read({BRANDS_PATH!r}), 'types': read({TYPES_PATH!r})}}, ensure_ascii=False))
"""
    out = ssh.python(code)
    return json.loads(out.strip().splitlines()[-1])


# ---------------------------------------------------------------- Moonraker

def api(host: str, port: int, path: str) -> dict:
    url = f"http://{host}:{port}{path}"
    try:
        with urllib.request.urlopen(url, timeout=6) as r:
            return json.loads(r.read().decode())
    except Exception as e:
        die(f"Moonraker 请求失败: {e}")


def send_gcode(host: str, port: int, script: str) -> dict:
    url = (f"http://{host}:{port}/printer/gcode/script?script="
           + urllib.parse.quote(script, safe=""))
    req = urllib.request.Request(url, data=b"", method="POST")
    try:
        with urllib.request.urlopen(req, timeout=8) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        die(f"固件拒绝: {e.read().decode(errors='replace')}")
    except Exception as e:
        die(f"Moonraker 请求失败: {e}")


def slot_info(host: str, port: int):
    d = api(host, port, "/printer/objects/query?print_task_config")
    c = d["result"]["status"]["print_task_config"]
    return [{
        "tool": i,
        "vendor": c["filament_vendor"][i],
        "type": c["filament_type"][i],
        "sub": c["filament_sub_type"][i],
        "color": c["filament_color_rgba"][i],
        "official": c["filament_official"][i],
    } for i in range(4)]


def q(s: str) -> str:
    # Klipper 的 G-code 参数用空格分隔，带空格的字符串要加引号
    return f'"{s}"' if any(ch.isspace() for ch in s) else s


def apply_slot(host: str, port: int, tool: int, vendor: str, ftype: str, sub: str,
               rgba: str):
    slots = slot_info(host, port)
    if slots[tool]["official"]:
        die(f"T{tool + 1} 是官方 RFID 耗材，固件不允许改")
    sub = sub or "generic"
    cmd = (f"SET_PRINT_FILAMENT_CONFIG CONFIG_EXTRUDER={tool} "
           f"VENDOR={q(vendor)} FILAMENT_TYPE={q(ftype)} "
           f"FILAMENT_SUBTYPE={q(sub)} FILAMENT_COLOR_RGBA={rgba}")
    r = send_gcode(host, port, cmd)
    if r.get("result") != "ok":
        die(f"写入失败: {r}")
    print(f"✓ T{tool + 1} = {vendor} {ftype} {sub} #{rgba[:6]}")


# ---------------------------------------------------------------- Orca presets

def orca_dir() -> str:
    home = os.path.expanduser("~")
    if sys.platform.startswith("win"):
        base = os.environ.get("APPDATA", os.path.join(home, "AppData", "Roaming"))
        return os.path.join(base, "Snapmaker_Orca", "user", "default", "filament")
    if sys.platform == "darwin":
        return os.path.join(home, "Library", "Application Support", "Snapmaker_Orca",
                            "user", "default", "filament")
    return os.path.join(home, ".config", "Snapmaker_Orca", "user", "default", "filament")


def orca_known_filaments() -> set:
    """尽量从 Orca 的系统索引里读出可继承的预设名。"""
    names = set()
    home = os.path.expanduser("~")
    candidates = [
        os.path.join(home, ".config", "Snapmaker_Orca", "system", "Snapmaker.json"),
        os.path.join(home, "Applications", "Snapmaker_Orca", "resources", "profiles", "Snapmaker.json"),
    ]
    for p in candidates:
        try:
            d = json.load(open(p, encoding="utf-8"))
            for x in d.get("filament_list", []):
                names.add(x["name"])
        except Exception:
            pass
    return names


def orca_parent(ftype: str, nz: str, known: set) -> str:
    for cand in (f"Snapmaker {ftype} @U1 {nz} nozzle",
                 f"Generic {ftype} @U1 {nz} nozzle",
                 f"Snapmaker {ftype} @U1 base",
                 f"Generic {ftype} @U1 base"):
        if not known or cand in known:
            return cand
    return f"fdm_filament_{ftype.lower()}"


def make_orca_presets(vendor: str, ftype: str, sub: str, color: str,
                      nozzles: list, target_dir: str | None = None) -> list:
    d = target_dir or orca_dir()
    os.makedirs(d, exist_ok=True)
    known = orca_known_filaments()
    written = []
    base_name = f"{vendor} {ftype}" + (f" {sub}" if sub else "")
    for nz in nozzles:
        name = f"{base_name} @U1 {nz} nozzle"
        sid = b62(f"{vendor}/{ftype}/{sub}/{nz}".encode(), 16)
        obj = {
            "type": "filament",
            "from": "User",
            "instantiation": "true",
            "name": name,
            "filament_settings_id": [name],
            "setting_id": sid,
            "version": "2.4.0.2",
            "inherits": orca_parent(ftype, nz, known),
            "filament_vendor": [vendor],
            "filament_type": [ftype],
            "default_filament_colour": [color],
            "compatible_printers": [f"Snapmaker U1 ({nz} nozzle)"],
        }
        p = os.path.join(d, name + ".json")
        with open(p, "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=2, ensure_ascii=False)
        written.append(p)
    return written


# ---------------------------------------------------------------- commands

def norm_color(c: str) -> str:
    c = (c or "").strip().lstrip("#")
    if len(c) == 6:
        c += "FF"
    if len(c) != 8 or any(ch not in "0123456789abcdefABCDEF" for ch in c):
        die("颜色格式应为 #RRGGBB 或 RRGGBBAA")
    return c.upper()


def cmd_list(args):
    ssh = Ssh(args.host, args.ssh_user, args.ssh_pass)
    try:
        data = read_files(ssh)
    finally:
        ssh.close()
    brands = data["brands"] or ["(未创建，使用内置默认)"]
    types = data["types"] or ["(空)"]
    print("品牌:", ", ".join(brands))
    print("类型:", ", ".join(types))
    print("\n当前喷嘴:")
    for s in slot_info(args.host, args.port):
        off = " · 官方" if s["official"] else ""
        print(f"  T{s['tool'] + 1}: {s['vendor']} {s['type']} "
              f"{s['sub'] or 'generic'} #{s['color'][:6]}{off}")


def cmd_brand(args):
    ssh = Ssh(args.host, args.ssh_user, args.ssh_pass)
    try:
        lines = brand_add(ssh, args.name)
    finally:
        ssh.close()
    print(f"✓ 品牌列表: {', '.join(lines)}")


def cmd_type(args):
    ssh = Ssh(args.host, args.ssh_user, args.ssh_pass)
    try:
        lines = type_add(ssh, args.entry)
    finally:
        ssh.close()
    print(f"✓ 类型列表: {', '.join(lines)}")


def cmd_set(args):
    apply_slot(args.host, args.port, args.tool, args.vendor, args.type,
               args.sub or "", norm_color(args.color))


def cmd_add(args):
    ssh = Ssh(args.host, args.ssh_user, args.ssh_pass)
    try:
        brand_add(ssh, args.vendor)
        type_add(ssh, f"{args.type} {args.sub}".strip())
    finally:
        ssh.close()
    print(f"✓ 已写入打印机品牌/类型: {args.vendor} / {args.type} {args.sub}")
    if args.tool is not None:
        apply_slot(args.host, args.port, args.tool, args.vendor, args.type,
                   args.sub or "", norm_color(args.color))
    if args.orca:
        nozzles = [x.strip() for x in args.nozzles.split(",") if x.strip()]
        files = make_orca_presets(args.vendor, args.type, args.sub or "",
                                  "#" + norm_color(args.color)[:6], nozzles,
                                  args.orca_dir)
        print("✓ 已生成 Orca 用户预设:")
        for p in files:
            print("   ", p)
        print("  重启 Orca 后可在 Prepare → Filaments → User presets → Custom 里选择")


def interactive(args):
    print("=== 给打印机添加自定义耗材 ===")
    vendor = input("品牌 (如 Kexcelled): ").strip()
    if not vendor:
        die("品牌不能为空")
    ftype = input("类型 (如 ABS): ").strip() or "PLA"
    sub = input("子类型/系列 (可空，如 Basic): ").strip()
    color = input("颜色 #RRGGBB (默认 #FFFFFF): ").strip() or "#FFFFFF"
    tool = input("设置到喷嘴 1-4 (回车跳过): ").strip()
    orca = input("同时生成 Orca 预设? (y/N): ").strip().lower() in ("y", "yes")

    args.vendor, args.type, args.sub, args.color = vendor, ftype, sub, color
    args.tool = (int(tool) - 1) if tool else None
    args.orca = orca
    cmd_add(args)


def main():
    ap = argparse.ArgumentParser(description="给 skreeu1 打印机添加自定义耗材")
    ap.add_argument("--host", default=os.environ.get("SKREEU1_HOST"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("SKREEU1_PORT", "7125")))
    ap.add_argument("--ssh-user", default=os.environ.get("SKREEU1_SSH_USER", "root"))
    ap.add_argument("--ssh-pass", default=os.environ.get("SKREEU1_SSH_PASS"))
    sub = ap.add_subparsers(dest="cmd")

    p = sub.add_parser("list", help="列出现有品牌/类型和喷嘴耗材")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("brand", help="品牌管理")
    b = p.add_subparsers(dest="action", required=True)
    pb = b.add_parser("add", help="添加品牌")
    pb.add_argument("name")
    pb.set_defaults(func=cmd_brand)

    p = sub.add_parser("type", help="类型管理")
    t = p.add_subparsers(dest="action", required=True)
    pt = t.add_parser("add", help='添加类型，如 "ABS Basic"')
    pt.add_argument("entry")
    pt.set_defaults(func=cmd_type)

    p = sub.add_parser("set", help="把某个喷嘴设成指定耗材")
    p.add_argument("--tool", type=int, required=True, choices=[0, 1, 2, 3])
    p.add_argument("--vendor", required=True)
    p.add_argument("--type", required=True)
    p.add_argument("--sub", default="")
    p.add_argument("--color", default="#FFFFFF")
    p.set_defaults(func=cmd_set)

    p = sub.add_parser("add", help="品牌+类型一起加，可选设喷嘴/生成 Orca 预设")
    p.add_argument("--vendor", required=True)
    p.add_argument("--type", required=True)
    p.add_argument("--sub", default="")
    p.add_argument("--color", default="#FFFFFF")
    p.add_argument("--tool", type=int, choices=[0, 1, 2, 3])
    p.add_argument("--orca", action="store_true", help="同时写 Orca 用户预设")
    p.add_argument("--nozzles", default="0.2,0.4,0.6,0.8")
    p.add_argument("--orca-dir", default=None)
    p.set_defaults(func=cmd_add)

    args = ap.parse_args()
    if not args.host:
        die("请用 --host 或环境变量 SKREEU1_HOST 指定打印机地址")
    if not args.ssh_pass:
        args.ssh_pass = getpass.getpass(f"{args.ssh_user}@{args.host} 的 SSH 密码: ")
    if not getattr(args, "cmd", None):
        interactive(args)
    else:
        args.func(args)


if __name__ == "__main__":
    main()

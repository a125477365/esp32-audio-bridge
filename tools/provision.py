#!/usr/bin/env python3
"""
ESP32 Audio Bridge 串口配网/状态查询工具

用法:
  配置 WiFi:  python3 tools/provision.py --port /dev/cu.usbmodem1234561 --ssid "我的WiFi" --password "密码"
  查询状态:   python3 tools/provision.py --port /dev/cu.usbmodem1234561 --status

不带 --port 时自动选择第一个 usbmodem/usbserial 串口。
"""
import argparse
import glob
import json
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial: 请运行 pip3 install --user pyserial")


def autodetect_port():
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/cu.wchusbserial*"):
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    sys.exit("未找到 ESP32 串口，请用 --port 指定")


def send_cmd(port, payload, wait_s=3.0, reopen_wait=0):
    ser = serial.Serial(port, 115200, timeout=0.5)
    try:
        ser.reset_input_buffer()
        ser.write((json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8"))
        ser.flush()
        deadline = time.time() + wait_s
        buf = b""
        while time.time() < deadline:
            buf += ser.read(4096)
            for line in buf.decode("utf-8", errors="replace").splitlines():
                line = line.strip()
                if line.startswith("{") and '"ok"' in line:
                    try:
                        return json.loads(line)
                    except json.JSONDecodeError:
                        pass
        return None
    finally:
        ser.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None)
    ap.add_argument("--ssid")
    ap.add_argument("--password", default="")
    ap.add_argument("--status", action="store_true")
    ap.add_argument("--reset", action="store_true")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    print(f"串口: {port}")

    if args.status:
        resp = send_cmd(port, {"cmd": "status"})
        if resp:
            print(json.dumps(resp, ensure_ascii=False, indent=2))
            if resp.get("state") == "working":
                print(f"\n✅ 设备工作中，请把后端 config.json 的 esp32.host 设为: {resp.get('ip')}")
        else:
            print("⚠️ 设备未响应（可能正在连接 WiFi，请等 30 秒后重试）")
        return

    if args.reset:
        resp = send_cmd(port, {"cmd": "reset"})
        print("已发送重置命令" if resp else "设备未响应")
        return

    if not args.ssid:
        ap.error("需要 --ssid（或使用 --status / --reset）")

    resp = send_cmd(port, {"cmd": "setWifi", "ssid": args.ssid, "password": args.password})
    if not resp or not resp.get("ok"):
        sys.exit("⚠️ 配网命令未确认，请重试")
    print(f"✅ WiFi 配置已保存（{args.ssid}），设备正在重启并连接...")

    # 等设备重启并连上 WiFi，然后查状态拿 IP
    time.sleep(18)
    for _ in range(5):
        resp = send_cmd(port, {"cmd": "status"})
        if resp and resp.get("state") == "working":
            print(f"✅ 已连接！设备 IP: {resp.get('ip')}，UDP 端口: {resp.get('port')}")
            print(f"请把后端 config.json 的 esp32.host 设为 {resp.get('ip')}（或在网页设置页修改）")
            return
        time.sleep(5)
    print("⚠️ 设备尚未进入工作状态，请运行 --status 复查（WiFi 密码错误时设备会停在配网模式）")


if __name__ == "__main__":
    main()

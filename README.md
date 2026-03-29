# ESP32 WiFi to S/PDIF Audio Bridge

High-fidelity wireless digital audio interface supporting up to 192kHz/32bit audio with low-jitter APLL clock.

## Features

- **Hi-Fi Audio**: Up to 192kHz/32bit PCM streaming
- **Low Jitter**: ESP32 APLL (Audio PLL) for precision clock
- **Web Configuration**: Easy setup via captive portal
- **Anti-Jitter Buffer**: Configurable ring buffer (50-500ms)
- **Flexible Network**: DHCP or static IP configuration

## Hardware

### Components
- ESP32-WROOM-32 development board
- I2S to S/PDIF module (CS8406 or DP7406)
- RCA coaxial cable

### Wiring

| ESP32 Pin | S/PDIF Module |
|-----------|---------------|
| GPIO 25 | BCK (Bit Clock) |
| GPIO 33 | WS/LRCLK (Word Select) |
| GPIO 26 | DATA |
| GND | GND |

## Software

### Build Requirements
- PlatformIO (recommended) or Arduino IDE
- ESP32 board support package

### Building with PlatformIO

#### 安装 PlatformIO
```bash
pip install platformio
```

#### 上传固件和文件系统（首次使用必须执行两步）
```bash
# 步骤 1: 上传固件
pio run --target upload

# 步骤 2: 上传 SPIFFS 文件系统（网页配置界面）
pio run --target uploadfs
```

> **重要**: 首次烧写或更新网页界面时，必须同时执行以上两步。
> - `upload` 上传程序代码
> - `uploadfs` 上传网页文件（data 目录下的 index.html 等）
> - 如果只修改代码，只需执行 `upload`
> - 如果只修改网页，只需执行 `uploadfs`

#### 串口监视器
```bash
pio device monitor
```

### First Use

1. Power on ESP32
2. Connect to WiFi: `ESP32_Audio_Setup`
3. Open browser: `http://192.168.4.1`
4. Configure network and audio settings
5. Click Save - device will restart

### Audio Sender Requirements

- Protocol: UDP
- Format: Raw PCM, Little-Endian
- Channels: Stereo interleaved (L R L R...)
- Settings must match ESP32 configuration

## Configuration

### Network Settings
- WiFi SSID and password
- DHCP or static IP
- UDP listening port (default: 8000)

### Audio Settings
- Sample rate: 44.1kHz to 192kHz
- Bit depth: 16/24/32 bit
- Buffer duration: 50-500ms

## Reset

Hold BOOT button for 5 seconds to clear all settings and enter configuration mode.

## License

MIT License

# ESP32 WiFi → S/PDIF 数字音频桥

把 Web Audio Streamer 后端通过 UDP 推送的 PCM 音频流，经 ESP32-S3 的 I2S 接口送入 **CS8406** S/PDIF 发射板，再由光纤/同轴输出到功放或解码器。配合 APLL 主时钟实现低抖动播放。

> 配套后端项目：[web-audio-streamer](https://github.com/a125477365/web-audio-streamer)

## 功能

- **闭环流控**：后端发 `setAudioConfig` 控制包，ESP32 回 ACK 确认后才推流，采样率/位深自动协调
- **低抖动**：启用 ESP32 APLL，并向 CS8406 输出 256Fs 主时钟（MCLK）
- **双配网方式**：WiFi 配置热点网页，或 USB 串口 JSON 命令一键配网
- **抗抖动环形缓冲**：根据可用内存与采样率自动计算缓冲大小
- **掉包保护**：缓冲溢出时整包丢弃，不破坏采样帧的左右声道对齐

## 硬件

### 元件
- ESP32-S3 开发板（platformio.ini 默认 `esp32-s3-devkitc-1`）
- **CS8406** I2S → S/PDIF 发射板（光纤 + 同轴输出）
- 光纤（TOSLINK）或同轴（RCA）线接功放/解码器

### ⭐ I2S 引脚对接（CS8406 ↔ ESP32-S3）

| CS8406 引脚 | 接到 ESP32-S3 | 说明 |
|------------|--------------|------|
| **MCK**  | **GPIO 16** | 主时钟，256Fs —— **CS8406 规格要求必须提供，否则无声** |
| **BCK**  | **GPIO 4**  | 位时钟，64Fs |
| **LRCK** | **GPIO 5**  | 左右声道时钟（WS） |
| **DIN**  | **GPIO 6**  | 串行音频数据 |
| **GND**  | **GND**     | 共地（必接） |

> 引脚定义见 [`include/config.h`](include/config.h)，可按需修改 `I2S_MCLK_PIN / I2S_BCLK_PIN / I2S_WS_PIN / I2S_DATA_PIN`。
> ESP32-S3 的 I2S 可经 GPIO 矩阵把任意信号（含 MCLK）路由到任意 GPIO；若换用经典 ESP32（非 S3），MCLK 只能用 GPIO 0/1/3。

### 供电与电平

- CS8406 板由**自带的 Type-C USB 5V 供电**，**不要**再从 ESP32 的 5V 给它供电（两路 5V 不要并联）。
- 信号电平 3.3V，与 ESP32 GPIO 一致，可直连。
- CS8406 工作在 **I2S 从模式 / 标准 I2S 格式**；ESP32 为主模式（产生 MCLK/BCK/LRCK），二者匹配。

### 关于采样率 / 位深

- CS8406 支持 24bit / 44.1k–192kHz。
- 本固件统一以 **32bit I2S 帧（BCK=64Fs）** 输出，源音频的 24bit 数据位于高位，CS8406 取高 24bit，无损。
- 后端默认把采样率限制在 **44.1k / 48k**，以保证 ESP32 单线程 UDP 在 WiFi 上稳定不欠载（更高码率可在后端 `_normalizeFormat` 放开）。

## 软件

### 构建环境
- PlatformIO（推荐）或 Arduino IDE
- ESP32 平台包（`espressif32 @ ^6.7.0`）

### 安装 PlatformIO
```bash
pip install --user platformio
```

### 编译与烧录
```bash
# 编译
pio run

# 烧录固件（按住 BOOT、点按 RST 进入下载模式后执行）
pio run -t upload --upload-port /dev/cu.usbmodemXXXX

# 串口监视（115200）
pio device monitor -b 115200
```

> 本项目网页配置界面通过代码内置（`web_server.cpp`），无需单独 `uploadfs`。

## 配网

### 方式一：配置热点（无需电脑）
1. 上电后 ESP32 开启热点 `ESP32_Audio_Setup`（开放，无密码）
2. 手机/电脑连上该热点，浏览器打开 `http://192.168.4.1`
3. 填写要连接的 WiFi 与端口，保存后设备重启并连入目标网络

### 方式二：USB 串口一键配网（推荐，适合无屏调试）
串口（115200）发送一行 JSON：
```jsonc
// 配置 WiFi（保存后自动重启）
{"cmd":"setWifi","ssid":"你的WiFi名","password":"你的密码"}
// 查询状态（返回 state / ip / ssid / port / rssi / heap）
{"cmd":"status"}
// 恢复出厂（清空配置并重启）
{"cmd":"reset"}
```

> ⚠️ ESP32 只支持 **2.4GHz** WiFi。若用 iPhone 个人热点，请在「设置 → 个人热点」开启「**最大兼容性**」，否则连不上。

## UDP 协议（与后端约定）

- 端口：默认 **8000**（`DEFAULT_LISTEN_PORT`）
- **音频包**：裸 PCM，小端，立体声交织（L R L R…），32bit/帧
- **控制包**：`[0xAA][0x55][seq][len_h][len_l][JSON]`
  - `{"cmd":"setAudioConfig","sampleRate":44100,"bitsPerSample":32,"channels":2}` — 重配 I2S
  - `{"cmd":"setVolume","volume":0-100}` — 音量（仅 ESP32 端缩放，避免双重衰减）
  - `{"cmd":"stop"}` — 停止并清空缓冲
- **ACK 包**：同格式，JSON 为 `{"cmd":"ack","originalCmd":"...","status":"ok|error"}`

## 复位

长按 **BOOT 键 5 秒** 清空全部配置并进入配置热点模式（开机 30 秒内忽略，避免误触）。

## License

MIT

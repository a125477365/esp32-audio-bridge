# ESP32 WiFi to S/PDIF Audio Bridge

High-fidelity wireless digital audio interface supporting up to 192kHz/32bit audio with low-jitter APLL clock.

## Features

- **Hi-Fi Audio**: Up to 192kHz/32bit PCM streaming
- **Low Jitter**: ESP32 APLL (Audio PLL) for precision clock
- **Web Configuration**: Easy setup via captive portal
- **Anti-Jitter Buffer**: Configurable ring buffer (50-500ms)
- **Flexible Network**: DHCP or static IP configuration
- **VBAN Support**: Compatible with Voicemeeter (Windows)
- **Auto-Detect**: Automatically detects VBAN vs raw PCM format

## Hardware

### Components
- ESP32-WROOM-32 development board
- I2S to S/PDIF module (CS8406 or DP7406)
- RCA coaxial cable

### Wiring

| ESP32 Pin | S/PDIF Module |
|-----------|---------------|
| GPIO 25   | BCK (Bit Clock) |
| GPIO 33   | WS/LRCLK (Word Select) |
| GPIO 26   | DATA |
| GND       | GND |

## Software

### Build Requirements
- PlatformIO (recommended) or Arduino IDE
- ESP32 board support package

### Building with PlatformIO

```bash
pip install platformio
pio run
pio run --target upload
```

### First Use

1. Power on ESP32
2. Connect to WiFi: `ESP32_Audio_Setup`
3. Open browser: `http://192.168.4.1`
4. Configure network and audio settings
5. Click Save - device will restart

## Audio Sources

### Method 1: VBAN (Recommended for Windows)

1. Install [Voicemeeter](https://vb-audio.com/Voicemeeter/) (free)
2. Set your music player output to "Voicemeeter Input"
3. Open Voicemeeter, click "VBAN" button
4. Add new stream:
   - Destination: ESP32 IP address
   - Port: 8000 (or your configured port)
   - Sample Rate: match ESP32 setting
   - Bit: 16-bit

### Method 2: Raw PCM (Command Line)

```bash
# macOS/Linux
ffmpeg -i music.flac -f s16le -ar 44100 -ac 2 - | nc -u <ESP32_IP> 8000

# Windows (PowerShell, requires ffmpeg and netcat)
ffmpeg -i music.flac -f s16le -ar 44100 -ac 2 - | nc -u <ESP32_IP> 8000
```

### Method 3: Custom Software

- Protocol: UDP
- Format: Raw PCM or VBAN
- Channels: Stereo interleaved (L R L R...)
- Little-Endian byte order

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

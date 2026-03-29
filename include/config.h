#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Pin Definitions - ESP32-S3 compatible
// ESP32-S3 I2S can use any GPIO, these are safe choices
#define I2S_BCLK_PIN 4
#define I2S_WS_PIN 5
#define I2S_DATA_PIN 6
#define RESET_BUTTON_PIN 0 // BOOT button (GPIO 0)

// Default Audio Parameters
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BITS_PER_SAMPLE 16
#define DEFAULT_CHANNELS 2
#define DEFAULT_LISTEN_PORT 8000
#define DEFAULT_BUFFER_MS 200

// Network Defaults
#define AP_SSID "ESP32_Audio_Setup"
#define AP_PASSWORD ""
#define AP_CHANNEL 1
#define AP_MAX_CONN 4

// Reset Button
#define RESET_HOLD_TIME_MS 5000

// Buffer Threshold
#define BUFFER_START_THRESHOLD 0.5f
#define BUFFER_UNDERRUN_THRESHOLD 0.1f

// WiFi Retry
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RETRY_INTERVAL_MS 5000
#define WIFI_MAX_RETRIES 3

// Debug - ESP32-S3 uses USB CDC
#if ARDUINO_USB_CDC_ON_BOOT
#define DEBUG_SERIAL Serial
#else
#define DEBUG_SERIAL Serial
#endif
#define DEBUG_BAUD 115200

// NVS Keys
#define NVS_NAMESPACE "audio_cfg"
#define NVK_SSID "ssid"
#define NVK_PASSWORD "pwd"
#define NVK_IP_MODE "ip_mode"
#define NVK_STATIC_IP "static_ip"
#define NVK_GATEWAY "gateway"
#define NVK_SUBNET "subnet"
#define NVK_DNS "dns"
#define NVK_PORT "port"
#define NVK_SAMPLE_RATE "sample_rate"
#define NVK_BITS "bits"
#define NVK_BUFFER_MS "buffer_ms"

// System States
enum SystemState {
    STATE_CONFIG_MODE,
    STATE_CONNECTING,
    STATE_WORKING,
    STATE_ERROR
};

// IP Mode
enum IPMode {
    IP_MODE_DHCP = 0,
    IP_MODE_STATIC = 1
};

#endif // CONFIG_H

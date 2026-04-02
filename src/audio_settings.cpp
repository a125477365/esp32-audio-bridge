#include "audio_settings.h"
#include <Preferences.h>

void AudioSettings::loadFromNVS() {
 Preferences prefs;
 // Try read-only first, fall back to read-write if namespace doesn't exist
 bool readOnly = true;
 if (!prefs.begin(NVS_NAMESPACE, true)) {
  // Namespace doesn't exist, open in read-write mode to create it
  prefs.end();
  readOnly = false;
  prefs.begin(NVS_NAMESPACE, false);
 }

 // Network
 wifiSSID = prefs.getString(NVK_SSID, "");
 wifiPassword = prefs.getString(NVK_PASSWORD, "");
 ipMode = (IPMode)prefs.getUChar(NVK_IP_MODE, IP_MODE_DHCP);

 String ipStr;
 ipStr = prefs.getString(NVK_STATIC_IP, "");
 if (ipStr.length() > 0) staticIP.fromString(ipStr);
 ipStr = prefs.getString(NVK_GATEWAY, "");
 if (ipStr.length() > 0) gateway.fromString(ipStr);
 ipStr = prefs.getString(NVK_SUBNET, "");
 if (ipStr.length() > 0) subnet.fromString(ipStr);
 ipStr = prefs.getString(NVK_DNS, "");
 if (ipStr.length() > 0) dns.fromString(ipStr);

 listenPort = prefs.getUShort(NVK_PORT, DEFAULT_LISTEN_PORT);

 // Audio - use auto-calculated buffer if not set
 bufferMs = prefs.getUShort(NVK_BUFFER_MS, 0);  // 0 means auto

 // If we created the namespace, write default values
 if (!readOnly) {
  saveToNVS();
 }
 prefs.end();
}

void AudioSettings::saveToNVS() {
 Preferences prefs;
 prefs.begin(NVS_NAMESPACE, false); // Read-write

 // Network
 prefs.putString(NVK_SSID, wifiSSID);
 prefs.putString(NVK_PASSWORD, wifiPassword);
 prefs.putUChar(NVK_IP_MODE, (uint8_t)ipMode);
 prefs.putString(NVK_STATIC_IP, staticIP.toString());
 prefs.putString(NVK_GATEWAY, gateway.toString());
 prefs.putString(NVK_SUBNET, subnet.toString());
 prefs.putString(NVK_DNS, dns.toString());
 prefs.putUShort(NVK_PORT, listenPort);

 // Audio - only buffer, sample rate and bits are set dynamically by backend
 prefs.putUShort(NVK_BUFFER_MS, bufferMs);

 prefs.end();
}

void AudioSettings::reset() {
 Preferences prefs;
 prefs.begin(NVS_NAMESPACE, false);
 prefs.clear();
 prefs.end();

 // Reset to defaults
 wifiSSID = "";
 wifiPassword = "";
 ipMode = IP_MODE_DHCP;
 bufferMs = 0;  // Auto
 listenPort = DEFAULT_LISTEN_PORT;
}

bool AudioSettings::hasWiFiConfig() {
 return wifiSSID.length() > 0;
}

// Calculate optimal buffer size based on available memory
// Returns buffer size in bytes, and sets bufferMs if auto mode
size_t AudioSettings::calculateOptimalBufferSize(uint32_t sampleRate, uint8_t bitsPerSample) {
 // Get available heap memory
 size_t freeHeap = ESP.getFreeHeap();

 // Reserve some memory for system operations (leave ~32KB free)
 size_t usableMemory = (freeHeap > 32768) ? (freeHeap - 32768) : 8192;

 // Limit to reasonable buffer size
 // Max 128KB for PSRAM devices, 48KB for internal RAM only
#if defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM == 1
 size_t maxBufferSize = 131072; // 128KB
#else
 size_t maxBufferSize = 49152;  // 48KB for internal RAM
#endif

 if (usableMemory > maxBufferSize) {
  usableMemory = maxBufferSize;
 }

 // Calculate bytes per millisecond at given sample rate
 // bytes_per_ms = sampleRate * (bits/8) * channels / 1000
 size_t bytesPerSample = bitsPerSample / 8;
 size_t bytesPerFrame = bytesPerSample * DEFAULT_CHANNELS;
 size_t bytesPerMs = (sampleRate * bytesPerFrame) / 1000;

 if (bytesPerMs == 0) bytesPerMs = 1; // Prevent division by zero

 // Calculate buffer duration in ms
 size_t calculatedBufferMs = usableMemory / bytesPerMs;

 // Clamp to reasonable range: 50ms minimum, 1000ms maximum
 if (calculatedBufferMs < 50) calculatedBufferMs = 50;
 if (calculatedBufferMs > 1000) calculatedBufferMs = 1000;

 // If bufferMs is 0 (auto mode), update it
 if (bufferMs == 0) {
  bufferMs = (uint16_t)calculatedBufferMs;
 }

 // Calculate final buffer size
 size_t totalBytes = bytesPerMs * bufferMs;

 // Round up to nearest 1024 for DMA alignment
 totalBytes = ((totalBytes + 1023) / 1024) * 1024;

 return totalBytes;
}

size_t AudioSettings::calculateBufferSize() {
 // Use default sample rate if not yet configured
 return calculateOptimalBufferSize(DEFAULT_SAMPLE_RATE, DEFAULT_BITS_PER_SAMPLE);
}

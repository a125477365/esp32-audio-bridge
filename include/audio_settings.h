#ifndef AUDIO_SETTINGS_H
#define AUDIO_SETTINGS_H

#include <Arduino.h>
#include "config.h"

struct AudioSettings {
 // Network
 String wifiSSID;
 String wifiPassword;
 IPMode ipMode;
 IPAddress staticIP;
 IPAddress gateway;
 IPAddress subnet;
 IPAddress dns;
 uint16_t listenPort;

 // Audio
 uint16_t bufferMs;  // 0 = auto (calculated from available memory)

 // Methods
 void loadFromNVS();
 void saveToNVS();
 void reset();
 bool hasWiFiConfig();

 // Calculate optimal buffer size based on available memory
 // If bufferMs is 0, it will be auto-calculated
 size_t calculateOptimalBufferSize(uint32_t sampleRate, uint8_t bitsPerSample);

 // Legacy method for compatibility
 size_t calculateBufferSize();
};

#endif // AUDIO_SETTINGS_H

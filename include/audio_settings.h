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
    uint32_t sampleRate;
    uint8_t bitsPerSample;
    uint16_t bufferMs;
    
    // Methods
    void loadFromNVS();
    void saveToNVS();
    void reset();
    bool hasWiFiConfig();
    
    // Calculate buffer size based on settings
    size_t calculateBufferSize();
};

#endif // AUDIO_SETTINGS_H

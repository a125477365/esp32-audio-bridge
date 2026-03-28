#include "audio_settings.h"
#include <Preferences.h>

void AudioSettings::loadFromNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // Read-only
    
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
    
    // Audio
    sampleRate = prefs.getUInt(NVK_SAMPLE_RATE, DEFAULT_SAMPLE_RATE);
    bitsPerSample = prefs.getUChar(NVK_BITS, DEFAULT_BITS_PER_SAMPLE);
    bufferMs = prefs.getUShort(NVK_BUFFER_MS, DEFAULT_BUFFER_MS);
    
    prefs.end();
}

void AudioSettings::saveToNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);  // Read-write
    
    // Network
    prefs.putString(NVK_SSID, wifiSSID);
    prefs.putString(NVK_PASSWORD, wifiPassword);
    prefs.putUChar(NVK_IP_MODE, (uint8_t)ipMode);
    prefs.putString(NVK_STATIC_IP, staticIP.toString());
    prefs.putString(NVK_GATEWAY, gateway.toString());
    prefs.putString(NVK_SUBNET, subnet.toString());
    prefs.putString(NVK_DNS, dns.toString());
    prefs.putUShort(NVK_PORT, listenPort);
    
    // Audio
    prefs.putUInt(NVK_SAMPLE_RATE, sampleRate);
    prefs.putUChar(NVK_BITS, bitsPerSample);
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
    sampleRate = DEFAULT_SAMPLE_RATE;
    bitsPerSample = DEFAULT_BITS_PER_SAMPLE;
    bufferMs = DEFAULT_BUFFER_MS;
    listenPort = DEFAULT_LISTEN_PORT;
}

bool AudioSettings::hasWiFiConfig() {
    return wifiSSID.length() > 0;
}

size_t AudioSettings::calculateBufferSize() {
    // Buffer size = sample_rate * (bits/8) * channels * buffer_ms / 1000
    // Example: 44100 * 2 * 2 * 0.2 = 35280 bytes for 200ms stereo 16-bit
    size_t bytesPerSample = bitsPerSample / 8;
    size_t bytesPerFrame = bytesPerSample * DEFAULT_CHANNELS;
    size_t totalBytes = (sampleRate * bytesPerFrame * bufferMs) / 1000;
    
    // Round up to nearest 1024 for DMA alignment
    return ((totalBytes + 1023) / 1024) * 1024;
}

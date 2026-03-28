/**
 * ESP32 WiFi to S/PDIF Digital Audio Bridge
 * 
 * Features:
 * - Hi-Fi audio support up to 192kHz/32bit
 * - Low jitter via APLL (Audio PLL)
 * - Web-based configuration
 * - Anti-jitter ring buffer
 * 
 * Hardware:
 * - ESP32-WROOM-32
 * - I2S to S/PDIF module (CS8406/DP7406)
 * 
 * Pinout:
 * - BCLK: GPIO 25
 * - WS/LRCLK: GPIO 33
 * - DATA: GPIO 26
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "audio_settings.h"
#include "ring_buffer.h"
#include "i2s_audio.h"
#include "udp_receiver.h"
#include "web_server.h"

// Global objects
AudioSettings settings;
RingBuffer audioBuffer;
I2SAudio i2s;
UDPReceiver udp;
WebConfigServer* webServer = nullptr;

// System state
SystemState currentState = STATE_CONFIG_MODE;
int wifiRetryCount = 0;

// Reset button timing
unsigned long resetPressStart = 0;
bool resetButtonPressed = false;

// Forward declarations
void enterConfigMode();
void enterConnectingMode();
void enterWorkingMode();
void handleResetButton();
void processAudioStream();
bool connectToWiFi();
void printStatus();

void setup() {
    // Initialize serial
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(1000);
    
    DEBUG_SERIAL.println("\n");
    DEBUG_SERIAL.println("=================================");
    DEBUG_SERIAL.println(" ESP32 Audio Bridge v1.0");
    DEBUG_SERIAL.println(" Hi-Fi WiFi to S/PDIF");
    DEBUG_SERIAL.println("=================================");
    
    // Load settings from NVS
    settings.loadFromNVS();
    
    // Initialize reset button
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    
    // Check if we have WiFi configuration
    if (settings.hasWiFiConfig()) {
        // We have config, try to connect
        enterConnectingMode();
    } else {
        // No config, enter config mode
        enterConfigMode();
    }
}

void loop() {
    // Always check reset button
    handleResetButton();
    
    switch (currentState) {
        case STATE_CONFIG_MODE:
            // Handle web server requests
            if (webServer) {
                webServer->handleClient();
            }
            break;
            
        case STATE_CONNECTING:
            // Wait for connection (handled in connectToWiFi)
            break;
            
        case STATE_WORKING:
            // Process audio stream
            processAudioStream();
            break;
            
        case STATE_ERROR:
            // Error state - wait for reset
            delay(1000);
            break;
    }
}

void enterConfigMode() {
    currentState = STATE_CONFIG_MODE;
    
    DEBUG_SERIAL.println("[MAIN] Entering Config Mode...");
    
    // Start WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONN);
    
    IPAddress apIP = WiFi.softAPIP();
    DEBUG_SERIAL.printf("[MAIN] AP started: %s\n", apIP.toString().c_str());
    DEBUG_SERIAL.printf("[MAIN] Connect to '%s' and open http://192.168.4.1\n", AP_SSID);
    
    // Start web server and DNS
    webServer = new WebConfigServer();
    webServer->begin();
}

void enterConnectingMode() {
    currentState = STATE_CONNECTING;
    
    DEBUG_SERIAL.println("[MAIN] Entering Connecting Mode...");
    DEBUG_SERIAL.printf("[MAIN] Connecting to SSID: %s\n", settings.wifiSSID.c_str());
    
    if (connectToWiFi()) {
        enterWorkingMode();
    } else {
        // Connection failed after retries
        DEBUG_SERIAL.println("[MAIN] WiFi connection failed, entering config mode");
        settings.reset();
        enterConfigMode();
    }
}

void enterWorkingMode() {
    currentState = STATE_WORKING;
    
    DEBUG_SERIAL.println("[MAIN] Entering Working Mode...");
    
    // Calculate buffer size
    size_t bufferSize = settings.calculateBufferSize();
    DEBUG_SERIAL.printf("[MAIN] Buffer size: %u bytes (%u ms)\n", bufferSize, settings.bufferMs);
    
    // Initialize ring buffer
    if (!audioBuffer.init(bufferSize)) {
        DEBUG_SERIAL.println("[MAIN] ERROR: Failed to allocate buffer!");
        currentState = STATE_ERROR;
        return;
    }
    
    // Initialize I2S with user settings
    if (!i2s.begin(settings.sampleRate, settings.bitsPerSample, bufferSize)) {
        DEBUG_SERIAL.println("[MAIN] ERROR: Failed to initialize I2S!");
        currentState = STATE_ERROR;
        return;
    }
    
    // Initialize UDP receiver
    if (!udp.begin(settings.listenPort)) {
        DEBUG_SERIAL.println("[MAIN] ERROR: Failed to start UDP listener!");
        currentState = STATE_ERROR;
        return;
    }
    
    printStatus();
}

bool connectToWiFi() {
    WiFi.mode(WIFI_STA);
    
    // Configure static IP if needed
    if (settings.ipMode == IP_MODE_STATIC) {
        if (!WiFi.config(settings.staticIP, settings.gateway, settings.subnet, settings.dns)) {
            DEBUG_SERIAL.println("[WIFI] Failed to set static IP");
        } else {
            DEBUG_SERIAL.printf("[WIFI] Static IP: %s\n", settings.staticIP.toString().c_str());
        }
    }
    
    // Start connection
    WiFi.begin(settings.wifiSSID.c_str(), settings.wifiPassword.c_str());
    
    wifiRetryCount = 0;
    while (wifiRetryCount < WIFI_MAX_RETRIES) {
        DEBUG_SERIAL.printf("[WIFI] Connecting... attempt %d/%d\n", 
                           wifiRetryCount + 1, WIFI_MAX_RETRIES);
        
        unsigned long startTime = millis();
        while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
            if (WiFi.status() == WL_CONNECTED) {
                DEBUG_SERIAL.println("[WIFI] Connected!");
                DEBUG_SERIAL.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
                return true;
            }
            delay(100);
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            wifiRetryCount++;
            DEBUG_SERIAL.printf("[WIFI] Connection failed, retry in %d ms\n", WIFI_RETRY_INTERVAL_MS);
            delay(WIFI_RETRY_INTERVAL_MS);
        }
    }
    
    return false;
}

void processAudioStream() {
    static uint8_t udpBuffer[2048];
    static uint8_t i2sBuffer[2048];
    static bool bufferStarted = false;
    
    // Read from UDP
    int bytesRead = udp.readPacket(udpBuffer, sizeof(udpBuffer));
    
    if (bytesRead > 0) {
        // Push to ring buffer
        size_t pushed = audioBuffer.push(udpBuffer, bytesRead);
        if (pushed < bytesRead) {
            // Buffer full, data lost
            DEBUG_SERIAL.println("[AUDIO] Buffer overflow!");
        }
    }
    
    // Check buffer level for output
    float level = audioBuffer.level();
    
    if (!bufferStarted) {
        // Wait for buffer to reach threshold before starting
        if (level >= BUFFER_START_THRESHOLD) {
            bufferStarted = true;
            DEBUG_SERIAL.printf("[AUDIO] Buffer started at %.1f%%\n", level * 100);
        }
    } else {
        // Check for underrun
        if (level < BUFFER_UNDERRUN_THRESHOLD) {
            DEBUG_SERIAL.println("[AUDIO] Buffer underrun, outputting silence");
            i2s.writeSilence(sizeof(i2sBuffer));
            bufferStarted = false;
        } else {
            // Normal operation: read from buffer and write to I2S
            size_t available = audioBuffer.available();
            if (available > 0) {
                size_t toRead = (available < sizeof(i2sBuffer)) ? available : sizeof(i2sBuffer);
                size_t bytesReadFromBuffer = audioBuffer.pop(i2sBuffer, toRead);
                if (bytesReadFromBuffer > 0) {
                    i2s.write(i2sBuffer, bytesReadFromBuffer);
                }
            }
        }
    }
}

void handleResetButton() {
    int buttonState = digitalRead(RESET_BUTTON_PIN);
    
    if (buttonState == LOW) {
        // Button pressed
        if (!resetButtonPressed) {
            resetButtonPressed = true;
            resetPressStart = millis();
        } else {
            // Check for long press
            if (millis() - resetPressStart >= RESET_HOLD_TIME_MS) {
                DEBUG_SERIAL.println("\n[MAIN] Reset button held 5s, clearing config...");
                settings.reset();
                ESP.restart();
            }
        }
    } else {
        // Button released
        resetButtonPressed = false;
        resetPressStart = 0;
    }
}

void printStatus() {
    DEBUG_SERIAL.println("\n=================================");
    DEBUG_SERIAL.println(" System Ready");
    DEBUG_SERIAL.println("=================================");
    DEBUG_SERIAL.printf(" WiFi: %s\n", settings.wifiSSID.c_str());
    DEBUG_SERIAL.printf(" IP: %s\n", WiFi.localIP().toString().c_str());
    DEBUG_SERIAL.printf(" Port: %d\n", settings.listenPort);
    DEBUG_SERIAL.printf(" Sample Rate: %lu Hz\n", settings.sampleRate);
    DEBUG_SERIAL.printf(" Bit Depth: %d bit\n", settings.bitsPerSample);
    DEBUG_SERIAL.printf(" Buffer: %u ms\n", settings.bufferMs);
    DEBUG_SERIAL.println("=================================");
    DEBUG_SERIAL.println("Waiting for audio stream...");
    DEBUG_SERIAL.println("Hold BOOT button 5s to reset config");
    DEBUG_SERIAL.println("=================================\n");
}

/**
 * ESP32 WiFi to S/PDIF Digital Audio Bridge
 *
 * Features:
 * - Hi-Fi audio support up to 192kHz/32bit
 * - Low jitter via APLL (Audio PLL)
 * - Web-based configuration
 * - Anti-jitter ring buffer
 * - Bit-perfect playback with dynamic sample rate switching
 * - Auto-calculated buffer size based on available memory
 *
 * Hardware:
 * - ESP32-WROOM-32 / ESP32-S3
 * - I2S to S/PDIF module (CS8406/DP7406)
 *
 * Pinout:
 * - BCLK: GPIO 4 (ESP32-S3)
 * - WS/LRCLK: GPIO 5 (ESP32-S3)
 * - DATA: GPIO 6 (ESP32-S3)
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
unsigned long startupTime = 0;
const unsigned long BUTTON_IGNORE_MS = 30000;

// Forward declarations
void enterConfigMode();
void enterConnectingMode();
void enterWorkingMode();
void handleResetButton();
void processAudioStream();
bool connectToWiFi();
void printStatus();

void setup() {
 DEBUG_SERIAL.begin(DEBUG_BAUD);
 delay(1000);
 DEBUG_SERIAL.println("\n");
 DEBUG_SERIAL.println("=================================");
 DEBUG_SERIAL.println(" ESP32 Audio Bridge v1.2");
 DEBUG_SERIAL.println(" Bit-Perfect WiFi to S/PDIF");
 DEBUG_SERIAL.println(" Auto Buffer Sizing");
 DEBUG_SERIAL.println("=================================");

 pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
 int bootPinStatus = digitalRead(RESET_BUTTON_PIN);
 DEBUG_SERIAL.printf("[MAIN] GPIO 0 (BOOT pin) status: %s\n", bootPinStatus == HIGH ? "HIGH (normal)" : "LOW (waiting...)");

 if (bootPinStatus == LOW) {
  DEBUG_SERIAL.println("[MAIN] BOOT pin is LOW, waiting for release...");
  int waitCount = 0;
  while (digitalRead(RESET_BUTTON_PIN) == LOW && waitCount < 100) {
   delay(100);
   waitCount++;
   if (waitCount % 10 == 0) {
    DEBUG_SERIAL.printf("[MAIN] Still waiting... (%d seconds)\n", waitCount / 10);
   }
  }
  if (digitalRead(RESET_BUTTON_PIN) == HIGH) {
   DEBUG_SERIAL.println("[MAIN] BOOT pin released, continuing startup...");
   delay(500);
  } else {
   DEBUG_SERIAL.println("[MAIN] BOOT pin still LOW after 10s, forcing continue...");
  }
 }

 settings.loadFromNVS();
 startupTime = millis();
 pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

 if (settings.hasWiFiConfig()) {
  enterConnectingMode();
 } else {
  enterConfigMode();
 }
}

void loop() {
 handleResetButton();

 switch (currentState) {
  case STATE_CONFIG_MODE:
   if (webServer) {
    webServer->handleClient();
   }
   break;

  case STATE_CONNECTING:
   break;

  case STATE_WORKING:
   processAudioStream();
   break;

  case STATE_ERROR:
   delay(1000);
   break;
 }
}

void enterConfigMode() {
 currentState = STATE_CONFIG_MODE;
 DEBUG_SERIAL.println("[MAIN] Entering Config Mode...");

 WiFi.mode(WIFI_AP);
 WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONN);
 IPAddress apIP = WiFi.softAPIP();
 DEBUG_SERIAL.printf("[MAIN] AP started: %s\n", apIP.toString().c_str());
 DEBUG_SERIAL.printf("[MAIN] Connect to '%s' and open http://192.168.4.1\n", AP_SSID);

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
  DEBUG_SERIAL.println("[MAIN] WiFi connection failed, entering config mode");
  settings.reset();
  enterConfigMode();
 }
}

void enterWorkingMode() {
 currentState = STATE_WORKING;
 DEBUG_SERIAL.println("[MAIN] Entering Working Mode...");

 if (webServer) {
  webServer->stop();
  delete webServer;
  webServer = nullptr;
 }
 WiFi.softAPdisconnect(true);
 WiFi.mode(WIFI_STA);
 DEBUG_SERIAL.println("[MAIN] AP mode stopped, using STA mode only");

 size_t bufferSize = settings.calculateBufferSize();
 DEBUG_SERIAL.printf("[MAIN] Free heap: %u bytes\n", ESP.getFreeHeap());
 DEBUG_SERIAL.printf("[MAIN] Buffer: %u bytes (%u ms)\n", bufferSize, settings.bufferMs);

 if (!audioBuffer.init(bufferSize)) {
  DEBUG_SERIAL.println("[MAIN] ERROR: Failed to allocate buffer!");
  currentState = STATE_ERROR;
  return;
 }

 if (!i2s.begin(DEFAULT_SAMPLE_RATE, DEFAULT_BITS_PER_SAMPLE, bufferSize)) {
  DEBUG_SERIAL.println("[MAIN] ERROR: Failed to initialize I2S!");
  currentState = STATE_ERROR;
  return;
 }

 if (!udp.begin(settings.listenPort)) {
  DEBUG_SERIAL.println("[MAIN] ERROR: Failed to start UDP listener!");
  currentState = STATE_ERROR;
  return;
 }

 printStatus();
}

bool connectToWiFi() {
 WiFi.mode(WIFI_STA);

 if (settings.ipMode == IP_MODE_STATIC) {
  if (!WiFi.config(settings.staticIP, settings.gateway, settings.subnet, settings.dns)) {
   DEBUG_SERIAL.println("[WIFI] Failed to set static IP");
  } else {
   DEBUG_SERIAL.printf("[WIFI] Static IP: %s\n", settings.staticIP.toString().c_str());
  }
 }

 WiFi.begin(settings.wifiSSID.c_str(), settings.wifiPassword.c_str());
 wifiRetryCount = 0;

 while (wifiRetryCount < WIFI_MAX_RETRIES) {
  DEBUG_SERIAL.printf("[WIFI] Connecting... attempt %d/%d\n", wifiRetryCount + 1, WIFI_MAX_RETRIES);

  unsigned long startTime = millis();
  while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
   if (WiFi.status() == WL_CONNECTED) {
    DEBUG_SERIAL.println("[WIFI] Connected!");
    DEBUG_SERIAL.printf("[WIFI] Local IP: %s\n", WiFi.localIP().toString().c_str());
    DEBUG_SERIAL.printf("[WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    DEBUG_SERIAL.printf("[WIFI] DNS: %s\n", WiFi.dnsIP().toString().c_str());
    DEBUG_SERIAL.printf("[WIFI] Subnet: %s\n", WiFi.subnetMask().toString().c_str());
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
 if (WiFi.status() != WL_CONNECTED) {
  DEBUG_SERIAL.println("[AUDIO] WiFi disconnected! Reconnecting...");
  WiFi.reconnect();
  delay(1000);
  return;
 }

 static uint8_t udpBuffer[2048];
 static uint8_t i2sBuffer[2048];
 static bool bufferStarted = false;

 int bytesRead = udp.readPacket(udpBuffer, sizeof(udpBuffer));

 if (bytesRead > 0) {
  if (udp.isControlPacket(udpBuffer, bytesRead)) {
   ControlPacket ctrl = udp.parseControlPacket(udpBuffer, bytesRead);
   if (ctrl.valid) {
    if (ctrl.cmd == CMD_SET_AUDIO_CONFIG) {
     udp.sendAck(ctrl.seq, CMD_SET_AUDIO_CONFIG, "ok");

     audioBuffer.clear();
     bufferStarted = false;

     if (i2s.reconfigure(ctrl.sampleRate, ctrl.bitsPerSample, ctrl.channels)) {
      DEBUG_SERIAL.printf("[AUDIO] Reconfigured to %lu Hz / %d bit / %d ch\n",
       ctrl.sampleRate, ctrl.bitsPerSample, ctrl.channels);

      size_t newBufferSize = settings.calculateOptimalBufferSize(ctrl.sampleRate, ctrl.bitsPerSample);
      DEBUG_SERIAL.printf("[AUDIO] Optimal buffer: %u bytes (%u ms)\n",
       newBufferSize, settings.bufferMs);
     } else {
      DEBUG_SERIAL.println("[AUDIO] ERROR: Reconfigure failed!");
     }
    } else if (ctrl.cmd == CMD_STOP) {
     udp.sendAck(ctrl.seq, CMD_STOP, "ok");

     audioBuffer.clear();
     bufferStarted = false;
     i2s.writeSilence(sizeof(i2sBuffer));
     DEBUG_SERIAL.println("[AUDIO] Stop command received, buffer cleared");
    }
   }
  } else {
   size_t pushed = audioBuffer.push(udpBuffer, bytesRead);
   if (pushed < bytesRead) {
    DEBUG_SERIAL.println("[AUDIO] Buffer overflow!");
   }
  }
 }

 float level = audioBuffer.level();

 if (!bufferStarted) {
  if (level >= BUFFER_START_THRESHOLD) {
   bufferStarted = true;
   DEBUG_SERIAL.printf("[AUDIO] Buffer started at %.1f%%\n", level * 100);
  }
 } else {
  if (level < BUFFER_UNDERRUN_THRESHOLD) {
   DEBUG_SERIAL.println("[AUDIO] Buffer underrun, outputting silence");
   i2s.writeSilence(sizeof(i2sBuffer));
   bufferStarted = false;
  } else {
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

 static int stableState = HIGH;
 static unsigned long lastDebounceTime = 0;
 const unsigned long DEBOUNCE_DELAY = 50;
 static int lastButtonState = HIGH;

 if (buttonState != lastButtonState) {
  lastDebounceTime = millis();
 }

 lastButtonState = buttonState;

 if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
  if (buttonState != stableState) {
   stableState = buttonState;

   if (stableState == LOW) {
    if (!resetButtonPressed) {
     resetButtonPressed = true;
     resetPressStart = millis();
     DEBUG_SERIAL.println("[MAIN] BOOT button pressed (GPIO 0 LOW)");
    }
   } else {
    if (resetButtonPressed) {
     DEBUG_SERIAL.println("[MAIN] BOOT button released");
    }
    resetButtonPressed = false;
    resetPressStart = 0;
   }
  }
 }

 if (resetButtonPressed && stableState == LOW) {
  if (millis() - startupTime < BUTTON_IGNORE_MS) {
   return;
  }

  unsigned long heldTime = millis() - resetPressStart;
  if (heldTime >= RESET_HOLD_TIME_MS) {
   DEBUG_SERIAL.println("\n[MAIN] Reset button held 5s, clearing config...");
   settings.reset();
   ESP.restart();
  } else if (heldTime >= 1000 && heldTime % 1000 == 0) {
   DEBUG_SERIAL.printf("[MAIN] BOOT button held: %lu seconds\n", heldTime / 1000);
  }
 }
}

void printStatus() {
 DEBUG_SERIAL.println("\n=================================");
 DEBUG_SERIAL.println(" System Ready");
 DEBUG_SERIAL.println("=================================");
 DEBUG_SERIAL.printf(" WiFi: %s\n", settings.wifiSSID.c_str());
 DEBUG_SERIAL.printf(" IP: %s\n", WiFi.localIP().toString().c_str());
 DEBUG_SERIAL.printf(" Port: %d\n", settings.listenPort);
 DEBUG_SERIAL.printf(" Buffer: %u ms (auto)\n", settings.bufferMs);
 DEBUG_SERIAL.println("=================================");
 DEBUG_SERIAL.println("Waiting for audio stream...");
 DEBUG_SERIAL.println("Sample rate/bit depth set by backend");
 DEBUG_SERIAL.println("Hold BOOT button 5s to reset config");
 DEBUG_SERIAL.println("=================================\n");
}

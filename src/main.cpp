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
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
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
void handleSerialConfig();
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
 handleSerialConfig();

 switch (currentState) {
  case STATE_CONFIG_MODE:
   if (webServer) {
    webServer->handleClient();
   }
   // Self-heal: if credentials exist (e.g. router was down at boot),
   // retry STA connection every 60s instead of staying stuck in AP mode
   {
    static unsigned long lastWifiRetry = 0;
    if (settings.hasWiFiConfig() && millis() - lastWifiRetry > 60000) {
     lastWifiRetry = millis();
     DEBUG_SERIAL.println("[MAIN] Config mode: retrying stored WiFi...");
     enterConnectingMode();
    }
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

 if (!webServer) {
  webServer = new WebConfigServer();
  webServer->begin();
 }
}

void enterConnectingMode() {
 currentState = STATE_CONNECTING;
 DEBUG_SERIAL.println("[MAIN] Entering Connecting Mode...");
 DEBUG_SERIAL.printf("[MAIN] Connecting to SSID: %s\n", settings.wifiSSID.c_str());

 if (connectToWiFi()) {
  enterWorkingMode();
 } else {
  // Keep stored credentials: the AP may just be temporarily down.
  // Config AP + serial provisioning both stay available in config mode.
  DEBUG_SERIAL.println("[MAIN] WiFi connection failed, entering config mode (credentials kept)");
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

 if (!i2s.begin(settings.lastSampleRate, settings.lastBitsPerSample, bufferSize)) {
  DEBUG_SERIAL.println("[MAIN] ERROR: Failed to initialize I2S!");
  currentState = STATE_ERROR;
  return;
 }

 
  DEBUG_SERIAL.printf("[MAIN] I2S initialized: %lu Hz / %d bit / %d ch (from last session)\n",
    settings.lastSampleRate, settings.lastBitsPerSample, settings.lastChannels);
if (!udp.begin(settings.listenPort)) {
  DEBUG_SERIAL.println("[MAIN] ERROR: Failed to start UDP listener!");
  currentState = STATE_ERROR;
  return;
 }

 printStatus();
}

bool connectToWiFi() {
 WiFi.mode(WIFI_STA);
 // Accept any AP security level — iPhone hotspots use WPA2/WPA3
 // transitional mode which can fail the handshake at the default
 // minimum (WPA2) on some IDF versions
 WiFi.setMinSecurity(WIFI_AUTH_WEP);

 if (settings.ipMode == IP_MODE_STATIC) {
  if (!WiFi.config(settings.staticIP, settings.gateway, settings.subnet, settings.dns)) {
   DEBUG_SERIAL.println("[WIFI] Failed to set static IP");
  } else {
   DEBUG_SERIAL.printf("[WIFI] Static IP: %s\n", settings.staticIP.toString().c_str());
  }
 }

 WiFi.begin(settings.wifiSSID.c_str(), settings.wifiPassword.c_str());

 // Workaround for WPA2/WPA3 transitional APs (e.g. iPhone hotspots):
 // declare the STA as PMF-incapable so the AP negotiates plain WPA2-PSK
 // instead of WPA3-SAE, whose handshake times out on IDF 4.4
 wifi_config_t conf;
 if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
  conf.sta.pmf_cfg.capable = false;
  conf.sta.pmf_cfg.required = false;
  esp_wifi_set_config(WIFI_IF_STA, &conf);
  esp_wifi_disconnect();
  esp_wifi_connect();
 }
 wifiRetryCount = 0;

 while (wifiRetryCount < WIFI_MAX_RETRIES) {
  DEBUG_SERIAL.printf("[WIFI] Connecting... attempt %d/%d\n", wifiRetryCount + 1, WIFI_MAX_RETRIES);

  unsigned long startTime = millis();
  while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
   if (WiFi.status() == WL_CONNECTED) {
    // Disable modem power-save: keeps ARP/UDP latency low and stable,
    // which is essential for real-time audio streaming
    WiFi.setSleep(false);
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

 // Periodic gratuitous ARP: some APs (notably phone hotspots) drop
 // client ARP broadcasts, so peers lose our MAC mapping and the device
 // becomes unreachable. Announcing ourselves keeps peer caches fresh.
 // Only needed while idle — during playback the 5Hz buffer-level reports
 // to the sender already keep peer ARP caches fresh.
 static unsigned long lastGarp = 0;
 if (udp.getSenderPort() == 0 && millis() - lastGarp > 30000) {
  lastGarp = millis();
  if (netif_default != nullptr) {
   etharp_gratuitous(netif_default);
  }
 }

 // Buffer level feedback to the sender (closed-loop rate control):
 // the backend compensates WiFi packet loss by speeding up / pausing
 static unsigned long lastLevelReport = 0;
 if (millis() - lastLevelReport > 200 && udp.getSenderPort() != 0) {
  lastLevelReport = millis();
  char json[64];
  int n = snprintf(json, sizeof(json), "{\"cmd\":\"bufLevel\",\"level\":%.2f}", audioBuffer.level());
  uint8_t pkt[80];
  pkt[0] = CTRL_MAGIC_0;
  pkt[1] = CTRL_MAGIC_1;
  pkt[2] = 0; // seq unused for unsolicited reports
  pkt[3] = (n >> 8) & 0xFF;
  pkt[4] = n & 0xFF;
  memcpy(pkt + 5, json, n);
  udp.sendToSender(pkt, 5 + n);
 }

 static uint8_t udpBuffer[2048];
 static uint8_t i2sBuffer[2048];
 static bool bufferStarted = false;

 // Drain ALL pending UDP packets before touching I2S: the blocking I2S
 // write below can stall this loop for a few ms while lwIP's UDP receive
 // queue only holds ~6 packets — reading one packet per loop iteration
 // loses packets and starves the ring buffer (start/underrun oscillation)
 int bytesRead;
 while ((bytesRead = udp.readPacket(udpBuffer, sizeof(udpBuffer))) > 0) {
  if (udp.isControlPacket(udpBuffer, bytesRead)) {
   ControlPacket ctrl = udp.parseControlPacket(udpBuffer, bytesRead);
   if (ctrl.valid) {
    if (ctrl.cmd == CMD_SET_AUDIO_CONFIG) {
     audioBuffer.clear();
     bufferStarted = false;

     bool reconfigOk = i2s.reconfigure(ctrl.sampleRate, ctrl.bitsPerSample, ctrl.channels);
     if (reconfigOk) {
      DEBUG_SERIAL.printf("[AUDIO] Reconfigured to %lu Hz / %d bit / %d ch\n",
       ctrl.sampleRate, ctrl.bitsPerSample, ctrl.channels);

      // Save to NVS for recovery after reboot
      settings.saveAudioConfig(ctrl.sampleRate, ctrl.bitsPerSample, ctrl.channels);

      // Resize ring buffer to match the new format's bandwidth
      size_t newBufferSize = settings.calculateOptimalBufferSize(ctrl.sampleRate, ctrl.bitsPerSample);
      if (newBufferSize != audioBuffer.size()) {
       if (audioBuffer.init(newBufferSize)) {
        DEBUG_SERIAL.printf("[AUDIO] Ring buffer resized: %u bytes (%u ms)\n",
         newBufferSize, settings.bufferMs);
       } else {
        DEBUG_SERIAL.println("[AUDIO] ERROR: Ring buffer resize failed!");
        reconfigOk = false;
       }
      }
     } else {
      DEBUG_SERIAL.println("[AUDIO] ERROR: Reconfigure failed!");
     }
     // ACK after reconfigure so the backend learns about failures
     udp.sendAck(ctrl.seq, CMD_SET_AUDIO_CONFIG, reconfigOk ? "ok" : "error");
    } else if (ctrl.cmd == CMD_SET_VOLUME) {
						udp.sendAck(ctrl.seq, CMD_SET_VOLUME, "ok");
						i2s.setVolume(ctrl.volume);
						DEBUG_SERIAL.printf("[AUDIO] Volume set to %d%%\n", ctrl.volume);
					} else if (ctrl.cmd == CMD_STOP) {
     udp.sendAck(ctrl.seq, CMD_STOP, "ok");

     audioBuffer.clear();
     bufferStarted = false;
     i2s.writeSilence(sizeof(i2sBuffer));
     DEBUG_SERIAL.println("[AUDIO] Stop command received, buffer cleared");
    }
   }
  } else {
   if (audioBuffer.freeSpace() >= (size_t)bytesRead) {
    audioBuffer.push(udpBuffer, bytesRead);
   } else {
    // Drop the whole packet: a partial write would shift the sample
    // frame boundary and swap L/R channels for the rest of the stream
    static uint32_t dropCount = 0;
    if ((++dropCount % 50) == 1) {
     DEBUG_SERIAL.println("[AUDIO] Buffer overflow, packet dropped");
    }
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
    // Small write quantum (~5ms of audio) keeps the blocking time short
    // so the UDP drain loop above runs often enough to avoid packet loss
    const size_t writeQuantum = 1024;
    size_t toRead = (available < writeQuantum) ? available : writeQuantum;
    size_t bytesReadFromBuffer = audioBuffer.pop(i2sBuffer, toRead);
    if (bytesReadFromBuffer > 0) {
     i2s.write(i2sBuffer, bytesReadFromBuffer);
    }
   }
  }
 }
}

/**
 * Serial provisioning channel (USB CDC)
 *
 * Line-based JSON commands, so the device can be configured headlessly
 * without joining the config AP:
 *   {"cmd":"setWifi","ssid":"MyAP","password":"secret"}  -> save + reboot
 *   {"cmd":"status"}                                      -> one-line JSON status
 *   {"cmd":"reset"}                                       -> clear config + reboot
 */
void handleSerialConfig() {
 static String line;
 while (DEBUG_SERIAL.available() > 0) {
  char c = (char)DEBUG_SERIAL.read();
  if (c == '\r') continue;
  if (c != '\n') {
   if (line.length() < 512) line += c;
   continue;
  }
  String cmdLine = line;
  line = "";
  cmdLine.trim();
  if (cmdLine.length() == 0 || cmdLine[0] != '{') continue;

  JsonDocument doc;
  if (deserializeJson(doc, cmdLine)) {
   DEBUG_SERIAL.println("{\"ok\":false,\"error\":\"bad json\"}");
   continue;
  }
  const char* cmd = doc["cmd"] | "";
  if (strcmp(cmd, "setWifi") == 0) {
   const char* ssid = doc["ssid"] | "";
   const char* password = doc["password"] | "";
   if (strlen(ssid) == 0) {
    DEBUG_SERIAL.println("{\"ok\":false,\"error\":\"missing ssid\"}");
    continue;
   }
   settings.wifiSSID = String(ssid);
   settings.wifiPassword = String(password);
   settings.ipMode = IP_MODE_DHCP;
   if (doc["port"].is<uint16_t>()) settings.listenPort = doc["port"].as<uint16_t>();
   settings.saveToNVS();
   DEBUG_SERIAL.printf("{\"ok\":true,\"cmd\":\"setWifi\",\"ssid\":\"%s\"}\n", ssid);
   delay(200);
   ESP.restart();
  } else if (strcmp(cmd, "status") == 0) {
   const char* stateName =
    currentState == STATE_WORKING ? "working" :
    currentState == STATE_CONFIG_MODE ? "config" :
    currentState == STATE_CONNECTING ? "connecting" : "error";
   DEBUG_SERIAL.printf(
    "{\"ok\":true,\"state\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"port\":%u,\"rate\":%lu,\"bits\":%u,\"rssi\":%d,\"rxPackets\":%lu,\"rxBytes\":%llu,\"bufLevel\":%.2f}\n",
    stateName,
    settings.wifiSSID.c_str(),
    WiFi.localIP().toString().c_str(),
    settings.listenPort,
    settings.lastSampleRate,
    settings.lastBitsPerSample,
    WiFi.RSSI(),
    (unsigned long)udp.rxPackets(),
    (unsigned long long)udp.rxBytes(),
    audioBuffer.level());
  } else if (strcmp(cmd, "reset") == 0) {
   DEBUG_SERIAL.println("{\"ok\":true,\"cmd\":\"reset\"}");
   settings.reset();
   delay(200);
   ESP.restart();
  } else if (strcmp(cmd, "udptest") == 0) {
   // Connectivity diagnostic: fire a few UDP packets at a host so we can
   // verify the device->LAN direction independently of the receive path
   const char* host = doc["host"] | "";
   uint16_t testPort = doc["port"] | 9999;
   IPAddress target;
   if (strlen(host) == 0 || !target.fromString(host)) {
    DEBUG_SERIAL.println("{\"ok\":false,\"error\":\"bad host\"}");
   } else {
    WiFiUDP testUdp;
    for (int i = 0; i < 5; i++) {
     testUdp.beginPacket(target, testPort);
     testUdp.printf("esp32-udptest-%d ip=%s", i, WiFi.localIP().toString().c_str());
     testUdp.endPacket();
     delay(100);
    }
    DEBUG_SERIAL.printf("{\"ok\":true,\"cmd\":\"udptest\",\"sent\":5,\"to\":\"%s:%u\"}\n", host, testPort);
   }
  } else {
   DEBUG_SERIAL.println("{\"ok\":false,\"error\":\"unknown cmd\"}");
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

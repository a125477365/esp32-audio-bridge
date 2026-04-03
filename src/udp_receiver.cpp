#include "udp_receiver.h"
#include <ArduinoJson.h>

UDPReceiver::UDPReceiver()
 : _running(false)
 , _port(DEFAULT_LISTEN_PORT)
 , _senderPort(0)
{
}

UDPReceiver::~UDPReceiver() {
 end();
}

bool UDPReceiver::begin(uint16_t port) {
 if (_running) {
 end();
 }
 _port = port;
 if (_udp.begin(port)) {
 _running = true;
 DEBUG_SERIAL.printf("[UDP] Listening on port %d\n", port);
 return true;
 }
 DEBUG_SERIAL.println("[UDP] Failed to start UDP listener");
 return false;
}

void UDPReceiver::end() {
 if (_running) {
 _udp.stop();
 _running = false;
 DEBUG_SERIAL.println("[UDP] Stopped");
 }
}

int UDPReceiver::readPacket(uint8_t* buffer, size_t maxLen) {
 if (!_running || buffer == nullptr) {
 return 0;
 }
 
 int packetSize = _udp.parsePacket();
 if (packetSize <= 0) {
 return 0;
 }

 // Get sender info
 _senderIP = _udp.remoteIP();
 _senderPort = _udp.remotePort();

 // Read packet data
 int bytesRead = _udp.read(buffer, (packetSize < (int)maxLen) ? packetSize : maxLen);
 return bytesRead;
}

bool UDPReceiver::isControlPacket(const uint8_t* buffer, size_t len) {
 return len >= 5 && buffer[0] == CTRL_MAGIC_0 && buffer[1] == CTRL_MAGIC_1;
}

ControlPacket UDPReceiver::parseControlPacket(const uint8_t* buffer, size_t len) {
 ControlPacket result;
 if (!isControlPacket(buffer, len)) {
 return result;
 }

 // Parse: [0xAA][0x55][seq][len_h][len_l][JSON]
 if (len < 5) {
 return result;
 }

 result.seq = buffer[2];
 uint16_t payloadLen = (buffer[3] << 8) | buffer[4];
 if (len < 5 + payloadLen) {
 DEBUG_SERIAL.println("[UDP] Control packet too short");
 return result;
 }

 // Extract JSON payload
 String json = String((const char*)(buffer + 5), payloadLen);
 DEBUG_SERIAL.printf("[UDP] Control packet seq=%d: %s\n", result.seq, json.c_str());

 // Parse JSON
 JsonDocument doc;
 DeserializationError error = deserializeJson(doc, json);
 if (error) {
 DEBUG_SERIAL.printf("[UDP] JSON parse error: %s\n", error.c_str());
 return result;
 }

 // Extract command
 const char* cmd = doc["cmd"];
 if (cmd == nullptr) {
 return result;
 }
 result.cmd = String(cmd);

 // Parse setAudioConfig
 if (result.cmd == CMD_SET_AUDIO_CONFIG) {
 result.sampleRate = doc["sampleRate"] | 44100;
 result.bitsPerSample = doc["bitsPerSample"] | 16;
 result.channels = doc["channels"] | 2;
 result.valid = true;
 }
 // Parse stop
 else if (result.cmd == CMD_STOP) {
 result.valid = true;
 }

 return result;
}

void UDPReceiver::sendAck(uint8_t seq, const char* originalCmd, const char* status) {
 // Build ACK JSON
 JsonDocument doc;
 doc["cmd"] = CMD_ACK;
 doc["originalCmd"] = originalCmd;
 doc["status"] = status;

 String json;
 serializeJson(doc, json);

 // Build ACK packet: [0xAA][0x55][seq][len_h][len_l][JSON]
 // Same format as control packet, with cmd="ack" in JSON
 uint8_t packet[256];
 int pos = 0;
 packet[pos++] = CTRL_MAGIC_0;
 packet[pos++] = CTRL_MAGIC_1;
 packet[pos++] = seq;
 uint16_t len = json.length();
 packet[pos++] = (len >> 8) & 0xFF;
 packet[pos++] = len & 0xFF;
 memcpy(packet + pos, json.c_str(), len);
 pos += len;

 sendToSender(packet, pos);
 DEBUG_SERIAL.printf("[UDP] Sent ACK for seq=%d, cmd=%s\n", seq, originalCmd);
}

void UDPReceiver::sendToSender(const uint8_t* data, size_t len) {
 if (!_running) return;
 _udp.beginPacket(_senderIP, _senderPort);
 _udp.write(data, len);
 _udp.endPacket();
}

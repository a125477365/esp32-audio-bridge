#include "udp_receiver.h"

UDPReceiver::UDPReceiver()
 : _running(false)
 , _port(DEFAULT_LISTEN_PORT)
 , _senderPort(0) {
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

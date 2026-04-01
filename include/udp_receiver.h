#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

// Control packet parse result
struct ControlPacket {
  uint8_t seq;
  String cmd;
  uint32_t sampleRate;
  uint8_t bitsPerSample;
  uint8_t channels;
  bool valid;
  
  ControlPacket() : seq(0), cmd(""), sampleRate(0), bitsPerSample(0), channels(0), valid(false) {}
};

class UDPReceiver {
public:
  UDPReceiver();
  ~UDPReceiver();
  
  bool begin(uint16_t port);
  void end();
  
  // Check for and read packet (returns bytes read, 0 if none)
  int readPacket(uint8_t* buffer, size_t maxLen);
  
  // Check if packet is a control packet
  bool isControlPacket(const uint8_t* buffer, size_t len);
  
  // Parse control packet (returns valid=true if successful)
  ControlPacket parseControlPacket(const uint8_t* buffer, size_t len);
  
  // Send ACK response
  void sendAck(uint8_t seq, const char* originalCmd, const char* status);
  
  // Send packet to current sender
  void sendToSender(const uint8_t* data, size_t len);
  
  // Get sender info
  IPAddress getSenderIP() const { return _senderIP; }
  uint16_t getSenderPort() const { return _senderPort; }
  
  bool isRunning() const { return _running; }

private:
  WiFiUDP _udp;
  bool _running;
  uint16_t _port;
  IPAddress _senderIP;
  uint16_t _senderPort;
};

#endif // UDP_RECEIVER_H

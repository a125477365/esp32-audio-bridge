#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "config.h"

class UDPReceiver {
public:
    UDPReceiver();
    ~UDPReceiver();
    
    bool begin(uint16_t port);
    void end();
    
    // Check for and read packet (returns bytes read, 0 if none)
    int readPacket(uint8_t* buffer, size_t maxLen);
    
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

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "audio_settings.h"

class WebConfigServer {
public:
    WebConfigServer();
    ~WebConfigServer();
    
    void begin();
    void handleClient();
    void stop();
    
    // DNS Server for captive portal
    void startDNS();
    void stopDNS();
    void handleDNS();
    
private:
    WebServer* _server;
    DNSServer* _dnsServer;
    AudioSettings* _settings;
    
    void setupRoutes();
    
    // Route handlers
    void handleRoot();
    void handleConfig();
    void handleSave();
    void handleScan();
    void handleReset();
    void handleNotFound();
};

#endif // WEB_SERVER_H

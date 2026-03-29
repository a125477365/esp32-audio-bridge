#include "web_server.h"
#include "config.h"
#include <DNSServer.h>
#include <FS.h>
#include <SPIFFS.h>

WebConfigServer::WebConfigServer()
    : _server(nullptr)
    , _dnsServer(nullptr)
    , _settings(nullptr)
{
}

WebConfigServer::~WebConfigServer()
{
    stop();
}

void WebConfigServer::begin()
{
    _settings = new AudioSettings();
    _server = new WebServer(80);
    _dnsServer = new DNSServer();

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        DEBUG_SERIAL.println("[WebServer] SPIFFS mount failed");
    }

    setupRoutes();
    _server->begin();
    startDNS();
    DEBUG_SERIAL.println("[WebServer] Started on 192.168.4.1");
}

void WebConfigServer::handleClient()
{
    if (_dnsServer) {
        _dnsServer->processNextRequest();
    }
    if (_server) {
        _server->handleClient();
    }
}

void WebConfigServer::stop()
{
    if (_dnsServer) {
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    if (_settings) {
        delete _settings;
        _settings = nullptr;
    }
    SPIFFS.end();
}

void WebConfigServer::startDNS()
{
    // Redirect all DNS queries to the captive portal
    _dnsServer->start(53, "*", WiFi.softAPIP());
}

void WebConfigServer::stopDNS()
{
    if (_dnsServer) {
        _dnsServer->stop();
    }
}

void WebConfigServer::setupRoutes()
{
    _server->on("/", std::bind(&WebConfigServer::handleRoot, this));
    _server->on("/config", std::bind(&WebConfigServer::handleConfig, this));
    _server->on("/save", HTTP_POST, std::bind(&WebConfigServer::handleSave, this));
    _server->on("/scan", std::bind(&WebConfigServer::handleScan, this));
    _server->on("/reset", HTTP_POST, std::bind(&WebConfigServer::handleReset, this));
    _server->onNotFound(std::bind(&WebConfigServer::handleNotFound, this));
}

void WebConfigServer::handleRoot()
{
    // Serve the configuration page from SPIFFS
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        DEBUG_SERIAL.println("[WebServer] Failed to open index.html");
        _server->send(500, "text/plain", "Failed to load page");
        return;
    }

    String content = file.readString();
    file.close();
    _server->send(200, "text/html", content);
}

void WebConfigServer::handleConfig()
{
    // Return current configuration as JSON
    _settings->loadFromNVS();

    String json = "{";
    json += "\"ssid\":\"" + _settings->wifiSSID + "\",";
    json += "\"port\":" + String(_settings->listenPort) + ",";
    json += "\"sample_rate\":" + String(_settings->sampleRate) + ",";
    json += "\"bits\":" + String(_settings->bitsPerSample) + ",";
    json += "\"buffer_ms\":" + String(_settings->bufferMs) + ",";
    json += "\"ip_mode\":" + String((int)_settings->ipMode) + ",";
    json += "\"ip\":\"" + _settings->staticIP.toString() + "\",";
    json += "\"gateway\":\"" + _settings->gateway.toString() + "\",";
    json += "\"subnet\":\"" + _settings->subnet.toString() + "\",";
    json += "\"dns\":\"" + _settings->dns.toString() + "\"";
    json += "}";

    _server->send(200, "application/json", json);
}

void WebConfigServer::handleSave()
{
    DEBUG_SERIAL.println("[WebServer] Saving configuration...");

    // Network config
    _settings->wifiSSID = _server->arg("ssid");
    _settings->wifiPassword = _server->arg("pwd");
    _settings->listenPort = _server->arg("port").toInt();

    // IP config
    if (_server->hasArg("ip_mode") && _server->arg("ip_mode") == "1") {
        _settings->ipMode = IP_MODE_STATIC;
        _settings->staticIP.fromString(_server->arg("ip"));
        _settings->gateway.fromString(_server->arg("gateway"));
        _settings->subnet.fromString(_server->arg("subnet"));
        _settings->dns.fromString(_server->arg("dns"));
    } else {
        _settings->ipMode = IP_MODE_DHCP;
    }

    // Audio config
    _settings->sampleRate = _server->arg("sample_rate").toInt();
    _settings->bitsPerSample = _server->arg("bits").toInt();
    _settings->bufferMs = _server->arg("buffer_ms").toInt();

    // Save to NVS
    _settings->saveToNVS();

    DEBUG_SERIAL.println("[WebServer] Configuration saved, restarting...");
    _server->send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
}

void WebConfigServer::handleScan()
{
    DEBUG_SERIAL.println("[WebServer] Scanning WiFi networks...");

    int n = WiFi.scanNetworks();
    String json = "[";

    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i));
        json += "}";
    }
    json += "]";

    WiFi.scanDelete();
    _server->send(200, "application/json", json);
}

void WebConfigServer::handleReset()
{
    DEBUG_SERIAL.println("[WebServer] Resetting configuration...");
    _settings->reset();
    _server->send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
}

void WebConfigServer::handleNotFound()
{
    // Redirect to captive portal for any unknown request
    _server->sendHeader("Location", "http://192.168.4.1/", true);
    _server->send(302, "text/plain", "");
}

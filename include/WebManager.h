#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

class WebManager {
private:
    AsyncWebServer server;
    DNSServer dns;

    String getHtmlPage();
    String getFileManagerHtml();
    void handleSave(AsyncWebServerRequest *request);
    bool checkAuth(AsyncWebServerRequest *request);

public:
    bool apMode;
    bool pendingRestart;
    unsigned long restartTime;

    WebManager(uint16_t port = 80);
    ~WebManager() = default;

    void setupWebServer();
    void startConfigPortal();
    void processDns();
};

extern WebManager webManager;

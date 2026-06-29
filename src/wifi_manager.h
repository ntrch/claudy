#pragma once

// ============================================================
// wifi_manager.h — WiFi setup via captive portal header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "config.h"
#include "api_client.h"  // for AuthType

class WifiSetupManager {
public:
    WifiSetupManager();

    // Initialize: load saved config, then start WiFiManager portal if needed
    // displayCallback is called with status messages during setup
    bool begin(std::function<void(const String&)> displayCallback = nullptr);

    // Check and maintain connection; returns true if connected
    bool isConnected();

    // Attempt reconnect if disconnected (call from loop)
    void maintain();

    // Accessors for saved credentials
    AuthType getAuthType() const { return _authType; }
    String   getToken()    const { return _token; }

    // RSSI of current connection
    int getRssi() const;

    // Reset saved WiFi + config (triggers fresh captive portal on next boot)
    void resetConfig();

private:
    WiFiManager  _wm;
    Preferences  _prefs;
    AuthType     _authType;
    String       _token;

    bool         _configured;
    uint32_t     _lastReconnectAttempt;

    void loadConfig();
    void saveConfig(const String& authType, const String& token);
};

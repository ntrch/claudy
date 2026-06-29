// ============================================================
// wifi_manager.cpp — WiFi setup via captive portal
// ESP32 Claude Code Companion
// ============================================================

#include "wifi_manager.h"
#include <WiFi.h>

// --------------- Constructor ---------------
WifiSetupManager::WifiSetupManager()
    : _configured(false)
    , _lastReconnectAttempt(0)
{
    _apiUrl = DEFAULT_API_URL;
    _apiKey = DEFAULT_API_KEY;
}

// --------------- begin() ---------------
bool WifiSetupManager::begin(std::function<void(const String&)> displayCallback) {
    // Load previously saved API config from Preferences (NVS)
    loadConfig();

    // Configure WiFiManager
    _wm.setConfigPortalTimeout(WIFI_CONNECT_TIMEOUT_S);
    _wm.setConnectTimeout(20);        // 20s per connect attempt
    _wm.setDebugOutput(true);
    _wm.setSaveConfigCallback([this]() {
        Serial.println("[WiFi] Config saved by portal callback");
    });

    // Custom parameters for API endpoint and key
    WiFiManagerParameter paramApiUrl(
        "api_url", "API Endpoint URL", _apiUrl.c_str(), 200
    );
    WiFiManagerParameter paramApiKey(
        "api_key", "API Key", _apiKey.c_str(), 100
    );
    _wm.addParameter(&paramApiUrl);
    _wm.addParameter(&paramApiKey);

    // Optional: notify display
    if (displayCallback) {
        displayCallback("Starting WiFi...");
    }

    Serial.println("[WiFi] Starting autoConnect...");
    bool connected = _wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    if (connected) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());

        // Read back custom parameters
        String newUrl = String(paramApiUrl.getValue());
        String newKey = String(paramApiKey.getValue());

        // Save if changed
        if (newUrl != _apiUrl || newKey != _apiKey) {
            _apiUrl = newUrl;
            _apiKey = newKey;
            saveConfig(_apiUrl, _apiKey);
        }

        if (displayCallback) {
            displayCallback("WiFi OK: " + WiFi.localIP().toString());
        }
    } else {
        Serial.println("[WiFi] Connection failed / timeout");
        if (displayCallback) {
            displayCallback("WiFi FAILED");
        }
    }

    _configured = true;
    return connected;
}

// --------------- isConnected() ---------------
bool WifiSetupManager::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

// --------------- maintain() — call from loop ---------------
void WifiSetupManager::maintain() {
    if (WiFi.status() == WL_CONNECTED) {
        return; // All good
    }

    uint32_t now = millis();
    // Retry reconnect every 30 seconds
    if (now - _lastReconnectAttempt < 30000UL) {
        return;
    }
    _lastReconnectAttempt = now;

    Serial.println("[WiFi] Disconnected — attempting reconnect...");
    WiFi.reconnect();

    // Wait up to 10s for reconnect
    uint32_t waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 10000UL) {
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] Reconnected. IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("[WiFi] Reconnect failed, will retry in 30s");
    }
}

// --------------- getRssi() ---------------
int WifiSetupManager::getRssi() const {
    return WiFi.RSSI();
}

// --------------- resetConfig() ---------------
void WifiSetupManager::resetConfig() {
    _wm.resetSettings();
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[WiFi] Config reset. Restarting...");
    ESP.restart();
}

// --------------- Private: load config ---------------
void WifiSetupManager::loadConfig() {
    _prefs.begin(PREF_NAMESPACE, true); // read-only
    _apiUrl = _prefs.getString(PREF_KEY_API_URL, DEFAULT_API_URL);
    _apiKey = _prefs.getString(PREF_KEY_API_KEY, DEFAULT_API_KEY);
    _prefs.end();

    Serial.print("[WiFi] Loaded API URL: ");
    Serial.println(_apiUrl);
}

// --------------- Private: save config ---------------
void WifiSetupManager::saveConfig(const String& apiUrl, const String& apiKey) {
    _prefs.begin(PREF_NAMESPACE, false); // read-write
    _prefs.putString(PREF_KEY_API_URL, apiUrl);
    _prefs.putString(PREF_KEY_API_KEY, apiKey);
    _prefs.end();

    Serial.println("[WiFi] Config saved to NVS");
}

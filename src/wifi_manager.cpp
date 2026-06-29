// ============================================================
// wifi_manager.cpp — WiFi setup via captive portal
// ESP32 Claude Code Companion
// ============================================================

#include "wifi_manager.h"
#include <WiFi.h>

// --------------- Constructor ---------------
WifiSetupManager::WifiSetupManager()
    : _authType(AuthType::API_KEY)
    , _token("")
    , _clientId("")
    , _accessToken("")
    , _configured(false)
    , _lastReconnectAttempt(0)
{}

// --------------- begin() ---------------
bool WifiSetupManager::begin(std::function<void(const String&)> displayCallback) {
    loadConfig();

    _wm.setConfigPortalTimeout(WIFI_CONNECT_TIMEOUT_S);
    _wm.setConnectTimeout(20);
    _wm.setDebugOutput(true);
    _wm.setSaveConfigCallback([this]() {
        Serial.println("[WiFi] Config saved by portal callback");
    });

    _wm.setTitle("Claudy Kurulum");
    _wm.setCustomHeadElement("<style>body{font-family:sans-serif;} .c{text-align:left;}</style>");

    // --- Custom HTML explanation block (no input) ---
    WiFiManagerParameter htmlInfo(
        "<hr><h3>Claude Ayarlari</h3>"
        "<p><b>Adim 1:</b> Terminalde <code>claude setup-token</code> calistirin</p>"
        "<p><b>Adim 2:</b> Claude Code binary'den Client ID'yi cikartin:<br>"
        "<code>node -e \"...\"</code></p>"
        "<p><b>Adim 3:</b> Asagidaki alanlari doldurun</p><hr>"
    );

    // --- Auth type: use a simple text input (oauth or apikey) ---
    // WiFiManager doesn't support dropdowns natively, so use a text field
    // with instructions
    WiFiManagerParameter htmlAuthLabel(
        "<label><b>Kimlik Dogrulama Turu</b></label><br>"
        "<small>Abone icin: <b>oauth</b> yazin | API Anahtari icin: <b>apikey</b> yazin</small>"
    );

    String authDefault = (_authType == AuthType::OAUTH_TOKEN) ? "oauth" : "apikey";
    WiFiManagerParameter paramAuthType("auth_type", "Tur (oauth / apikey)", authDefault.c_str(), 10);

    WiFiManagerParameter paramClientId("client_id", "OAuth Client ID", _clientId.c_str(), 100);
    WiFiManagerParameter paramToken("token", "Refresh Token / API Key", _token.c_str(), 250);

    _wm.addParameter(&htmlInfo);
    _wm.addParameter(&htmlAuthLabel);
    _wm.addParameter(&paramAuthType);
    _wm.addParameter(&paramClientId);
    _wm.addParameter(&paramToken);

    if (displayCallback) {
        displayCallback("Baglaniyor...");
    }

    Serial.println("[WiFi] Starting autoConnect...");
    bool connected = _wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    if (connected) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());

        String newAuthStr  = String(paramAuthType.getValue());
        String newClientId = String(paramClientId.getValue());
        String newToken    = String(paramToken.getValue());

        AuthType newAuthType = (newAuthStr == "oauth") ? AuthType::OAUTH_TOKEN : AuthType::API_KEY;

        bool changed = false;
        if (newAuthType != _authType) { _authType = newAuthType; changed = true; }
        if (newClientId.length() > 0 && newClientId != _clientId) { _clientId = newClientId; changed = true; }
        if (newToken.length() > 0 && newToken != _token) { _token = newToken; changed = true; }

        if (changed) {
            saveConfig();
        }

        if (displayCallback) {
            displayCallback("WiFi OK: " + WiFi.localIP().toString());
        }
    } else {
        Serial.println("[WiFi] Connection failed / timeout");
        if (displayCallback) {
            displayCallback("WiFi HATASI");
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

// --------------- setAccessToken() ---------------
void WifiSetupManager::setAccessToken(const String& token) {
    _accessToken = token;
    _prefs.begin(PREF_NAMESPACE, false);
    _prefs.putString(PREF_KEY_ACCESS_TOKEN, token);
    _prefs.end();
}

// --------------- Private: load config ---------------
void WifiSetupManager::loadConfig() {
    _prefs.begin(PREF_NAMESPACE, true); // read-only
    String authTypeStr = _prefs.getString(PREF_KEY_AUTH_TYPE, "oauth");
    _token             = _prefs.getString(PREF_KEY_TOKEN, "");
    _clientId          = _prefs.getString(PREF_KEY_OAUTH_CLIENT_ID, "");
    _accessToken       = _prefs.getString(PREF_KEY_ACCESS_TOKEN, "");
    _prefs.end();

    _authType = (authTypeStr == "oauth") ? AuthType::OAUTH_TOKEN : AuthType::API_KEY;

    Serial.print("[WiFi] Loaded auth type: ");
    Serial.println(authTypeStr);
    Serial.print("[WiFi] Token set: ");
    Serial.println(_token.length() > 0 ? "yes" : "no");
    Serial.print("[WiFi] Client ID set: ");
    Serial.println(_clientId.length() > 0 ? "yes" : "no");
    Serial.print("[WiFi] Access token cached: ");
    Serial.println(_accessToken.length() > 0 ? "yes" : "no");
}

// --------------- Private: save config ---------------
void WifiSetupManager::saveConfig() {
    _prefs.begin(PREF_NAMESPACE, false); // read-write
    _prefs.putString(PREF_KEY_AUTH_TYPE, (_authType == AuthType::OAUTH_TOKEN) ? "oauth" : "apikey");
    _prefs.putString(PREF_KEY_TOKEN, _token);
    _prefs.putString(PREF_KEY_OAUTH_CLIENT_ID, _clientId);
    _prefs.putString(PREF_KEY_ACCESS_TOKEN, _accessToken);
    _prefs.end();

    Serial.println("[WiFi] Config saved to NVS");
}

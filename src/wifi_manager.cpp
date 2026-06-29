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
    , _configured(false)
    , _lastReconnectAttempt(0)
{}

// --------------- begin() ---------------
bool WifiSetupManager::begin(std::function<void(const String&)> displayCallback) {
    // Load previously saved config from Preferences (NVS)
    loadConfig();

    // Configure WiFiManager
    _wm.setConfigPortalTimeout(WIFI_CONNECT_TIMEOUT_S);
    _wm.setConnectTimeout(20);        // 20s per connect attempt
    _wm.setDebugOutput(true);
    _wm.setSaveConfigCallback([this]() {
        Serial.println("[WiFi] Config saved by portal callback");
    });

    // Turkish portal title and style
    _wm.setTitle("Claudy Kurulum");
    _wm.setCustomHeadElement("<style>body{font-family:sans-serif;}</style>");

    // Auth type selector — custom HTML rendered as a dropdown
    // Note: WiFiManagerParameter with id="" and custom HTML injects raw HTML
    const char* authTypeHtml = R"rawliteral(
<br/>
<label for='auth_type'><b>Kimlik Dogrulama Turu</b></label><br/>
<select name='auth_type' id='auth_type' style='width:100%;padding:5px;margin:5px 0;'>
  <option value='oauth'>Abone (Pro/Max/Team) - OAuth Token</option>
  <option value='apikey'>API Anahtari - Gelistirici</option>
</select>
<br/><br/>
<label for='token'><b>Token / Anahtar</b></label><br/>
<small>Pro/Max: Claude Code'da <code>claude setup-token</code> komutu ile alin</small><br/>
<small>API: Anthropic Console'dan API anahtarinizi kopyalayin</small>
)rawliteral";

    // Inject the auth type dropdown as a custom HTML block
    WiFiManagerParameter authTypeParam("auth_type_html", authTypeHtml, "", 0, authTypeHtml);

    // Token input field (pre-fill with saved token)
    WiFiManagerParameter tokenParam("token", "Token / Anahtar", _token.c_str(), 200);

    _wm.addParameter(&authTypeParam);
    _wm.addParameter(&tokenParam);

    // Optional: notify display
    if (displayCallback) {
        displayCallback("Baglaniyor...");
    }

    Serial.println("[WiFi] Starting autoConnect...");
    bool connected = _wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    if (connected) {
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());

        // Read back custom parameters
        String newAuthTypeStr = String(authTypeParam.getValue());
        String newToken       = String(tokenParam.getValue());

        // Determine auth type from dropdown value
        AuthType newAuthType = (newAuthTypeStr == "oauth")
                               ? AuthType::OAUTH_TOKEN
                               : AuthType::API_KEY;

        // Save if changed
        bool authChanged  = (newAuthType != _authType);
        bool tokenChanged = (newToken != _token && newToken.length() > 0);

        if (authChanged || tokenChanged) {
            _authType = newAuthType;
            if (newToken.length() > 0) {
                _token = newToken;
            }
            saveConfig(
                (_authType == AuthType::OAUTH_TOKEN) ? "oauth" : "apikey",
                _token
            );
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

// --------------- Private: load config ---------------
void WifiSetupManager::loadConfig() {
    _prefs.begin(PREF_NAMESPACE, true); // read-only
    String authTypeStr = _prefs.getString(PREF_KEY_AUTH_TYPE, "apikey");
    _token             = _prefs.getString(PREF_KEY_TOKEN, "");
    _prefs.end();

    _authType = (authTypeStr == "oauth") ? AuthType::OAUTH_TOKEN : AuthType::API_KEY;

    Serial.print("[WiFi] Loaded auth type: ");
    Serial.println(authTypeStr);
    Serial.print("[WiFi] Token set: ");
    Serial.println(_token.length() > 0 ? "yes" : "no");
}

// --------------- Private: save config ---------------
void WifiSetupManager::saveConfig(const String& authType, const String& token) {
    _prefs.begin(PREF_NAMESPACE, false); // read-write
    _prefs.putString(PREF_KEY_AUTH_TYPE, authType);
    _prefs.putString(PREF_KEY_TOKEN, token);
    _prefs.end();

    Serial.println("[WiFi] Config saved to NVS");
}

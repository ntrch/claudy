// ============================================================
// api_client.cpp — Fetch usage data from Anthropic API headers
// ESP32 Claude Code Companion
// ============================================================

#include "api_client.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Rate-limit headers to collect from Anthropic response
static const char* kRateLimitHeaders[] = {
    "anthropic-ratelimit-unified-5h-utilization",
    "anthropic-ratelimit-unified-5h-reset",
    "anthropic-ratelimit-unified-7d-utilization",
    "anthropic-ratelimit-unified-7d-reset",
    "anthropic-ratelimit-requests-limit",
    "anthropic-ratelimit-requests-remaining",
    "anthropic-ratelimit-tokens-limit",
    "anthropic-ratelimit-tokens-remaining",
    "retry-after",
};
static const int kRateLimitHeaderCount = sizeof(kRateLimitHeaders) / sizeof(kRateLimitHeaders[0]);

// Minimal request body — costs almost nothing, just triggers rate-limit headers
static const char* kRequestBody =
    "{\"model\":\"" ANTHROPIC_MIN_MODEL "\","
    "\"max_tokens\":1,"
    "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";

// --------------- Constructor ---------------
ApiClient::ApiClient()
    : _authType(AuthType::API_KEY)
    , _authToken("")
    , _refreshToken("")
    , _clientId("")
    , _refreshAttempted(false)
    , _wifiMgr(nullptr)
{}

// --------------- Configure authentication ---------------
void ApiClient::setAuth(AuthType type, const String& token) {
    _authType  = type;
    _authToken = token;
    Serial.print("[API] Auth type: ");
    Serial.println(type == AuthType::OAUTH_TOKEN ? "OAuth Token" : "API Key");
}

// --------------- Set OAuth credentials ---------------
void ApiClient::setOAuthCredentials(const String& refreshToken, const String& clientId, const String& accessToken) {
    _refreshToken = refreshToken;
    _clientId     = clientId;
    if (accessToken.length() > 0) {
        _authToken = accessToken;  // use cached access token
    }
}

// --------------- OAuth token refresh ---------------
bool ApiClient::refreshOAuthToken() {
    if (_refreshToken.length() == 0) {
        Serial.println("[API] No refresh token available");
        return false;
    }
    if (_clientId.length() == 0) {
        Serial.println("[API] No OAuth client ID available");
        return false;
    }

    Serial.println("[API] Refreshing OAuth access token...");
    Serial.print("[API]   refresh_token len: ");
    Serial.println(_refreshToken.length());
    Serial.print("[API]   client_id len: ");
    Serial.println(_clientId.length());

    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;
    if (!http.begin(secureClient, OAUTH_REFRESH_URL)) {
        Serial.println("[API] OAuth refresh HTTP begin failed");
        return false;
    }

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = "grant_type=refresh_token"
                  "&refresh_token=" + _refreshToken +
                  "&client_id=" + _clientId;

    int code = http.POST(body);
    bool ok = false;

    if (code == 200) {
        String payload = http.getString();
        Serial.print("[API] OAuth response: ");
        Serial.println(payload.substring(0, 100));
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            String newAccess = doc["access_token"] | "";
            if (newAccess.length() > 0) {
                _authToken = newAccess;
                if (_wifiMgr) {
                    _wifiMgr->setAccessToken(newAccess);
                }
                Serial.println("[API] OAuth token refreshed successfully");
                ok = true;
            }
        } else {
            Serial.print("[API] OAuth JSON parse error: ");
            Serial.println(err.c_str());
        }
    }

    if (!ok) {
        Serial.printf("[API] OAuth refresh failed HTTP %d\n", code);
        if (code > 0) {
            String errBody = http.getString();
            Serial.print("[API] Response: ");
            Serial.println(errBody.substring(0, 200));
        }
    }

    http.end();
    return ok;
}

// --------------- Main fetch ---------------
ApiResult ApiClient::fetchUsage(UsageData& outData) {
    if (WiFi.status() != WL_CONNECTED) {
        _lastError = "No WiFi connection";
        return ApiResult::ERR_NO_WIFI;
    }

    // If OAuth and no access token yet, try refresh first
    if (_authType == AuthType::OAUTH_TOKEN && _authToken.length() == 0) {
        if (!refreshOAuthToken()) {
            _lastError = "OAuth: token yenilenemedi";
            return ApiResult::ERR_HTTP_STATUS;
        }
    }

    Serial.println("[API] POST " ANTHROPIC_API_URL);

    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Skip certificate verification for simplicity

    HTTPClient http;
    if (!http.begin(secureClient, ANTHROPIC_API_URL)) {
        _lastError = "HTTP begin failed";
        return ApiResult::ERR_HTTP_CONNECT;
    }

    http.setTimeout(15000); // 15 second timeout
    http.addHeader("Content-Type", "application/json");
    http.addHeader("anthropic-version", ANTHROPIC_API_VERSION);

    if (_authType == AuthType::OAUTH_TOKEN) {
        http.addHeader("Authorization", "Bearer " + _authToken);
    } else {
        http.addHeader("x-api-key", _authToken);
    }

    // Register headers to collect from response
    http.collectHeaders(kRateLimitHeaders, kRateLimitHeaderCount);

    String body = String(kRequestBody);
    int code = http.POST(body);

    if (code <= 0) {
        _lastError = "HTTP connect error: " + String(code) + " " + http.errorToString(code);
        http.end();
        return ApiResult::ERR_HTTP_CONNECT;
    }

    Serial.print("[API] HTTP status: ");
    Serial.println(code);

    // Handle 401: try OAuth refresh and retry once
    if (code == 401 && _authType == AuthType::OAUTH_TOKEN && !_refreshAttempted) {
        http.end();
        Serial.println("[API] 401 — attempting OAuth token refresh...");
        _refreshAttempted = true;
        if (refreshOAuthToken()) {
            ApiResult result = fetchUsage(outData);
            _refreshAttempted = false;
            return result;
        }
        _refreshAttempted = false;
        _lastError = "OAuth token gecersiz";
        return ApiResult::ERR_HTTP_STATUS;
    }

    // 200 OK or 429 Too Many Requests both return useful rate-limit headers
    if (code != 200 && code != 429) {
        _lastError = "HTTP " + String(code);
        http.end();
        return ApiResult::ERR_HTTP_STATUS;
    }

    ApiResult result = parseHeaders(http, outData);
    http.end();
    return result;
}

// --------------- Retry with exponential backoff ---------------
bool ApiClient::fetchWithRetry(UsageData& outData, int maxRetries) {
    uint32_t backoffMs = API_RETRY_BASE_DELAY_MS;

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.print("[API] Attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(maxRetries);

        ApiResult result = fetchUsage(outData);

        if (result == ApiResult::OK) {
            Serial.println("[API] Fetch OK");
            return true;
        }

        Serial.print("[API] Failed: ");
        Serial.println(_lastError);

        if (attempt < maxRetries) {
            Serial.print("[API] Retrying in ");
            Serial.print(backoffMs);
            Serial.println("ms...");
            delay(backoffMs);
            backoffMs *= 2; // exponential backoff
        }
    }

    Serial.println("[API] All retries exhausted");
    return false;
}

// --------------- Parse response headers ---------------
ApiResult ApiClient::parseHeaders(HTTPClient& http, UsageData& outData) {
    // --- Subscriber (Pro/Max/Team) unified rate-limit headers ---
    String s5h      = http.header("anthropic-ratelimit-unified-5h-utilization");
    String s5hReset = http.header("anthropic-ratelimit-unified-5h-reset");
    String s7d      = http.header("anthropic-ratelimit-unified-7d-utilization");
    String s7dReset = http.header("anthropic-ratelimit-unified-7d-reset");

    if (s5h.length() > 0) {
        // Subscriber mode — values are fractions (e.g., "0.45" = 45%)
        float pct5h = s5h.toFloat();
        outData.sessionUsed  = (int)(pct5h * 100.0f);
        outData.sessionLimit = 100;
        outData.sessionUnit  = "%";
        outData.resetSession = s5hReset;

        Serial.printf("[API]   5h usage: %.4f (= %d%%)\n", pct5h, outData.sessionUsed);
    }

    if (s7d.length() > 0) {
        float pct7d = s7d.toFloat();
        outData.weeklyUsed  = (int)(pct7d * 100.0f);
        outData.weeklyLimit = 100;
        outData.weeklyUnit  = "%";
        outData.resetWeekly = s7dReset;

        Serial.printf("[API]   7d usage: %.4f (= %d%%)\n", pct7d, outData.weeklyUsed);
    }

    // --- API key users: per-request/token headers (fallback) ---
    if (s5h.length() == 0) {
        String reqLimit     = http.header("anthropic-ratelimit-requests-limit");
        String reqRemaining = http.header("anthropic-ratelimit-requests-remaining");

        if (reqLimit.length() > 0) {
            int limit     = reqLimit.toInt();
            int remaining = reqRemaining.toInt();
            outData.sessionUsed  = limit - remaining;
            outData.sessionLimit = limit;
            outData.sessionUnit  = "req";

            Serial.printf("[API]   Requests: %d / %d\n", outData.sessionUsed, outData.sessionLimit);
        } else {
            // No recognisable rate-limit headers — still mark valid
            Serial.println("[API]   Warning: no rate-limit headers found");
            outData.sessionUsed  = 0;
            outData.sessionLimit = 0;
            outData.sessionUnit  = "req";
        }

        // Weekly not available via API key headers
        outData.weeklyUsed  = 0;
        outData.weeklyLimit = 0;
        outData.weeklyUnit  = "req";
    }

    outData.valid    = true;
    outData.status   = "idle";
    outData.errorMsg = "";

    Serial.println("[API] Parse OK");
    return ApiResult::OK;
}

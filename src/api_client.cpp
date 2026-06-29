// ============================================================
// api_client.cpp — Fetch usage data from configurable API
// ESP32 Claude Code Companion
// ============================================================

#include "api_client.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// --------------- Constructor ---------------
ApiClient::ApiClient()
    : _apiUrl(DEFAULT_API_URL)
    , _apiKey(DEFAULT_API_KEY)
{}

// --------------- Configure endpoint ---------------
void ApiClient::setEndpoint(const String& url, const String& apiKey) {
    _apiUrl = url;
    _apiKey = apiKey;
    Serial.print("[API] Endpoint set: ");
    Serial.println(_apiUrl);
}

// --------------- Main fetch ---------------
ApiResult ApiClient::fetchUsage(UsageData& outData) {
    if (WiFi.status() != WL_CONNECTED) {
        _lastError = "No WiFi connection";
        return ApiResult::ERR_NO_WIFI;
    }

    Serial.print("[API] GET ");
    Serial.println(_apiUrl);

    String body;
    int httpCode = httpGet(_apiUrl, body);

    if (httpCode <= 0) {
        _lastError = "HTTP connect error: " + String(httpCode);
        return ApiResult::ERR_HTTP_CONNECT;
    }

    if (httpCode != 200) {
        _lastError = "HTTP " + String(httpCode);
        return ApiResult::ERR_HTTP_STATUS;
    }

    Serial.print("[API] Response (");
    Serial.print(body.length());
    Serial.println(" bytes)");

    return parseResponse(body, outData);
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

// --------------- Parse JSON response ---------------
ApiResult ApiClient::parseResponse(const String& body, UsageData& outData) {
    // Expected JSON:
    // {
    //   "daily_usage":  { "used": 150,  "limit": 500,  "unit": "requests" },
    //   "weekly_usage": { "used": 2100, "limit": 3500, "unit": "requests" },
    //   "cost":         { "current": 12.50, "limit": 50.00, "currency": "USD" },
    //   "reset":        { "daily": "2024-01-15T00:00:00Z", "weekly": "2024-01-21T00:00:00Z" },
    //   "plan":         "Pro"
    // }

    // Use ArduinoJson v7 (JsonDocument is auto-sized)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
        _lastError = String("JSON parse: ") + err.c_str();
        Serial.print("[API] JSON error: ");
        Serial.println(_lastError);
        return ApiResult::ERR_JSON_PARSE;
    }

    // --- daily_usage ---
    if (doc["daily_usage"].is<JsonObject>()) {
        outData.dailyUsed  = doc["daily_usage"]["used"]  | 0;
        outData.dailyLimit = doc["daily_usage"]["limit"] | 0;
        outData.dailyUnit  = doc["daily_usage"]["unit"]  | "req";
    } else {
        outData.dailyUsed  = 0;
        outData.dailyLimit = 0;
        outData.dailyUnit  = "req";
    }

    // --- weekly_usage ---
    if (doc["weekly_usage"].is<JsonObject>()) {
        outData.weeklyUsed  = doc["weekly_usage"]["used"]  | 0;
        outData.weeklyLimit = doc["weekly_usage"]["limit"] | 0;
        outData.weeklyUnit  = doc["weekly_usage"]["unit"]  | "req";
    } else {
        outData.weeklyUsed  = 0;
        outData.weeklyLimit = 0;
        outData.weeklyUnit  = "req";
    }

    // --- cost ---
    if (doc["cost"].is<JsonObject>()) {
        outData.costCurrent  = doc["cost"]["current"]  | 0.0f;
        outData.costLimit    = doc["cost"]["limit"]    | 0.0f;
        outData.costCurrency = doc["cost"]["currency"] | "USD";
    } else {
        outData.costCurrent  = 0.0f;
        outData.costLimit    = 0.0f;
        outData.costCurrency = "USD";
    }

    // --- reset ---
    if (doc["reset"].is<JsonObject>()) {
        outData.resetDaily  = doc["reset"]["daily"]  | "";
        outData.resetWeekly = doc["reset"]["weekly"] | "";
    } else {
        outData.resetDaily  = "";
        outData.resetWeekly = "";
    }

    // --- plan ---
    outData.plan = doc["plan"] | "Free";

    outData.valid    = true;
    outData.errorMsg = "";

    Serial.println("[API] Parse OK");
    Serial.printf("[API]   Daily: %d/%d %s\n",
                  outData.dailyUsed, outData.dailyLimit,
                  outData.dailyUnit.c_str());
    Serial.printf("[API]   Weekly: %d/%d %s\n",
                  outData.weeklyUsed, outData.weeklyLimit,
                  outData.weeklyUnit.c_str());
    Serial.printf("[API]   Cost: %.2f/%.2f %s\n",
                  outData.costCurrent, outData.costLimit,
                  outData.costCurrency.c_str());
    Serial.printf("[API]   Plan: %s\n", outData.plan.c_str());

    return ApiResult::OK;
}

// --------------- HTTP GET ---------------
int ApiClient::httpGet(const String& url, String& responseBody) {
    HTTPClient http;
    WiFiClientSecure secureClient;

    // For HTTPS: disable certificate verification for simplicity
    // (in production, load a root CA cert)
    secureClient.setInsecure();

    bool isHttps = url.startsWith("https://");

    if (isHttps) {
        if (!http.begin(secureClient, url)) {
            _lastError = "HTTPS begin failed";
            return -1;
        }
    } else {
        WiFiClient plainClient;
        if (!http.begin(plainClient, url)) {
            _lastError = "HTTP begin failed";
            return -1;
        }
    }

    // Set headers
    http.setTimeout(10000); // 10 second timeout
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "Claudy-ESP32/1.0");

    if (_apiKey.length() > 0) {
        http.addHeader("Authorization", "Bearer " + _apiKey);
    }

    int httpCode = http.GET();

    if (httpCode > 0) {
        responseBody = http.getString();
    } else {
        _lastError = http.errorToString(httpCode);
    }

    http.end();
    return httpCode;
}

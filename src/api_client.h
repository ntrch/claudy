#pragma once

// ============================================================
// api_client.h — Fetch usage data from API endpoint header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "display.h"   // for UsageData struct
#include "config.h"

// Result codes for API fetch
enum class ApiResult {
    OK,
    ERR_NO_WIFI,
    ERR_HTTP_CONNECT,
    ERR_HTTP_STATUS,
    ERR_JSON_PARSE,
    ERR_TIMEOUT,
};

class ApiClient {
public:
    ApiClient();

    // Configure endpoint
    void setEndpoint(const String& url, const String& apiKey);

    // Fetch usage data; fills outData on success
    // Returns ApiResult::OK on success
    ApiResult fetchUsage(UsageData& outData);

    // Human-readable error string for the last failed fetch
    String lastErrorString() const { return _lastError; }

    // Retry logic: call this to attempt fetch with backoff
    // Returns true if eventually succeeded within maxRetries
    bool fetchWithRetry(UsageData& outData, int maxRetries = API_RETRY_MAX);

private:
    String  _apiUrl;
    String  _apiKey;
    String  _lastError;

    // Parse JSON body into UsageData
    ApiResult parseResponse(const String& body, UsageData& outData);

    // HTTP GET helper; returns HTTP status code, body via out param
    int httpGet(const String& url, String& responseBody);
};

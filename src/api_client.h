#pragma once

// ============================================================
// api_client.h — Fetch usage data from Anthropic API headers
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <HTTPClient.h>
#include "display.h"   // for UsageData struct
#include "config.h"

// Result codes for API fetch
enum class ApiResult {
    OK,
    ERR_NO_WIFI,
    ERR_HTTP_CONNECT,
    ERR_HTTP_STATUS,
    ERR_TIMEOUT,
};

// Auth types supported by Anthropic API
enum class AuthType {
    OAUTH_TOKEN,  // Pro/Max/Team subscribers: Bearer sk-ant-oat01-...
    API_KEY,      // API key users: x-api-key sk-ant-api03-...
};

class ApiClient {
public:
    ApiClient();

    // Configure authentication
    void setAuth(AuthType type, const String& token);

    // Fetch usage data via Anthropic API rate-limit headers
    // Fills outData on success
    ApiResult fetchUsage(UsageData& outData);

    // Human-readable error string for the last failed fetch
    String lastErrorString() const { return _lastError; }

    // Retry logic: call this to attempt fetch with backoff
    // Returns true if eventually succeeded within maxRetries
    bool fetchWithRetry(UsageData& outData, int maxRetries = API_RETRY_MAX);

private:
    AuthType _authType;
    String   _authToken;
    String   _lastError;

    // Parse response headers into UsageData
    ApiResult parseHeaders(HTTPClient& http, UsageData& outData);
};

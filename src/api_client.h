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
    OAUTH_TOKEN,  // Pro/Max/Team subscribers: Bearer access token
    API_KEY,      // API key users: x-api-key sk-ant-api03-...
};

// Forward declaration to avoid circular include
class WifiSetupManager;

class ApiClient {
public:
    ApiClient();

    // Configure authentication
    void setAuth(AuthType type, const String& token);

    // Set OAuth credentials for refresh flow
    void setOAuthCredentials(const String& refreshToken, const String& clientId, const String& accessToken);

    // Provide reference to wifi manager to save refreshed access tokens
    void setWifiManager(WifiSetupManager* wm) { _wifiMgr = wm; }

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
    String   _authToken;      // API key or OAuth access token
    String   _refreshToken;   // OAuth refresh token
    String   _clientId;       // OAuth client ID
    String   _lastError;
    bool     _refreshAttempted;

    WifiSetupManager* _wifiMgr;

    // Parse response headers into UsageData
    ApiResult parseHeaders(HTTPClient& http, UsageData& outData);

    // Exchange refresh token for new access token
    bool refreshOAuthToken();
};

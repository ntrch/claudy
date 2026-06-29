#pragma once

// ============================================================
// display.h — OLED display management header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// --------------- Usage Data Structure ---------------
struct UsageData {
    // Daily usage
    int dailyUsed;
    int dailyLimit;
    String dailyUnit;

    // Weekly usage
    int weeklyUsed;
    int weeklyLimit;
    String weeklyUnit;

    // Cost
    float costCurrent;
    float costLimit;
    String costCurrency;

    // Reset times (raw ISO string, parsed locally)
    String resetDaily;   // e.g. "2024-01-15T00:00:00Z"
    String resetWeekly;  // e.g. "2024-01-21T00:00:00Z"

    // Plan
    String plan;

    // State flags
    bool valid;          // true if data has been fetched successfully
    String errorMsg;     // non-empty if last fetch failed
};

// --------------- Display Manager Class ---------------
class DisplayManager {
public:
    DisplayManager();

    // Lifecycle
    bool begin();
    void clear();
    void show();

    // Screen control
    void setScreen(uint8_t screen);
    void nextScreen();
    uint8_t currentScreen() const { return _currentScreen; }

    // Data update
    void setUsageData(const UsageData& data);
    void setWifiStatus(bool connected, int rssi = 0);
    void setError(const String& message);
    void clearError();

    // Rendering
    void render();
    void renderUsageScreen();
    void renderCostScreen();
    void renderErrorScreen();
    void renderNoDataScreen();

    // Dimming
    void setDimmed(bool dimmed);
    bool isDimmed() const { return _dimmed; }

    // Raw access for animations
    Adafruit_SSD1306& getDisplay() { return _display; }

private:
    Adafruit_SSD1306 _display;
    UsageData        _data;
    uint8_t          _currentScreen;
    bool             _wifiConnected;
    int              _wifiRssi;
    bool             _dimmed;
    bool             _hasError;
    String           _errorMsg;

    // Helpers
    void drawHeader(const char* title);
    void drawProgressBar(int x, int y, int w, int h, float pct);
    void drawWifiIcon(int x, int y, bool connected, int rssi);
    String formatTimeUntil(const String& isoTimestamp);
    float safePercent(int used, int limit);
};

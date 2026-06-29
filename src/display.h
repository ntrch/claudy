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
    // Session (daily) usage
    int sessionUsed;
    int sessionLimit;
    String sessionUnit;

    // Weekly usage
    int weeklyUsed;
    int weeklyLimit;
    String weeklyUnit;

    // Cost
    float costCurrent;
    float costLimit;
    String costCurrency;

    // Reset times (raw ISO string, parsed locally)
    String resetSession; // e.g. "2024-01-15T00:00:00Z"
    String resetWeekly;  // e.g. "2024-01-21T00:00:00Z"

    // Backwards-compatible aliases
    int   dailyUsed;
    int   dailyLimit;
    String dailyUnit;
    String resetDaily;

    // Plan / status
    String plan;
    String status;   // "running", "idle", "thinking", etc.

    // State flags
    bool valid;      // true if data has been fetched successfully
    String errorMsg; // non-empty if last fetch failed
};

// --------------- Display Screens ---------------
#define SCREEN_STATUS  0
#define SCREEN_SESSION 1
#define SCREEN_WEEKLY  2
#undef  SCREEN_COUNT
#define SCREEN_COUNT   3

// --------------- Typing Animation State ---------------
struct TypingState {
    uint8_t  msgIndex;      // which demo message is active
    uint16_t charPos;       // how many chars shown
    bool     erasing;       // false = typing, true = erasing
    uint32_t lastCharMs;    // millis() of last char add/remove
    uint32_t pauseUntilMs;  // millis() when pause ends (0 = not pausing)
    bool     cursorVisible; // blink state
    uint32_t lastCursorMs;  // millis() of last cursor blink
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

    // Rendering (non-blocking — uses millis() for typing animation)
    void render();
    void renderStatusScreen();
    void renderSessionScreen();
    void renderWeeklyScreen();
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

    // Typing animation state (for status screen)
    TypingState      _typing;

    // Helpers
    void drawProgressBar(int x, int y, int w, int h, float pct);
    void drawWifiIcon(int x, int y, bool connected, int rssi);
    String formatTimeUntil(const String& isoTimestamp);
    float safePercent(int used, int limit);

    // Typing animation helpers
    void initTyping();
    void updateTyping();
    String getCurrentTypingText();
};

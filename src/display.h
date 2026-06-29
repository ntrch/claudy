#pragma once

// ============================================================
// display.h — OLED display management header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// --------------- Usage Data Structure ---------------
struct UsageData {
    // Session (5-hour) usage
    int sessionUsed;      // percentage (0-100) for subscribers, count for API key
    int sessionLimit;     // 100 for subscribers, actual limit for API key
    String sessionUnit;   // "%" or "req"

    // Weekly (7-day) usage
    int weeklyUsed;
    int weeklyLimit;
    String weeklyUnit;    // "%" or "req"

    // Reset times (ISO strings from Anthropic headers)
    String resetSession;  // e.g. "2024-01-15T00:00:00Z"
    String resetWeekly;   // e.g. "2024-01-21T00:00:00Z"

    // Status
    String status;        // "running", "idle", "thinking", etc.

    // State flags
    bool valid;           // true if data has been fetched successfully
    String errorMsg;      // non-empty if last fetch failed
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

    // For animations: draw a full 128x64 bitmap frame and send to display
    void drawFrame(const uint8_t* frameBitmap);
    void sendBuffer();

    // Sleep/wake
    void sleepDisplay();
    void wakeDisplay();
    bool isSleeping() const { return _sleeping; }

    // Raw U8g2 access (for direct use if needed)
    U8G2_SH1106_128X64_NONAME_F_HW_I2C& getU8g2() { return _u8g2; }

private:
    U8G2_SH1106_128X64_NONAME_F_HW_I2C _u8g2;
    UsageData   _data;
    uint8_t     _currentScreen;
    bool        _wifiConnected;
    int         _wifiRssi;
    bool        _dimmed;
    bool        _sleeping;
    bool        _hasError;
    String      _errorMsg;

    // Typing animation state (for status screen)
    TypingState _typing;

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

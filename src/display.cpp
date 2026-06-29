// ============================================================
// display.cpp — OLED display management implementation
// ESP32 Claude Code Companion
// ============================================================

#include "display.h"
#include <Wire.h>

// --------------- Demo messages for CLI typing effect ---------------
static const char* const kTypingMessages[] = {
    "coding auth flow",
    "debugging api client",
    "writing tests",
    "reviewing PR #42",
    "refactoring queries",
    "updating docs",
    "deploying staging",
};
static const uint8_t kTypingMessageCount = 7;

// --------------- Constructor ---------------
DisplayManager::DisplayManager()
    : _u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN)
    , _currentScreen(SCREEN_STATUS)
    , _wifiConnected(false)
    , _wifiRssi(0)
    , _dimmed(false)
    , _sleeping(false)
    , _hasError(false)
{
    _data.valid          = false;
    _data.sessionUsed    = 0;
    _data.sessionLimit   = 0;
    _data.weeklyUsed     = 0;
    _data.weeklyLimit    = 0;
    _data.status         = "idle";
    _data.sessionUnit    = "req";
    _data.weeklyUnit     = "req";

    initTyping();
}

// --------------- Lifecycle ---------------
bool DisplayManager::begin() {
    _u8g2.begin();
    _u8g2.clearBuffer();
    _u8g2.sendBuffer();
    Serial.println("[Display] SH1106 U8g2 init OK");
    return true;
}

void DisplayManager::clear() {
    _u8g2.clearBuffer();
}

void DisplayManager::show() {
    _u8g2.sendBuffer();
}

// --------------- Screen control ---------------
void DisplayManager::setScreen(uint8_t screen) {
    if (screen < SCREEN_COUNT) {
        _currentScreen = screen;
    }
}

void DisplayManager::nextScreen() {
    _currentScreen = (_currentScreen + 1) % SCREEN_COUNT;
}

// --------------- Data update ---------------
void DisplayManager::setUsageData(const UsageData& data) {
    _data     = data;
    _hasError = false;
}

void DisplayManager::setWifiStatus(bool connected, int rssi) {
    _wifiConnected = connected;
    _wifiRssi      = rssi;
}

void DisplayManager::setError(const String& message) {
    _hasError = true;
    _errorMsg = message;
}

void DisplayManager::clearError() {
    _hasError = false;
    _errorMsg = "";
}

// --------------- Dimming ---------------
void DisplayManager::setDimmed(bool dimmed) {
    _dimmed = dimmed;
    _u8g2.setContrast(dimmed ? 16 : 255);
}

// --------------- Sleep/Wake ---------------
void DisplayManager::sleepDisplay() {
    _sleeping = true;
    _u8g2.setPowerSave(1);
}

void DisplayManager::wakeDisplay() {
    _sleeping = false;
    _u8g2.setPowerSave(0);
}

// --------------- Main render dispatcher ---------------
void DisplayManager::render() {
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tr);

    if (_hasError) {
        renderErrorScreen();
    } else if (!_data.valid) {
        renderNoDataScreen();
    } else {
        switch (_currentScreen) {
            case SCREEN_STATUS:  renderStatusScreen();  break;
            case SCREEN_SESSION: renderSessionScreen(); break;
            case SCREEN_WEEKLY:  renderWeeklyScreen();  break;
            default:             renderStatusScreen();  break;
        }
    }

    _u8g2.sendBuffer();
}

// ============================================================
// Screen 0: CLI Status Screen — terminal frame with typing effect
// ============================================================
void DisplayManager::renderStatusScreen() {
    _u8g2.setFont(u8g2_font_6x10_tr);

    // --- Top: status text, no frame ---
    // y is baseline in U8g2; y=8 puts text at top of screen
    char statusBuf[32];
    String statusStr = (_data.status.length() > 0) ? _data.status : "idle";
    snprintf(statusBuf, sizeof(statusBuf), "*%s", statusStr.c_str());
    _u8g2.drawStr(0, 8, statusBuf);

    // WiFi icon top-right
    drawWifiIcon(116, 0, _wifiConnected, _wifiRssi);

    // --- Middle: framed typing area ---
    const int boxY = 16;
    const int boxH = 32;
    _u8g2.drawFrame(0, boxY, 128, boxH);

    // Typing effect inside the frame
    updateTyping();
    String typedText = getCurrentTypingText();
    bool showCursor = _typing.cursorVisible;

    String line = "> /" + typedText;
    if (showCursor) line += "_";
    // y = boxY + 20 puts baseline roughly centered in the frame
    _u8g2.drawStr(4, boxY + 20, line.c_str());
}

// ============================================================
// Screen 1: Session Usage Screen
// ============================================================
void DisplayManager::renderSessionScreen() {
    _u8g2.setFont(u8g2_font_6x10_tr);

    // Header (y=8 = baseline at top)
    _u8g2.drawStr(0, 8, "Session Usage");
    drawWifiIcon(116, 0, _wifiConnected, _wifiRssi);
    _u8g2.drawHLine(0, 10, 128);

    // Usage numbers: e.g. "45 / 100 %" or "150 / 500 req"
    char usageBuf[32];
    snprintf(usageBuf, sizeof(usageBuf), "%d / %d %s",
             _data.sessionUsed, _data.sessionLimit,
             _data.sessionUnit.c_str());
    _u8g2.drawStr(0, 22, usageBuf);

    // Progress bar
    float pct = safePercent(_data.sessionUsed, _data.sessionLimit);
    drawProgressBar(0, 26, PROGRESS_BAR_WIDTH, 8, pct);

    // Percentage right of bar (baseline at y=34 aligns with bar)
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(pct * 100));
    _u8g2.drawStr(PROGRESS_BAR_WIDTH + 2, 34, pctBuf);

    // Reset time
    String resetStr = "Resets: " + formatTimeUntil(_data.resetSession);
    _u8g2.drawStr(0, 48, resetStr.c_str());
}

// ============================================================
// Screen 2: Weekly Usage Screen
// ============================================================
void DisplayManager::renderWeeklyScreen() {
    _u8g2.setFont(u8g2_font_6x10_tr);

    // Header
    _u8g2.drawStr(0, 8, "Weekly Usage");
    drawWifiIcon(116, 0, _wifiConnected, _wifiRssi);
    _u8g2.drawHLine(0, 10, 128);

    // Usage numbers
    char usageBuf[32];
    snprintf(usageBuf, sizeof(usageBuf), "%d / %d %s",
             _data.weeklyUsed, _data.weeklyLimit,
             _data.weeklyUnit.c_str());
    _u8g2.drawStr(0, 22, usageBuf);

    // Progress bar
    float pct = safePercent(_data.weeklyUsed, _data.weeklyLimit);
    drawProgressBar(0, 26, PROGRESS_BAR_WIDTH, 8, pct);

    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(pct * 100));
    _u8g2.drawStr(PROGRESS_BAR_WIDTH + 2, 34, pctBuf);

    // Weekly reset
    String wr = _data.resetWeekly;
    if (wr.length() >= 16) {
        String datePart = wr.substring(5, 10);   // "MM-DD"
        String timePart = wr.substring(11, 16);  // "HH:MM"
        String resetStr = "Resets: " + datePart + " " + timePart;
        _u8g2.drawStr(0, 48, resetStr.c_str());
    } else {
        _u8g2.drawStr(0, 48, "Resets: --/-- --:--");
    }
}

// --------------- Error Screen ---------------
void DisplayManager::renderErrorScreen() {
    _u8g2.setFont(u8g2_font_6x10_tr);

    _u8g2.drawStr(0, 8, "CLAUDY");
    _u8g2.drawHLine(0, 10, 128);

    _u8g2.drawStr(0, 22, "! ERROR !");

    String msg = _errorMsg;
    if (msg.length() > 42) msg = msg.substring(0, 42);
    char line1[22];
    char line2[22];
    snprintf(line1, sizeof(line1), "%s", msg.substring(0, 21).c_str());
    _u8g2.drawStr(0, 34, line1);
    if (msg.length() > 21) {
        snprintf(line2, sizeof(line2), "%s", msg.substring(21).c_str());
        _u8g2.drawStr(0, 44, line2);
    }

    _u8g2.drawStr(0, 60, "Retrying...");
}

// --------------- No Data Screen ---------------
void DisplayManager::renderNoDataScreen() {
    _u8g2.setFont(u8g2_font_6x10_tr);

    _u8g2.drawStr(0, 8, "CLAUDY");
    _u8g2.drawHLine(0, 10, 128);

    _u8g2.drawStr(20, 36, "Fetching data...");
}

// ============================================================
// Animation: draw a full-screen bitmap from PROGMEM
// Converts horizontal MSB-first (Adafruit) to U8g2 vertical tile format
// ============================================================
void DisplayManager::drawFrame(const uint8_t* frameBitmap) {
    _u8g2.clearBuffer();
    uint8_t* buf = _u8g2.getBufferPtr();

    // U8g2 buffer for SH1106 128x64: organized as 8 tile rows,
    // each tile row is 128 bytes (one byte per column, 8 vertical pixels per byte).
    // Source bitmap: horizontal, MSB-first, 16 bytes per row, 64 rows.

    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            // Read pixel from horizontal bitmap (MSB first)
            int srcByte = y * 16 + x / 8;
            int srcBit  = 7 - (x % 8);
            uint8_t pixel = (pgm_read_byte(&frameBitmap[srcByte]) >> srcBit) & 1;

            // Write pixel to U8g2 vertical tile buffer
            int tileRow  = y / 8;
            int bitInTile = y % 8;
            int bufIdx   = tileRow * 128 + x;
            if (pixel) {
                buf[bufIdx] |= (1 << bitInTile);
            }
        }
    }
}

void DisplayManager::sendBuffer() {
    _u8g2.sendBuffer();
}

// ============================================================
// Private Helpers
// ============================================================

void DisplayManager::drawProgressBar(int x, int y, int w, int h, float pct) {
    _u8g2.drawFrame(x, y, w, h);
    int fillW = (int)((w - 2) * pct);
    if (fillW < 0) fillW = 0;
    if (fillW > w - 2) fillW = w - 2;
    if (fillW > 0) {
        _u8g2.drawBox(x + 1, y + 1, fillW, h - 2);
    }
}

void DisplayManager::drawWifiIcon(int x, int y, bool connected, int rssi) {
    if (!connected) {
        _u8g2.drawLine(x, y, x + 4, y + 4);
        _u8g2.drawLine(x + 4, y, x, y + 4);
        return;
    }

    int level = 0;
    if      (rssi >= -50) level = 4;
    else if (rssi >= -65) level = 3;
    else if (rssi >= -75) level = 2;
    else if (rssi >= -85) level = 1;
    else                  level = 0;

    const int barW = 2;
    const int barGap = 1;
    int barHeights[4] = {3, 5, 7, 9};

    for (int i = 0; i < 4; i++) {
        int bx = x + i * (barW + barGap);
        int bh = barHeights[i];
        int by = y + 9 - bh;
        if (i < level) {
            _u8g2.drawBox(bx, by, barW, bh);
        } else {
            _u8g2.drawFrame(bx, by, barW, bh);
        }
    }
}

String DisplayManager::formatTimeUntil(const String& isoTimestamp) {
    if (isoTimestamp.length() < 16) {
        return "--:-- UTC";
    }
    String date = isoTimestamp.substring(5, 10);   // MM-DD
    String time = isoTimestamp.substring(11, 16);  // HH:MM
    return date + " " + time + "Z";
}

float DisplayManager::safePercent(int used, int limit) {
    if (limit <= 0) return 0.0f;
    float p = (float)used / (float)limit;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

// --------------- Typing animation ---------------

void DisplayManager::initTyping() {
    _typing.msgIndex      = 0;
    _typing.charPos       = 0;
    _typing.erasing       = false;
    _typing.lastCharMs    = 0;
    _typing.pauseUntilMs  = 0;
    _typing.cursorVisible = true;
    _typing.lastCursorMs  = 0;
}

void DisplayManager::updateTyping() {
    uint32_t now = millis();

    // Cursor blink every 500ms (independent of typing state)
    if (now - _typing.lastCursorMs >= 500) {
        _typing.cursorVisible = !_typing.cursorVisible;
        _typing.lastCursorMs  = now;
    }

    // If in pause, wait until pause ends
    if (_typing.pauseUntilMs > 0) {
        if (now < _typing.pauseUntilMs) return;
        // Pause ended — switch to erasing
        _typing.pauseUntilMs = 0;
        _typing.erasing      = true;
        _typing.lastCharMs   = now;
        return;
    }

    const char* msg    = kTypingMessages[_typing.msgIndex % kTypingMessageCount];
    uint16_t    msgLen = (uint16_t)strlen(msg);

    if (!_typing.erasing) {
        // Typing phase
        if (now - _typing.lastCharMs >= TYPING_CHAR_DELAY_MS) {
            _typing.lastCharMs = now;
            if (_typing.charPos < msgLen) {
                _typing.charPos++;
            }
            if (_typing.charPos >= msgLen) {
                // Fully typed — start pause before erasing
                _typing.pauseUntilMs = now + TYPING_PAUSE_MS;
            }
        }
    } else {
        // Erasing phase
        if (now - _typing.lastCharMs >= TYPING_ERASE_DELAY_MS) {
            _typing.lastCharMs = now;
            if (_typing.charPos > 0) {
                _typing.charPos--;
            }
            if (_typing.charPos == 0) {
                // Fully erased — move to next message
                _typing.erasing  = false;
                _typing.msgIndex = (_typing.msgIndex + 1) % kTypingMessageCount;
                _typing.lastCharMs = now;
            }
        }
    }
}

String DisplayManager::getCurrentTypingText() {
    const char* msg = kTypingMessages[_typing.msgIndex % kTypingMessageCount];
    uint16_t    len = (uint16_t)strlen(msg);
    uint16_t    n   = _typing.charPos;
    if (n > len) n = len;
    return String(msg).substring(0, n);
}

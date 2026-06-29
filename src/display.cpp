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
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN)
    , _currentScreen(SCREEN_STATUS)
    , _wifiConnected(false)
    , _wifiRssi(0)
    , _dimmed(false)
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
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        Serial.println("[Display] SSD1306 init FAILED");
        return false;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.display();
    Serial.println("[Display] SSD1306 init OK");
    return true;
}

void DisplayManager::clear() {
    _display.clearDisplay();
}

void DisplayManager::show() {
    _display.display();
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
    _display.ssd1306_command(dimmed ? 0x81 : 0xCF);
    _display.ssd1306_command(dimmed ? 0x10 : 0xFF);
}

// --------------- Main render dispatcher ---------------
void DisplayManager::render() {
    _display.clearDisplay();

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

    _display.display();
}

// ============================================================
// Screen 0: CLI Status Screen — terminal frame with typing effect
// ============================================================
void DisplayManager::renderStatusScreen() {
    // --- Top: status text, no frame ---
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("*");
    String statusStr = (_data.status.length() > 0) ? _data.status : "idle";
    _display.print(statusStr);

    // WiFi icon top-right
    drawWifiIcon(116, 0, _wifiConnected, _wifiRssi);

    // --- Middle: framed typing area ---
    const int boxY = 16;
    const int boxH = 32;
    _display.drawRect(0, boxY, 128, boxH, SSD1306_WHITE);

    // Typing effect inside the frame
    updateTyping();
    String typedText = getCurrentTypingText();

    bool showCursor = _typing.cursorVisible;

    _display.setCursor(4, boxY + 8);
    _display.setTextSize(1);
    _display.print("> /");
    _display.print(typedText);
    if (showCursor) {
        _display.print("_");
    }
}

// ============================================================
// Screen 1: Session Usage Screen
// ============================================================
void DisplayManager::renderSessionScreen() {
    // Header
    _display.setTextSize(1);
    _display.setCursor(0, 1);
    _display.print("Session Usage");
    drawWifiIcon(116, 1, _wifiConnected, _wifiRssi);
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    // Usage numbers: e.g. "45 / 100 %" or "150 / 500 req"
    _display.setCursor(0, 13);
    char usageBuf[32];
    snprintf(usageBuf, sizeof(usageBuf), "%d / %d %s",
             _data.sessionUsed, _data.sessionLimit,
             _data.sessionUnit.c_str());
    _display.print(usageBuf);

    // Big progress bar (full width minus percentage label)
    float pct = safePercent(_data.sessionUsed, _data.sessionLimit);
    drawProgressBar(0, 24, PROGRESS_BAR_WIDTH, 8, pct);

    // Percentage right of bar
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(pct * 100));
    _display.setCursor(PROGRESS_BAR_WIDTH + 2, 25);
    _display.print(pctBuf);

    // Reset time
    _display.setCursor(0, 38);
    _display.print("Resets: ");
    _display.print(formatTimeUntil(_data.resetSession));
}

// ============================================================
// Screen 2: Weekly Usage Screen
// ============================================================
void DisplayManager::renderWeeklyScreen() {
    // Header
    _display.setTextSize(1);
    _display.setCursor(0, 1);
    _display.print("Weekly Usage");
    drawWifiIcon(116, 1, _wifiConnected, _wifiRssi);
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    // Usage numbers
    _display.setCursor(0, 13);
    char usageBuf[32];
    snprintf(usageBuf, sizeof(usageBuf), "%d / %d %s",
             _data.weeklyUsed, _data.weeklyLimit,
             _data.weeklyUnit.c_str());
    _display.print(usageBuf);

    // Progress bar
    float pct = safePercent(_data.weeklyUsed, _data.weeklyLimit);
    drawProgressBar(0, 24, PROGRESS_BAR_WIDTH, 8, pct);

    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(pct * 100));
    _display.setCursor(PROGRESS_BAR_WIDTH + 2, 25);
    _display.print(pctBuf);

    // Weekly reset
    _display.setCursor(0, 38);
    _display.print("Resets: ");
    // Parse ISO date to "MM-DD HH:MM" style
    String wr = _data.resetWeekly;
    if (wr.length() >= 16) {
        String datePart = wr.substring(5, 10);   // "MM-DD"
        String timePart = wr.substring(11, 16);  // "HH:MM"
        _display.print(datePart + " " + timePart);
    } else {
        _display.print("--/-- --:--");
    }
}

// --------------- Error Screen ---------------
void DisplayManager::renderErrorScreen() {
    _display.setTextSize(1);
    _display.setCursor(0, 1);
    _display.print("CLAUDY");
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    _display.setCursor(0, 14);
    _display.print("! ERROR !");

    _display.setCursor(0, 26);
    String msg = _errorMsg;
    if (msg.length() > 42) msg = msg.substring(0, 42);
    _display.print(msg.substring(0, 21));
    if (msg.length() > 21) {
        _display.setCursor(0, 36);
        _display.print(msg.substring(21));
    }

    _display.setCursor(0, 52);
    _display.print("Retrying...");
}

// --------------- No Data Screen ---------------
void DisplayManager::renderNoDataScreen() {
    _display.setTextSize(1);
    _display.setCursor(0, 1);
    _display.print("CLAUDY");
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    _display.setCursor(20, 28);
    _display.print("Fetching data...");
}

// ============================================================
// Private Helpers
// ============================================================

void DisplayManager::drawProgressBar(int x, int y, int w, int h, float pct) {
    _display.drawRect(x, y, w, h, SSD1306_WHITE);
    int fillW = (int)((w - 2) * pct);
    if (fillW < 0) fillW = 0;
    if (fillW > w - 2) fillW = w - 2;
    if (fillW > 0) {
        _display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawWifiIcon(int x, int y, bool connected, int rssi) {
    if (!connected) {
        _display.drawLine(x, y, x + 4, y + 4, SSD1306_WHITE);
        _display.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
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
            _display.fillRect(bx, by, barW, bh, SSD1306_WHITE);
        } else {
            _display.drawRect(bx, by, barW, bh, SSD1306_WHITE);
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

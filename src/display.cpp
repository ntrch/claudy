// ============================================================
// display.cpp — OLED display management implementation
// ESP32 Claude Code Companion
// ============================================================

#include "display.h"
#include <Wire.h>

// --------------- Constructor ---------------
DisplayManager::DisplayManager()
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN)
    , _currentScreen(SCREEN_USAGE)
    , _wifiConnected(false)
    , _wifiRssi(0)
    , _dimmed(false)
    , _hasError(false)
{
    _data.valid = false;
    _data.dailyUsed   = 0;
    _data.dailyLimit  = 0;
    _data.weeklyUsed  = 0;
    _data.weeklyLimit = 0;
    _data.costCurrent = 0.0f;
    _data.costLimit   = 0.0f;
    _data.plan        = "---";
    _data.dailyUnit   = "req";
    _data.weeklyUnit  = "req";
    _data.costCurrency = "USD";
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
    _data = data;
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
    _display.ssd1306_command(dimmed ? 0x81 : 0xCF); // contrast command
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
            case SCREEN_USAGE: renderUsageScreen(); break;
            case SCREEN_COST:  renderCostScreen();  break;
            default:           renderUsageScreen(); break;
        }
    }

    _display.display();
}

// --------------- Screen 1: Usage Overview ---------------
void DisplayManager::renderUsageScreen() {
    drawHeader("CLAUDY");

    // Separator line
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    // Daily usage label
    _display.setCursor(0, 13);
    _display.setTextSize(1);
    _display.print("Daily: ");
    _display.print(_data.dailyUsed);
    _display.print("/");
    _display.print(_data.dailyLimit);

    // Daily progress bar
    float dailyPct = safePercent(_data.dailyUsed, _data.dailyLimit);
    drawProgressBar(0, 22, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, dailyPct);

    // Percentage label next to bar
    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(dailyPct * 100));
    _display.setCursor(PROGRESS_BAR_WIDTH + 2, 22);
    _display.setTextSize(1);
    _display.print(pctBuf);

    // Weekly usage label
    _display.setCursor(0, 32);
    _display.print("Weekly:");
    _display.print(_data.weeklyUsed);
    _display.print("/");
    _display.print(_data.weeklyLimit);

    // Weekly progress bar
    float weeklyPct = safePercent(_data.weeklyUsed, _data.weeklyLimit);
    drawProgressBar(0, 41, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, weeklyPct);
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(weeklyPct * 100));
    _display.setCursor(PROGRESS_BAR_WIDTH + 2, 41);
    _display.print(pctBuf);

    // Time until daily reset
    _display.setCursor(0, 56);
    String resetStr = "Reset: " + formatTimeUntil(_data.resetDaily);
    _display.print(resetStr.substring(0, 21)); // truncate to fit
}

// --------------- Screen 2: Cost & Details ---------------
void DisplayManager::renderCostScreen() {
    drawHeader("CLAUDY");
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    // Cost line
    _display.setCursor(0, 13);
    _display.setTextSize(1);

    char costBuf[32];
    snprintf(costBuf, sizeof(costBuf), "Cost: $%.2f/$%.2f",
             _data.costCurrent, _data.costLimit);
    _display.print(costBuf);

    // Cost progress bar
    float costPct = (_data.costLimit > 0.0f)
                    ? (_data.costCurrent / _data.costLimit)
                    : 0.0f;
    if (costPct > 1.0f) costPct = 1.0f;
    drawProgressBar(0, 22, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, costPct);

    char pctBuf[6];
    snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(costPct * 100));
    _display.setCursor(PROGRESS_BAR_WIDTH + 2, 22);
    _display.print(pctBuf);

    // Weekly reset label
    _display.setCursor(0, 38);
    _display.print("Weekly Reset:");

    // Parse weekly reset date into human-readable form
    // Format: "Mon 00:00 UTC" from ISO string
    String wr = _data.resetWeekly;
    String wrDisplay = "--/-- 00:00 UTC";
    // ISO format: 2024-01-21T00:00:00Z  → extract MM-DD HH:MM
    if (wr.length() >= 16) {
        // "2024-01-21T00:00:00Z"
        //  0123456789012345
        String datePart = wr.substring(5, 10);  // "01-21"
        String timePart = wr.substring(11, 16); // "00:00"
        wrDisplay = datePart + " " + timePart + " UTC";
    }
    _display.setCursor(0, 48);
    _display.print(wrDisplay);
}

// --------------- Error Screen ---------------
void DisplayManager::renderErrorScreen() {
    drawHeader("CLAUDY");
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    _display.setCursor(0, 14);
    _display.setTextSize(1);
    _display.print("! ERROR !");

    _display.setCursor(0, 26);
    // Word-wrap rudimentary: print up to 21 chars per line
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
    drawHeader("CLAUDY");
    _display.drawFastHLine(0, 10, OLED_SCREEN_WIDTH, SSD1306_WHITE);

    _display.setCursor(20, 28);
    _display.setTextSize(1);
    _display.print("Fetching data...");
}

// --------------- Private Helpers ---------------

void DisplayManager::drawHeader(const char* title) {
    _display.setTextSize(1);
    _display.setCursor(0, 1);
    _display.print(title);

    // WiFi icon at top-right
    drawWifiIcon(104, 1, _wifiConnected, _wifiRssi);

    // Plan label (right of wifi icon area)
    if (_data.valid && _data.plan.length() > 0) {
        // Render plan to the left of wifi icon
        int planX = 104 - (_data.plan.length() * 6) - 2;
        _display.setCursor(planX, 1);
        _display.print(_data.plan);
    }
}

void DisplayManager::drawProgressBar(int x, int y, int w, int h, float pct) {
    // Outer border
    _display.drawRect(x, y, w, h, SSD1306_WHITE);

    // Inner fill
    int fillW = (int)((w - 2) * pct);
    if (fillW < 0) fillW = 0;
    if (fillW > w - 2) fillW = w - 2;

    if (fillW > 0) {
        _display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
    }
}

void DisplayManager::drawWifiIcon(int x, int y, bool connected, int rssi) {
    // 4-level WiFi bars (each bar is 3px wide, heights: 3,5,7,9)
    // Bars: leftmost = weakest
    if (!connected) {
        // Draw an X
        _display.drawLine(x, y, x + 4, y + 4, SSD1306_WHITE);
        _display.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
        return;
    }

    // Determine signal level 0-4 from RSSI
    int level = 0;
    if      (rssi >= -50) level = 4;
    else if (rssi >= -65) level = 3;
    else if (rssi >= -75) level = 2;
    else if (rssi >= -85) level = 1;
    else                  level = 0;

    // Draw 4 bars at x,y (total width ~10px)
    const int barW = 2;
    const int barGap = 1;
    int barHeights[4] = {3, 5, 7, 9};

    for (int i = 0; i < 4; i++) {
        int bx = x + i * (barW + barGap);
        int bh = barHeights[i];
        int by = y + 9 - bh; // align bottoms

        if (i < level) {
            _display.fillRect(bx, by, barW, bh, SSD1306_WHITE);
        } else {
            _display.drawRect(bx, by, barW, bh, SSD1306_WHITE);
        }
    }
}

String DisplayManager::formatTimeUntil(const String& isoTimestamp) {
    // isoTimestamp: "2024-01-15T00:00:00Z"
    // We can't do real date math easily without NTP, so we parse the hour/min
    // and show as "HH:MM UTC" if it's today, or "MM-DD" if future.
    // For a simple implementation, just show the date+time portion.
    if (isoTimestamp.length() < 16) {
        return "--:-- UTC";
    }
    // Return "MM-DD HH:MM"
    String date = isoTimestamp.substring(5, 10);  // MM-DD
    String time = isoTimestamp.substring(11, 16); // HH:MM
    return date + " " + time + "Z";
}

float DisplayManager::safePercent(int used, int limit) {
    if (limit <= 0) return 0.0f;
    float p = (float)used / (float)limit;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

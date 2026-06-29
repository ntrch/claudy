// ============================================================
// animations.cpp — Boot animation implementation
// ESP32 Claude Code Companion
// ============================================================

#include "animations.h"
#include <WiFi.h>

// --------------- Constructor ---------------
AnimationManager::AnimationManager(Adafruit_SSD1306& display)
    : _display(display)
{}

// --------------- Public: run both animations ---------------
void AnimationManager::runBootAnimations(bool& wifiConnected) {
    runTypingAnimation(BOOT_ANIM_PART1_MS);
    runConnectingAnimation(BOOT_ANIM_PART2_MS, wifiConnected);
}

// ===========================================================
// Animation 1: CLAUDY Typing + Breathing (15 seconds)
// Phase A (~7s): letters appear one by one, then blink cursor
// Phase B (~8s): whole text pulses (breathing brightness effect)
// ===========================================================
void AnimationManager::runTypingAnimation(uint32_t durationMs) {
    const char* TEXT      = "CLAUDY";
    const int   TEXT_LEN  = 6;
    const int   CHAR_W    = 18;   // big font char width (2x scale = 12px + gap)
    const int   TOTAL_W   = TEXT_LEN * CHAR_W;
    const int   START_X   = (OLED_SCREEN_WIDTH - TOTAL_W) / 2;
    const int   TEXT_Y    = 18;
    const int   TAGLINE_Y = 48;

    uint32_t start       = millis();
    uint32_t phaseAEnd   = start + 7000UL;  // typing phase: 7s
    uint32_t phaseBEnd   = start + durationMs;

    // ---- Phase A: Typing ----
    int charsShown = 0;

    while (millis() < phaseAEnd) {
        uint32_t elapsed = millis() - start;

        // Reveal one letter every ~900ms
        int targetChars = (int)(elapsed / 900) + 1;
        if (targetChars > TEXT_LEN) targetChars = TEXT_LEN;

        if (targetChars != charsShown) {
            charsShown = targetChars;
        }

        _display.clearDisplay();

        // Draw revealed characters in large font
        _display.setTextSize(2);
        _display.setTextColor(SSD1306_WHITE);
        for (int i = 0; i < charsShown; i++) {
            _display.setCursor(START_X + i * CHAR_W, TEXT_Y);
            _display.print(TEXT[i]);
        }

        // Blinking cursor after last revealed char
        bool cursorOn = ((millis() / 400) % 2 == 0);
        if (charsShown < TEXT_LEN && cursorOn) {
            _display.setCursor(START_X + charsShown * CHAR_W, TEXT_Y);
            _display.print("_");
        }

        // Tagline
        _display.setTextSize(1);
        _display.setCursor(18, TAGLINE_Y);
        _display.print("Claude Companion");

        _display.display();
        delay(40);
    }

    // ---- Phase B: Breathing / Pulsing ----
    // We simulate brightness by inverting display and using contrast control.
    // Since SSD1306 doesn't do per-pixel brightness, we use contrast + invert trick:
    // Draw white text and vary contrast via ssd1306_command.
    // Cycle: fade up → hold → fade down → repeat

    // Contrast values: 0x00 (dim) to 0xFF (bright)
    const int BREATH_CYCLE_MS = 2000; // 2-second full breath

    while (millis() < phaseBEnd) {
        uint32_t tNow    = millis();
        float    phase   = (float)((tNow % BREATH_CYCLE_MS)) / BREATH_CYCLE_MS; // 0..1
        float    bright  = easeInOut(phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f);
        uint8_t  contrast = (uint8_t)(10 + bright * 245); // 10..255

        _display.ssd1306_command(0x81);     // Set contrast command
        _display.ssd1306_command(contrast);

        _display.clearDisplay();

        // Full CLAUDY in large centered text
        _display.setTextSize(2);
        _display.setTextColor(SSD1306_WHITE);
        _display.setCursor(START_X, TEXT_Y);
        _display.print(TEXT);

        // Decorative underline
        int lineY = TEXT_Y + 18;
        _display.drawFastHLine(START_X - 2, lineY, TOTAL_W + 4, SSD1306_WHITE);

        // Tagline
        _display.setTextSize(1);
        _display.setCursor(18, TAGLINE_Y);
        _display.print("Claude Companion");

        // Small version dots (decorative)
        _display.setCursor(50, 57);
        _display.print("v1.0");

        _display.display();
        delay(30);
    }

    // Restore full contrast
    _display.ssd1306_command(0x81);
    _display.ssd1306_command(0xFF);
}

// ===========================================================
// Animation 2: WiFi Connecting (15 seconds)
// Phase A: WiFi signal bars fill up one by one, "Connecting..."
// Phase B: Dots cycle + progress bar
// If WiFi connects: show checkmark + "Connected!" + return early
// ===========================================================
void AnimationManager::runConnectingAnimation(uint32_t durationMs, bool& wifiConnected) {
    const int   CENTER_X    = OLED_SCREEN_WIDTH / 2;
    const int   BARS_Y      = 10;
    const int   STATUS_Y    = 38;
    const int   PBAR_X      = 10;
    const int   PBAR_Y      = 50;
    const int   PBAR_W      = 108;
    const int   PBAR_H      = 8;
    const int   MAX_BARS    = 5;

    uint32_t start   = millis();
    uint32_t endTime = start + durationMs;

    int  dotCount   = 0;
    int  barLevel   = 0;
    bool connected  = false;

    while (millis() < endTime) {
        uint32_t elapsed = millis() - start;
        float    progress = (float)elapsed / (float)durationMs;
        if (progress > 1.0f) progress = 1.0f;

        // Animate bars: one new bar every 1.5 seconds (cycle)
        barLevel = (int)((elapsed / 1500UL) % (MAX_BARS + 1));

        // Dot animation
        dotCount = (int)(elapsed / 500UL) % 4; // 0..3

        // Check WiFi status
        if (WiFi.status() == WL_CONNECTED) {
            connected    = true;
            wifiConnected = true;
        }

        _display.clearDisplay();

        if (connected) {
            // --- Connected! Show checkmark ---
            drawCheckmark(CENTER_X, 20);

            _display.setTextSize(1);
            _display.setCursor(28, 38);
            _display.print("WiFi Connected!");

            _display.setCursor(0, 52);
            _display.print(WiFi.localIP().toString());

            _display.display();
            delay(2000); // Show for 2 seconds then exit
            break;
        }

        // --- Still connecting ---
        _display.setTextSize(1);
        _display.setCursor(22, 0);
        _display.print("Connecting...");

        // WiFi signal bars (animated)
        drawWifiBars(CENTER_X, BARS_Y, barLevel, MAX_BARS);

        // "Connecting" label with dots
        _display.setCursor(22, STATUS_Y);
        _display.print("Please wait");
        drawDots(22 + 66, STATUS_Y, dotCount);

        // Progress bar
        drawProgressBarSimple(PBAR_X, PBAR_Y, PBAR_W, PBAR_H, progress);

        // Progress percentage
        char pctBuf[5];
        snprintf(pctBuf, sizeof(pctBuf), "%2d%%", (int)(progress * 100));
        _display.setCursor(PBAR_X + PBAR_W + 3, PBAR_Y);
        _display.print(pctBuf);

        _display.display();
        delay(40);
    }

    // If timeout without connection
    if (!connected) {
        _display.clearDisplay();
        _display.setTextSize(1);
        _display.setCursor(10, 20);
        _display.print("No WiFi found.");
        _display.setCursor(5, 34);
        _display.print("Open: Claudy-Setup");
        _display.setCursor(8, 48);
        _display.print("AP to configure.");
        _display.display();
        delay(2000);
    }
}

// --------------- Private Helpers ---------------

void AnimationManager::drawBreathingText(const char* text, uint8_t brightness) {
    _display.ssd1306_command(0x81);
    _display.ssd1306_command(brightness);
    _display.setTextSize(2);
    _display.setTextColor(SSD1306_WHITE);
    int len = strlen(text);
    int x   = (OLED_SCREEN_WIDTH - len * 12) / 2;
    _display.setCursor(x, 20);
    _display.print(text);
}

void AnimationManager::drawWifiBars(int centerX, int centerY, int level, int maxLevel) {
    // Draw concentric arcs as simplified WiFi bars using rectangles
    // Each "bar" is a taller rectangle, arranged like signal bars
    const int barW     = 6;
    const int barGap   = 3;
    const int totalW   = maxLevel * (barW + barGap) - barGap;
    const int startX   = centerX - totalW / 2;
    const int baseY    = centerY + maxLevel * 3;

    for (int i = 0; i < maxLevel; i++) {
        int bx = startX + i * (barW + barGap);
        int bh = (i + 1) * 3 + 2;
        int by = baseY - bh;

        if (i < level) {
            _display.fillRect(bx, by, barW, bh, SSD1306_WHITE);
        } else {
            _display.drawRect(bx, by, barW, bh, SSD1306_WHITE);
        }
    }

    // Small dot at the bottom center (signal source)
    _display.fillCircle(centerX, baseY + 2, 2, SSD1306_WHITE);
}

void AnimationManager::drawProgressBarSimple(int x, int y, int w, int h, float pct) {
    _display.drawRect(x, y, w, h, SSD1306_WHITE);
    int fillW = (int)((w - 2) * pct);
    if (fillW > 0) {
        _display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
    }
}

void AnimationManager::drawCheckmark(int cx, int cy) {
    // Draw a simple checkmark using lines
    // ✓ shape: from (cx-10, cy) → (cx-4, cy+8) → (cx+12, cy-10)
    _display.drawLine(cx - 10, cy,      cx - 4, cy + 8,  SSD1306_WHITE);
    _display.drawLine(cx - 10, cy + 1,  cx - 4, cy + 9,  SSD1306_WHITE);
    _display.drawLine(cx - 4,  cy + 8,  cx + 12, cy - 10, SSD1306_WHITE);
    _display.drawLine(cx - 4,  cy + 9,  cx + 12, cy - 9,  SSD1306_WHITE);

    // Circle around checkmark
    _display.drawCircle(cx, cy, 16, SSD1306_WHITE);
}

void AnimationManager::drawDots(int x, int y, int count) {
    // Draw 0..3 dots at position
    for (int i = 0; i < count; i++) {
        _display.setCursor(x + i * 6, y);
        _display.print(".");
    }
}

float AnimationManager::easeInOut(float t) {
    // Smooth step: 3t^2 - 2t^3
    return t * t * (3.0f - 2.0f * t);
}

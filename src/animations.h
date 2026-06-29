#pragma once

// ============================================================
// animations.h — Boot animation header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

class AnimationManager {
public:
    explicit AnimationManager(Adafruit_SSD1306& display);

    // Run both boot animations sequentially (blocks ~30 seconds)
    // wifiConnected: if WiFi connects during animation 2, show checkmark
    void runBootAnimations(bool& wifiConnected);

private:
    Adafruit_SSD1306& _display;

    // Animation 1: "CLAUDY" typing effect + breathing (15 seconds)
    void runTypingAnimation(uint32_t durationMs);

    // Animation 2: WiFi connecting bars + progress (15 seconds)
    void runConnectingAnimation(uint32_t durationMs, bool& wifiConnected);

    // Drawing helpers
    void drawBreathingText(const char* text, uint8_t brightness);
    void drawWifiBars(int centerX, int centerY, int level, int maxLevel);
    void drawProgressBarSimple(int x, int y, int w, int h, float pct);
    void drawCheckmark(int cx, int cy);
    void drawDots(int x, int y, int count);

    // Easing
    float easeInOut(float t); // t in [0,1] → smooth [0,1]
};

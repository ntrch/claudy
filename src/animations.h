#pragma once

// ============================================================
// animations.h — Bitmap face animation header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

class AnimationManager {
public:
    explicit AnimationManager(Adafruit_SSD1306& display);

    // Play one full face cycle: idle frames then blink frames
    // Blocking — takes approximately FACE_ANIM_DURATION_MS (~4 seconds)
    void playFaceCycle();

private:
    Adafruit_SSD1306& _display;

    // Draw a single 128x64 PROGMEM bitmap frame to the display
    void drawFrame(const unsigned char* frame);
};

#pragma once

// ============================================================
// animations.h — Bitmap face animation header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <Adafruit_SH110X.h>
#include "config.h"

class AnimationManager {
public:
    explicit AnimationManager(Adafruit_SH1106G& display);

    // Play one full face cycle: idle frames then blink frames
    // Blocking — takes approximately FACE_ANIM_DURATION_MS (~4 seconds)
    void playFaceCycle();

private:
    Adafruit_SH1106G& _display;

    // Draw a single 128x64 PROGMEM bitmap frame to the display
    void drawFrame(const unsigned char* frame);
};

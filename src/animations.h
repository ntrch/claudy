#pragma once

// ============================================================
// animations.h — Bitmap face animation header
// ESP32 Claude Code Companion
// ============================================================

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

class DisplayManager;  // forward declare

class AnimationManager {
public:
    explicit AnimationManager(DisplayManager& display);

    // Play one full face cycle: idle frames then blink frames
    // Blocking — takes approximately FACE_ANIM_DURATION_MS (~4 seconds)
    void playFaceCycle();

private:
    DisplayManager& _display;
};

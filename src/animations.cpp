// ============================================================
// animations.cpp — Bitmap face animation implementation
// ESP32 Claude Code Companion
// ============================================================

#include "animations.h"
#include "anim_idle.h"
#include "anim_blink.h"

// --------------- Constructor ---------------
AnimationManager::AnimationManager(Adafruit_SSD1306& display)
    : _display(display)
{}

// --------------- Public: play one face cycle ---------------
// Plays all idle frames then all blink frames at 10 FPS (100ms/frame).
// Total: (IDLE_FRAME_COUNT + BLINK_FRAME_COUNT) * ANIM_FRAME_DELAY_MS
void AnimationManager::playFaceCycle() {
    // Idle animation
    for (int i = 0; i < IDLE_FRAME_COUNT; i++) {
        const unsigned char* frame = (const unsigned char*)pgm_read_ptr(&idle_frames[i]);
        drawFrame(frame);
        delay(ANIM_FRAME_DELAY_MS);
    }

    // Blink animation
    for (int i = 0; i < BLINK_FRAME_COUNT; i++) {
        const unsigned char* frame = (const unsigned char*)pgm_read_ptr(&blink_frames[i]);
        drawFrame(frame);
        delay(ANIM_FRAME_DELAY_MS);
    }
}

// --------------- Private: draw a single PROGMEM bitmap frame ---------------
void AnimationManager::drawFrame(const unsigned char* frame) {
    _display.clearDisplay();
    _display.drawBitmap(0, 0, frame, OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, SSD1306_WHITE);
    _display.display();
}

// ============================================================
// animations.cpp — Bitmap face animation implementation
// ESP32 Claude Code Companion
// ============================================================

#include "animations.h"
#include "display.h"
#include "anim_idle.h"
#include "anim_blink.h"

// --------------- Constructor ---------------
AnimationManager::AnimationManager(DisplayManager& display)
    : _display(display)
{}

// --------------- Public: play one face cycle ---------------
// Plays all idle frames then all blink frames at 10 FPS (100ms/frame).
// Total: (IDLE_FRAME_COUNT + BLINK_FRAME_COUNT) * ANIM_FRAME_DELAY_MS
void AnimationManager::playFaceCycle() {
    // Idle animation
    for (int i = 0; i < IDLE_FRAME_COUNT; i++) {
        const uint8_t* frame = reinterpret_cast<const uint8_t*>(
            pgm_read_ptr(&idle_frames[i]));
        _display.drawFrame(frame);
        _display.sendBuffer();
        delay(ANIM_FRAME_DELAY_MS);
    }

    // Blink animation
    for (int i = 0; i < BLINK_FRAME_COUNT; i++) {
        const uint8_t* frame = reinterpret_cast<const uint8_t*>(
            pgm_read_ptr(&blink_frames[i]));
        _display.drawFrame(frame);
        _display.sendBuffer();
        delay(ANIM_FRAME_DELAY_MS);
    }
}

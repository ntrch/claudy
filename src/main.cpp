// ============================================================
// main.cpp — ESP32 Claude Code Companion
// OLED Usage Monitor — Main entry point
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "animations.h"
#include "wifi_manager.h"
#include "api_client.h"

// --------------- Global Objects ---------------
DisplayManager   display;
WifiSetupManager wifiMgr;
ApiClient        apiClient;
AnimationManager* anim = nullptr;  // Allocated after display.begin()

// --------------- State Machine ---------------
enum CycleState {
    FACE_ANIM_1,    //  0-5s:  face animation (blocking)
    STATUS_SCREEN,  //  5-20s: CLI status screen
    FACE_ANIM_2,    // 20-25s: face animation
    SESSION_SCREEN, // 25-40s: session usage
    FACE_ANIM_3,    // 40-45s: face animation
    WEEKLY_SCREEN,  // 45-60s: weekly usage
};

static CycleState cycleState    = FACE_ANIM_1;
static uint32_t   stateEnteredAt = 0;

// --------------- Other state ---------------
static uint32_t lastApiPoll      = 0;
static uint32_t lastDataChange   = 0;
static uint32_t noWifiSince      = 0;
static bool     wifiWasConnected = false;
static bool     dimmed           = false;

// --------------- Forward declarations ---------------
void pollApi();
void checkDimming();
void checkDeepSleep();
void enterDeepSleep();
void enterState(CycleState s);
CycleState nextCycleState(CycleState s);

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[Main] ESP32 Claude Companion starting...");

    // Wake button (GPIO0) as input
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);

    // Init display
    if (!display.begin()) {
        Serial.println("[Main] Display init FAILED — halting");
        while (true) { delay(1000); }
    }

    // Allocate animation manager
    anim = new AnimationManager(display);

    // Attempt WiFi with saved credentials (non-blocking start)
    Serial.println("[Main] Attempting WiFi with saved credentials...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(); // use saved credentials from NVS

    // Show connecting message while WiFi tries
    display.getU8g2().clearBuffer();
    display.getU8g2().setFont(u8g2_font_6x10_tr);
    display.getU8g2().drawStr(0, 18, "Connecting to WiFi...");
    display.getU8g2().sendBuffer();

    // Wait up to 10 seconds for quick connect
    uint32_t wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000UL) {
        delay(200);
    }

    bool quickConnect = (WiFi.status() == WL_CONNECTED);

    // If quick connect failed, use WiFiManager portal
    if (!quickConnect) {
        Serial.println("[Main] No saved WiFi — starting WiFiManager portal...");

        display.getU8g2().clearBuffer();
        display.getU8g2().setFont(u8g2_font_6x10_tr);
        display.getU8g2().drawStr(0, 18, "Connect to WiFi AP:");
        display.getU8g2().drawStr(0, 30, "  Claudy-Setup");
        display.getU8g2().drawStr(0, 46, "Then visit:");
        display.getU8g2().drawStr(0, 58, "  192.168.4.1");
        display.getU8g2().sendBuffer();

        bool ok = wifiMgr.begin([](const String& msg) {
            Serial.print("[WiFi] Status: ");
            Serial.println(msg);
        });

        if (ok) {
            Serial.println("[Main] WiFi connected via portal");
        } else {
            Serial.println("[Main] WiFi portal timed out");
        }
    } else {
        // Load config from NVS
        wifiMgr.begin(nullptr);
    }

    // Configure API client
    apiClient.setAuth(wifiMgr.getAuthType(), wifiMgr.getToken());

    // Update WiFi status in display
    bool connected = wifiMgr.isConnected();
    display.setWifiStatus(connected, connected ? wifiMgr.getRssi() : 0);
    if (connected) {
        wifiWasConnected = true;
        noWifiSince      = 0;
    } else {
        noWifiSince = millis();
    }

    // Initial API poll
    if (connected) {
        Serial.println("[Main] Initial API fetch...");
        pollApi();
    } else {
        display.setError("No WiFi");
    }

    lastApiPoll    = millis();
    lastDataChange = millis();

    // Enter first state
    enterState(FACE_ANIM_1);

    Serial.println("[Main] Setup complete. Entering main loop.");
}

// ============================================================
// loop() — 60-second cycle state machine
// ============================================================
void loop() {
    uint32_t now = millis();

    // --- Maintain WiFi ---
    wifiMgr.maintain();
    bool connected = wifiMgr.isConnected();

    if (connected != wifiWasConnected) {
        wifiWasConnected = connected;
        display.setWifiStatus(connected, connected ? wifiMgr.getRssi() : 0);
        if (!connected) {
            noWifiSince = now;
            display.setError("WiFi disconnected");
        } else {
            noWifiSince = 0;
            display.clearError();
        }
    }

    // Update RSSI every 5 seconds when connected
    static uint32_t lastRssiUpdate = 0;
    if (connected && now - lastRssiUpdate > 5000UL) {
        display.setWifiStatus(true, wifiMgr.getRssi());
        lastRssiUpdate = now;
    }

    // --- State machine ---
    switch (cycleState) {

        case FACE_ANIM_1:
        case FACE_ANIM_2:
        case FACE_ANIM_3: {
            // Face animation is blocking (~5s). Run it and immediately advance.
            anim->playFaceCycle();
            enterState(nextCycleState(cycleState));
            break;
        }

        case STATUS_SCREEN: {
            display.setScreen(SCREEN_STATUS);
            display.render();
            delay(50);

            if (now - stateEnteredAt >= STATUS_DURATION_MS) {
                // API poll once per 60-second cycle (at end of status screen)
                if (connected) {
                    if (now - lastApiPoll >= API_POLL_INTERVAL_MS) {
                        lastApiPoll = now;
                        pollApi();
                    }
                }
                enterState(nextCycleState(cycleState));
            }
            break;
        }

        case SESSION_SCREEN: {
            display.setScreen(SCREEN_SESSION);
            display.render();
            delay(50);

            if (now - stateEnteredAt >= SESSION_DURATION_MS) {
                enterState(nextCycleState(cycleState));
            }
            break;
        }

        case WEEKLY_SCREEN: {
            display.setScreen(SCREEN_WEEKLY);
            display.render();
            delay(50);

            if (now - stateEnteredAt >= WEEKLY_DURATION_MS) {
                enterState(nextCycleState(cycleState));
            }
            break;
        }
    }

    // --- Auto-dim check ---
    checkDimming();

    // --- Deep sleep check (no WiFi for 5 minutes) ---
    checkDeepSleep();
}

// ============================================================
// State helpers
// ============================================================
void enterState(CycleState s) {
    cycleState     = s;
    stateEnteredAt = millis();
    Serial.print("[Main] -> State: ");
    Serial.println((int)s);
}

CycleState nextCycleState(CycleState s) {
    switch (s) {
        case FACE_ANIM_1:    return STATUS_SCREEN;
        case STATUS_SCREEN:  return FACE_ANIM_2;
        case FACE_ANIM_2:    return SESSION_SCREEN;
        case SESSION_SCREEN: return FACE_ANIM_3;
        case FACE_ANIM_3:    return WEEKLY_SCREEN;
        case WEEKLY_SCREEN:  return FACE_ANIM_1;
        default:             return FACE_ANIM_1;
    }
}

// ============================================================
// pollApi() — fetch usage data and update display
// ============================================================
void pollApi() {
    Serial.println("[Main] Polling API...");
    UsageData data;
    bool ok = apiClient.fetchWithRetry(data, API_RETRY_MAX);

    if (ok) {
        display.setUsageData(data);
        display.clearError();
        lastDataChange = millis();
        Serial.println("[Main] API data updated");
    } else {
        String err = apiClient.lastErrorString();
        display.setError(err);
        Serial.print("[Main] API error: ");
        Serial.println(err);
    }
}

// ============================================================
// checkDimming() — auto-dim after 5 minutes of no data change
// ============================================================
void checkDimming() {
    uint32_t now      = millis();
    bool     shouldDim = (now - lastDataChange >= DISPLAY_DIM_TIMEOUT_MS);

    if (shouldDim && !dimmed) {
        dimmed = true;
        display.setDimmed(true);
        Serial.println("[Main] Display dimmed");
    } else if (!shouldDim && dimmed) {
        dimmed = false;
        display.setDimmed(false);
        Serial.println("[Main] Display un-dimmed");
    }
}

// ============================================================
// checkDeepSleep() — enter deep sleep if no WiFi for 5 minutes
// ============================================================
void checkDeepSleep() {
    if (wifiMgr.isConnected()) {
        noWifiSince = 0;
        return;
    }

    if (noWifiSince == 0) {
        noWifiSince = millis();
        return;
    }

    uint32_t noWifiMs = millis() - noWifiSince;
    if (noWifiMs >= DEEP_SLEEP_NO_WIFI_MS) {
        Serial.println("[Main] No WiFi for 5 minutes — entering deep sleep");
        enterDeepSleep();
    }
}

// ============================================================
// enterDeepSleep() — deep sleep, wake on GPIO0 (BOOT button)
// ============================================================
void enterDeepSleep() {
    display.getU8g2().clearBuffer();
    display.getU8g2().setFont(u8g2_font_6x10_tr);
    display.getU8g2().drawStr(10, 28, "Sleeping...");
    display.getU8g2().drawStr(0, 42, "Press BOOT to wake");
    display.getU8g2().sendBuffer();
    delay(1500);

    display.sleepDisplay();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BUTTON_PIN, 0);

    Serial.println("[Main] Entering deep sleep. Wake on GPIO0.");
    Serial.flush();

    esp_deep_sleep_start();
}

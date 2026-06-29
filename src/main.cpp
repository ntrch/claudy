// ============================================================
// main.cpp — ESP32 Claude Code Companion
// OLED Usage Monitor — Main entry point
// ============================================================

#include <Arduino.h>
#include <esp_task_wdt.h>
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

// --------------- State ---------------
static uint32_t lastApiPoll       = 0;
static uint32_t lastScreenRotate  = 0;
static uint32_t lastDataChange    = 0;   // for auto-dim
static uint32_t noWifiSince       = 0;   // for deep-sleep timer
static bool     wifiWasConnected  = false;
static bool     dimmed            = false;

// --------------- Forward declarations ---------------
void pollApi();
void checkDimming();
void checkDeepSleep();
void enterDeepSleep();

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[Main] ESP32 Claude Companion starting...");

    // --- Watchdog: 60 seconds (covers 30s boot animations + portal margin) ---
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = 60000,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&wdtCfg);
    esp_task_wdt_add(NULL);

    // --- Wake button (GPIO0) as input ---
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);

    // --- Init display ---
    if (!display.begin()) {
        Serial.println("[Main] Display init FAILED — halting");
        while (true) { delay(1000); }
    }

    // --- Boot animations ---
    // We run them before WiFi so the typing animation plays immediately.
    // The connecting animation will poll WiFi.status() internally.
    anim = new AnimationManager(display.getDisplay());

    bool wifiConnectedDuringAnim = false;

    // Kick off WiFi connection attempt in background before animations start.
    // WiFiManager will autoConnect; we run animations while it tries.
    // However, WiFiManager is blocking, so we start it after the first animation.

    // Start WiFi non-blocking using saved credentials from NVS.
    // The connecting animation (animation 2) will poll WiFi.status()
    // internally and show a checkmark when the connection succeeds.
    // If credentials are not saved, WiFiManager portal runs after animations.
    Serial.println("[Main] Attempting WiFi with saved credentials...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(); // Use saved credentials from NVS

    // Run both animations while WiFi tries to connect in background
    Serial.println("[Main] Running boot animations...");
    esp_task_wdt_reset();
    anim->runBootAnimations(wifiConnectedDuringAnim);
    esp_task_wdt_reset();

    // If WiFi didn't connect during animations, use WiFiManager portal
    if (!wifiConnectedDuringAnim && WiFi.status() != WL_CONNECTED) {
        Serial.println("[Main] No saved WiFi — starting WiFiManager portal...");

        display.getDisplay().clearDisplay();
        display.getDisplay().setTextSize(1);
        display.getDisplay().setCursor(0, 10);
        display.getDisplay().print("Connect to WiFi AP:");
        display.getDisplay().setCursor(0, 24);
        display.getDisplay().print("  Claudy-Setup");
        display.getDisplay().setCursor(0, 38);
        display.getDisplay().print("Then visit:");
        display.getDisplay().setCursor(0, 50);
        display.getDisplay().print("  192.168.4.1");
        display.getDisplay().display();

        esp_task_wdt_reset();
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
        // WiFi connected during animation — still load config from NVS
        wifiMgr.begin(nullptr);
    }

    // Configure API client with stored endpoint + key
    apiClient.setEndpoint(wifiMgr.getApiUrl(), wifiMgr.getApiKey());

    // Update display WiFi status
    bool connected = wifiMgr.isConnected();
    display.setWifiStatus(connected, connected ? wifiMgr.getRssi() : 0);
    if (connected) {
        wifiWasConnected = true;
        noWifiSince      = 0;
    } else {
        noWifiSince = millis();
    }

    // Initial API poll
    esp_task_wdt_reset();
    if (connected) {
        Serial.println("[Main] Initial API fetch...");
        pollApi();
    } else {
        display.setError("No WiFi");
    }

    display.render();

    lastApiPoll      = millis();
    lastScreenRotate = millis();
    lastDataChange   = millis();

    Serial.println("[Main] Setup complete. Entering main loop.");
    esp_task_wdt_reset();
}

// ============================================================
// loop()
// ============================================================
void loop() {
    esp_task_wdt_reset();
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
        display.render();
    }

    // Update RSSI icon periodically (every 5 seconds when connected)
    static uint32_t lastRssiUpdate = 0;
    if (connected && now - lastRssiUpdate > 5000UL) {
        display.setWifiStatus(true, wifiMgr.getRssi());
        lastRssiUpdate = now;
    }

    // --- API poll on interval ---
    if (now - lastApiPoll >= API_POLL_INTERVAL_MS) {
        lastApiPoll = now;
        if (connected) {
            pollApi();
        }
    }

    // --- Auto-rotate screens ---
    if (now - lastScreenRotate >= SCREEN_ROTATE_INTERVAL_MS) {
        lastScreenRotate = now;
        display.nextScreen();
        display.render();
    }

    // --- Auto-dim check ---
    checkDimming();

    // --- Deep sleep check (no WiFi for 5 minutes) ---
    checkDeepSleep();

    // Small yield to prevent watchdog starvation
    delay(50);
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

    display.render();
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
        noWifiSince = 0; // reset timer
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
    // Show sleep message
    display.getDisplay().clearDisplay();
    display.getDisplay().setTextSize(1);
    display.getDisplay().setCursor(10, 20);
    display.getDisplay().print("Sleeping...");
    display.getDisplay().setCursor(0, 34);
    display.getDisplay().print("Press BOOT to wake");
    display.getDisplay().display();
    delay(1500);

    // Turn off display
    display.getDisplay().ssd1306_command(SSD1306_DISPLAYOFF);

    // Configure GPIO0 as external wake source (LOW = button pressed)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BUTTON_PIN, 0);

    Serial.println("[Main] Entering deep sleep. Wake on GPIO0.");
    Serial.flush();

    esp_deep_sleep_start();
    // Does not return
}

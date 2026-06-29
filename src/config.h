#pragma once

// ============================================================
// config.h — Pin definitions, constants, and global settings
// ESP32 Claude Code Companion
// ============================================================

// --------------- OLED Display ---------------
#define OLED_SDA_PIN        21
#define OLED_SCL_PIN        22
#define OLED_I2C_ADDRESS    0x3C
#define OLED_SCREEN_WIDTH   128
#define OLED_SCREEN_HEIGHT  64
#define OLED_RESET_PIN      -1   // Share Arduino reset pin

// --------------- 60-Second Display Cycle Timing ---------------
#define CYCLE_TOTAL_MS          60000UL   // Full cycle length
#define FACE_ANIM_DURATION_MS   5000UL    // ~5s blocking face animation (50 frames @ 100ms)
#define STATUS_DURATION_MS      15000UL   // CLI status screen
#define SESSION_DURATION_MS     15000UL   // Session usage screen
#define WEEKLY_DURATION_MS      15000UL   // Weekly usage screen

// --------------- Animation Frame Timing ---------------
#define ANIM_FRAME_DELAY_MS     100       // 10 FPS frame rate
#define TYPING_CHAR_DELAY_MS    80        // ms per character typed
#define TYPING_ERASE_DELAY_MS   50        // ms per character erased
#define TYPING_PAUSE_MS         2000      // pause after fully typed

// --------------- API / Network Timing ---------------
#define API_POLL_INTERVAL_MS        60000UL   // API poll every 60 seconds
#define DISPLAY_DIM_TIMEOUT_MS      300000UL  // Auto-dim after 5 minutes
#define WIFI_CONNECT_TIMEOUT_S      180       // WiFi connection timeout: 3 minutes
#define DEEP_SLEEP_NO_WIFI_MS       300000UL  // Deep sleep if no WiFi for 5 minutes

// --------------- Retry / Backoff ---------------
#define API_RETRY_MAX               3
#define API_RETRY_BASE_DELAY_MS     2000UL    // 2s, 4s, 8s backoff

// --------------- GPIO ---------------
#define WAKE_BUTTON_PIN             0         // GPIO0 (BOOT button) — wake from deep sleep

// --------------- WiFi AP / Storage ---------------
#define WIFI_AP_NAME                "Claudy-Setup"
#define WIFI_AP_PASSWORD            ""        // Open AP (no password)
#define PREF_NAMESPACE              "claudy"
#define PREF_KEY_AUTH_TYPE          "auth_type"
#define PREF_KEY_TOKEN              "token"

// --------------- Anthropic API ---------------
#define ANTHROPIC_API_URL           "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_API_VERSION       "2023-06-01"
#define ANTHROPIC_MIN_MODEL         "claude-haiku-4-5-20251001"

// --------------- OAuth ---------------
#define OAUTH_REFRESH_URL           "https://console.anthropic.com/v1/oauth/token"
// Public Claude Code OAuth client_id — same for every install, not a secret.
// Hardcoded so the user never has to extract it from the binary.
#define OAUTH_CLIENT_ID             "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
#define PREF_KEY_OAUTH_CLIENT_ID    "client_id"
#define PREF_KEY_ACCESS_TOKEN       "access_tkn"

// --------------- Progress Bar ---------------
#define PROGRESS_BAR_WIDTH          108  // pixels
#define PROGRESS_BAR_HEIGHT         8    // pixels (taller for new screens)

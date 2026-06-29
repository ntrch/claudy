# Claudy — ESP32 Claude Code Companion

A compact OLED usage monitor for your Claude API account. Built around an ESP32-WROOM DevKit 1 and a 1.3-inch SSD1306 OLED display, Claudy fetches your daily/weekly request counts and cost from a configurable API endpoint and shows them on a clean pixel display.

---

## Features

- Boot animations: "CLAUDY" typing effect + WiFi connecting animation
- Live usage overview: daily and weekly request counts with progress bars
- Cost tracking screen: current spend vs. limit
- Auto-rotating screens every 10 seconds
- WiFi setup via captive portal (no hardcoded credentials)
- Configurable API endpoint and API key, saved to flash
- Auto-dim after 5 minutes of no data change
- Deep sleep after 5 minutes without WiFi (wake with BOOT button)
- WiFi signal strength icon with 4 levels

---

## Hardware

| Component | Part |
|---|---|
| Microcontroller | ESP32-WROOM DevKit 1 (38-pin) |
| Display | 1.3" SSD1306 OLED, 128x64, I2C |

---

## Wiring

```
OLED Pin   ->  ESP32 Pin
---------      ----------
SDA        ->  GPIO 21
SCL        ->  GPIO 22
VCC        ->  3.3V
GND        ->  GND
```

The BOOT button (GPIO0, already on the DevKit board) doubles as the wake-from-sleep button — no extra wiring needed.

---

## Building and Flashing

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- USB cable and ESP32-WROOM DevKit 1

### Flash

```bash
# Clone the repo
git clone <repo-url>
cd claudy

# Build and upload
pio run --target upload

# Open serial monitor
pio device monitor
```

---

## First-Time WiFi Configuration

On first boot (or after a reset), Claudy starts a WiFi access point:

```
SSID: Claudy-Setup
Password: (none — open network)
```

1. Connect your phone or laptop to **Claudy-Setup**.
2. A captive portal opens automatically (or navigate to `192.168.4.1`).
3. Select your home WiFi network and enter the password.
4. Fill in the **API Endpoint URL** and **API Key** fields.
5. Save — the ESP32 reboots and connects.

Credentials are stored in flash (ESP32 NVS) and survive power cycles.

---

## API Endpoint Format

Claudy sends an HTTP(S) GET request to your configured endpoint with an `Authorization: Bearer <api_key>` header.

### Expected JSON Response

```json
{
  "daily_usage": {
    "used": 150,
    "limit": 500,
    "unit": "requests"
  },
  "weekly_usage": {
    "used": 2100,
    "limit": 3500,
    "unit": "requests"
  },
  "cost": {
    "current": 12.50,
    "limit": 50.00,
    "currency": "USD"
  },
  "reset": {
    "daily":  "2024-01-15T00:00:00Z",
    "weekly": "2024-01-21T00:00:00Z"
  },
  "plan": "Pro"
}
```

All fields are optional — missing fields default to zero/empty. The display shows "---" for unknown plan names.

---

## Screen Layout

### Screen 1 — Usage Overview

```
┌──────────────────────┐
│ CLAUDY    [WiFi] Pro │  <- Header with WiFi icon and plan
│──────────────────────│
│ Daily:  150/500      │
│ ████████░░░░ 30%     │  <- Progress bar
│ Weekly:2100/3500     │
│ ████████████░ 60%    │
│ Reset: 01-15 00:00Z  │
└──────────────────────┘
```

### Screen 2 — Cost & Details

```
┌──────────────────────┐
│ CLAUDY    [WiFi] Pro │
│──────────────────────│
│ Cost: $12.50/$50.00  │
│ ████████░░░░ 25%     │
│                      │
│ Weekly Reset:        │
│ 01-21 00:00 UTC      │
└──────────────────────┘
```

---

## Project Structure

```
claudy/
├── platformio.ini            # PlatformIO build config
├── src/
│   ├── main.cpp              # Setup, loop, deep sleep
│   ├── config.h              # Pin definitions and constants
│   ├── display.h / .cpp      # OLED rendering and screen management
│   ├── animations.h / .cpp   # Boot animations (typing + WiFi)
│   ├── wifi_manager.h / .cpp # WiFiManager captive portal + NVS storage
│   └── api_client.h / .cpp   # HTTPS fetch + JSON parsing
└── data/                     # Reserved for SPIFFS assets
```

---

## Resetting WiFi / Config

To reset all saved WiFi credentials and API settings, hold the BOOT button (GPIO0) for 5 seconds while the device is running, or call `wifiMgr.resetConfig()` programmatically and reflash. This clears the NVS partition and restarts the captive portal on next boot.

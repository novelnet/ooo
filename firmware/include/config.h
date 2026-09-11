#pragma once

// --- Bekannte WLANs --------------------------------------------------------------
#define MAX_NETS           8     // so viele Netze merkt sich der ESP32 (aeltestes faellt raus)
#define MAX_MACS           3     // so viele MAC-Adressen je Netz (macOS rotiert sie)
#define CONNECT_TRY_MS     15000 // Zeitfenster je Netz und Versuch

// --- Wecken ----------------------------------------------------------------------
#define WOL_BURST          5     // Magic Packets pro Wake
#define WAKE_VERIFY_SEC    45    // so lange nach dem Wake auf Ping-Antwort warten

// --- Optional: Relais im Netzteilstrang (docs/hardware.md). Standard: aus. --------
#define RELAY_ENABLED      false
#define RELAY_PIN          26
#define RELAY_ACTIVE_LOW   true
#define POWER_PULSE_MS     8000

// --- Sonstiges -------------------------------------------------------------------
#define LED_PIN            2     // Onboard-LED des ESP32-DevKit (blau)
#define POLL_WAIT_SEC      20
#define POLL_MIN_GAP_MS    500
#define BACKOFF_MIN_MS     5000
#define BACKOFF_MAX_MS     60000
#define REBOOT_AFTER_OFFLINE_MS (10UL * 60UL * 1000UL)
#define FW_VERSION         "0.6.0"

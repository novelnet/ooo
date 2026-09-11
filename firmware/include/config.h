#pragma once

// --- Mac -------------------------------------------------------------------------
#define MAC_HOSTNAME_DEFAULT "MacBookPro"   // `hostname -s` auf dem Mac; im Setup-Portal änderbar
#define WOL_BURST          5                // Magic Packets pro Wake
#define WAKE_VERIFY_SEC    45               // so lange nach dem Wake auf Ping-Antwort warten

// --- WLAN-Setup ------------------------------------------------------------------
#define SETUP_AP_NAME      "ooo-setup"      // Setup-WLAN, wenn keine/falsche Zugangsdaten
#define SETUP_TIMEOUT_SEC  300              // danach Neustart und neuer Versuch
#define RESET_BUTTON_PIN   0                // BOOT-Taste beim Einschalten 3 s halten → WLAN vergessen

// --- Optional: Relais im Netzteilstrang (docs/hardware.md). Standard: aus. -------
#define RELAY_ENABLED      false
#define RELAY_PIN          26
#define RELAY_ACTIVE_LOW   true
#define POWER_PULSE_MS     8000

// --- Sonstiges -------------------------------------------------------------------
#define LED_PIN            2
#define POLL_WAIT_SEC      20
#define POLL_MIN_GAP_MS    500
#define BACKOFF_MIN_MS     5000
#define BACKOFF_MAX_MS     60000
#define REBOOT_AFTER_OFFLINE_MS (10UL * 60UL * 1000UL)
#define FW_VERSION         "0.3.0"

// Vorlage – mit 1Password rendern (scripts/secrets.sh render). secrets.h wird nie committet.
// WLAN und Mac-Adresse stehen hier absichtlich NICHT: das WLAN richtest du einmal per Handy
// über das Setup-WLAN "ooo-setup" ein, die MAC-Adresse des Macs lernt der ESP32 selbst.
#pragma once
#define OOO_URL          "https://{{ op://Dev/ooo/supabase/project-ref }}.supabase.co/functions/v1/ooo"
#define OOO_DEVICE_TOKEN "{{ op://Dev/ooo/tokens/device }}"

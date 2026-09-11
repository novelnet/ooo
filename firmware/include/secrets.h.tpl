// Vorlage – mit 1Password rendern (scripts/secrets.sh render). secrets.h wird nie committet.
// WLAN und Mac-Adresse stehen hier absichtlich NICHT: die kommen beim Einrichten über
// das USB-Kabel vom Mac (mac/setup.sh).
#pragma once
#define OOO_URL          "{{ op://Personal/ooo/deno/url }}"
#define OOO_DEVICE_TOKEN "{{ op://Personal/ooo/tokens/device }}"

# Hardware

## Standard: nur der ESP32

| Teil | Zweck |
|---|---|
| ESP32 DevKit (vorhanden) | WLAN, Long-Poll, sendet Magic Packets, pingt den Mac |
| USB-Netzteil 5 V | Versorgung |
| optional: USB-C-Ethernet-Adapter am Mac | macht WoL bei Apple zuverlässig (WLAN nur mit Apple TV/HomePod als Sleep Proxy) |
| optional: HDMI-Dummy | Mac bleibt zugeklappt wach und hat für VNC eine echte Auflösung |

Keine Verkabelung nötig. Onboard-LED: 2× blinken = bereit, 3× = HTTP-Fehler, an = Wake läuft.

## Option: Relais am Netzteil (`RELAY_ENABLED true`)

Nur falls WoL im Alltag nicht reicht. Strom anschließen weckt und bootet Apple-Silicon-Macs
immer, ein 8-s-Puls „aus → an“ genügt.

```
ESP32 GPIO26 ──► IN    Relais-Modul 5 V (Optokoppler, 3,3-V-tauglich, Kontakte 250 V / 10 A)
ESP32 5V     ──► VCC
ESP32 GND    ──► GND
COM / NO     ──► unterbricht nur L (Phase) zum MacBook-Netzteil; N und PE bleiben durchgehend
```

**230 V sind lebensgefährlich.** Isoliertes Gehäuse, Zugentlastung, nur mit Elektro-Erfahrung.
Sicherere Variante: fertiger Schaltstecker mit ESP (Athom ESP32-Plug, Shelly Plug S) und die
Firmware darauf portieren (`RELAY_PIN`/`LED_PIN` anpassen).
Schaltet das Relais beim Start den Mac stromlos, `RELAY_ACTIVE_LOW` umdrehen.

# firmware – ESP32 Wake-Trigger

Der ESP32 hängt im WLAN, wartet auf Kommandos vom Relay und weckt den Mac per Wake-on-LAN.
Bei jedem Poll pingt er den Mac und meldet, ob dieser wach ist.

## Einrichten

Der ESP32 wird nicht von Hand konfiguriert. Er bekommt alles über das USB-Kabel vom Mac:

```bash
cd firmware && pio run -t upload     # einmal flashen
cd .. && bash mac/setup.sh           # WLAN, Mac-Name und MAC-Adresse übertragen
```

Danach braucht er nur noch Strom, egal von welchem Netzteil. Ändert sich das WLAN,
einfach wieder anstecken und `bash mac/setup.sh` laufen lassen.

## LED

| LED | Bedeutung |
|---|---|
| dauerhaft an | WLAN verbunden, alles gut |
| schnelles Blinken | verbindet gerade |
| langsames Blinken | wartet auf Einrichtung per USB (oder Wake läuft) |
| 3× kurz | Relay nicht erreichbar, versucht es gleich wieder |

Auf dem klassischen ESP32-DevKit ist das die blaue LED an GPIO 2.

## Befehle über USB (115200 Baud)

| Befehl | Wirkung |
|---|---|
| `STATUS` | Zustand ausgeben |
| `RESET` | alles vergessen und neu starten |
| `PROV <b64 ssid> <b64 pass> <b64 host> <mac>` | einrichten (macht `mac/setup.sh`) |

## Optionen (`include/config.h`)

- `RELAY_ENABLED` (Standard `false`): zusätzlich Relais am Netzteil pulsen, siehe `docs/hardware.md`.
- `WOL_BURST`, `WAKE_VERIFY_SEC`, Poll-Intervalle, Watchdog.
- Getestet auf ESP32-D0WD-V3 (4 MB Flash, CH340). Anderes Board: `board =` in `platformio.ini`.

## Sicherheit

- Root-CA gepinnt (ISRG Root X1, bis 2035), kein `setInsecure()`.
- Device-Token kann nur Kommandos abholen und bestätigen.
- Das WLAN-Passwort liegt im Flash des ESP32 und kommt nur über das USB-Kabel dorthin.

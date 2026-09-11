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

Die rote LED ist mit **PWR** beschriftet und hängt fest am Strom – sie leuchtet immer, sobald
der ESP32 versorgt ist, und lässt sich per Software nicht schalten. Steuerbar ist nur die blaue
LED an GPIO 2. Der Zustand wird deshalb über Blau angezeigt, "blau aus" heißt: nur Rot sichtbar.

| Anzeige | Bedeutung |
|---|---|
| nur rot (blau aus) | noch nicht bereit, wartet auf Einrichtung per USB |
| blau blinkt gleichmäßig | arbeitet gerade: verbindet sich oder weckt den Mac |
| blau leuchtet dauerhaft | bereit, im WLAN, alles gut |
| blau blinkt doppelt | Fehler: WLAN-Zugangsdaten falsch oder Relay nicht erreichbar |

## Befehle über USB (115200 Baud)

| Befehl | Wirkung |
|---|---|
| `STATUS` | Zustand ausgeben |
| `SCAN` | sichtbare WLANs auflisten (`SCANNET <rssi> <kanal> <verschl.> <ssid>`) |
| `RESET` | alles vergessen und neu starten |
| `PROV <b64 ssid> <b64 pass> <b64 host> <mac>` | einrichten (macht `mac/setup.sh`) |

## Optionen (`include/config.h`)

- `RELAY_ENABLED` (Standard `false`): zusätzlich Relais am Netzteil pulsen, siehe `docs/hardware.md`.
- `WOL_BURST`, `WAKE_VERIFY_SEC`, Poll-Intervalle, Watchdog.
- Getestet auf ESP32-D0WD-V3 (4 MB Flash, CH340). Anderes Board: `board =` in `platformio.ini`.
- **Nur 2,4 GHz.** Der ESP32 kann kein 5-GHz-WLAN. Der Netz-Dialog zeigt deshalb nur, was er wirklich erreicht.

## MAC-Adresse

Zum Wecken zählt nicht die Hardware-Adresse des Macs, sondern die **private WLAN-Adresse**, die
macOS pro Netz vergibt. Der ESP32 liest sie nach jedem erfolgreichen Ping aus der ARP-Tabelle und
merkt sie sich. Die beim Einrichten übertragene Adresse ist nur ein Startwert.

Damit sie stabil bleibt: am Mac unter WLAN → Details → "Private WLAN-Adresse" auf **Fest** stellen.

## Sicherheit

- Root-CA gepinnt (ISRG Root X1, bis 2035), kein `setInsecure()`.
- Device-Token kann nur Kommandos abholen und bestätigen.
- Das WLAN-Passwort liegt im Flash des ESP32 und kommt nur über das USB-Kabel dorthin.

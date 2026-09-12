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
| `PROV <b64 ssid> <b64 pass> <b64 host>` | ein Netz hinzufügen (macht `mac/setup.sh`) |
| `CONNECT` | jetzt verbinden |
| `MACADDR <mac>` | Adresse des Macs für das gerade verbundene Netz merken |

## Optionen (`include/config.h`)

- `RELAY_ENABLED` (Standard `false`): zusätzlich ein Relais am Netzteil pulsen, siehe unten.
- `WOL_BURST`, `WAKE_VERIFY_SEC`, Poll-Intervalle, Watchdog.
- Getestet auf ESP32-D0WD-V3 (4 MB Flash, CH340). Anderes Board: `board =` in `platformio.ini`.
- **Nur 2,4 GHz.** Der ESP32 kann kein 5-GHz-WLAN. Der Netz-Dialog zeigt deshalb nur, was er wirklich erreicht.

## Mehrere Netze

Der ESP32 merkt sich bis zu acht WLANs und nimmt beim Start das stärkste, das er kennt.
Neue Netze kommen über USB dazu, das älteste fällt raus.

## MAC-Adresse

Zum Wecken zählt nicht die Hardware-Adresse des Macs, sondern die **private WLAN-Adresse**, die
macOS pro WLAN vergibt und gelegentlich wechselt. Der ESP32 bekommt sie auf zwei Wegen:

1. Der Mac schickt sie beim Einrichten mit (`ifconfig`), direkt nach dem Verbinden – so gehört
   sie eindeutig zu dem Netz, in dem der ESP32 gelandet ist.
2. Nach jedem erfolgreichen Ping frischt der ESP32 sie aus der ARP-Tabelle auf.

Er behält je Netz die letzten drei Adressen und schickt das Weckpaket an alle. Damit wirkt ein
Wechsel der privaten Adresse nicht sofort wie ein Ausfall.

## Sparsamkeit

- **Die verschlüsselte Verbindung bleibt offen.** Sie neu auszuhandeln ist der mit Abstand
  teuerste Teil – für den ESP32 (Rechenzeit, also Strom) und für den Server. Statt rund 3.500
  Mal am Tag wird sie nur noch bei einem Abbruch neu aufgebaut. Dafür steht hier ein eigener,
  minimaler HTTP-Client statt `HTTPClient`: die Bibliothek setzte bei jedem Aufruf neu an.
  Gemessen: zwei Minuten Betrieb, eine Verbindung statt fünf.
- **Stromsparmodus des Funkmoduls ist an** (`WiFi.setSleep(true)`). Halbiert den Ruheverbrauch
  und kostet beim Empfang höchstens ein DTIM-Intervall.
- Der ESP32 hängt am USB-Anschluss des MacBooks und zieht im Akkubetrieb aus dessen Akku.
  Deshalb zählt jedes Milliampere.

Beim Lesen der Antwort wird eine eigene Zeitgrenze mitgeführt. `setTimeout()` ist auf einer
verschlüsselten Verbindung unbrauchbar – es meldet „Bad file number", und je nach Core-Version
gilt die Angabe in Sekunden oder Millisekunden.

## Sicherheit

- Root-CA gepinnt (ISRG Root X1, bis 2035), kein `setInsecure()`.
- Device-Token kann nur Kommandos abholen und bestätigen.
- Das WLAN-Passwort liegt im Flash des ESP32 und kommt nur über das USB-Kabel dorthin.

## Hardware

| Teil | Zweck |
|---|---|
| ESP32 DevKit | WLAN, wartet auf Befehle, schickt Magic Packets, pingt den Mac |
| USB-Kabel zum MacBook | Strom **und** Einrichtung; der Mac versorgt ihn auch im Schlaf |
| optional: USB-C-Ethernet-Adapter am Mac | macht Wake-on-LAN bei Apple zuverlässig |

Getestet auf einem ESP32-D0WD-V3 mit 4 MB Flash und CH340-Chip.

## Option: Relais am Netzteil

Nur nötig, falls Wake-on-LAN im Alltag nicht reicht. Strom anschließen weckt und startet
Apple-Silicon-Macs immer; ein Puls „aus → an" von acht Sekunden genügt. Aktivieren mit
`RELAY_ENABLED true` in `include/config.h`.

```
ESP32 GPIO26 ──► IN    Relais-Modul 5 V (Optokoppler, 3,3-V-tauglich, Kontakte 250 V / 10 A)
ESP32 5V     ──► VCC
ESP32 GND    ──► GND
COM / NO     ──► unterbricht nur L (Phase) zum MacBook-Netzteil; N und PE bleiben durchgehend
```

**230 V sind lebensgefährlich.** Isoliertes Gehäuse, Zugentlastung, und nur mit
Elektro-Erfahrung. Die sichere Variante ist ein fertiger Schaltstecker mit ESP32 (Athom
ESP32-Plug, Shelly Plug S); die Firmware läuft darauf, nur `RELAY_PIN` und `LED_PIN` anpassen.
Schaltet das Relais den Mac beim Start stromlos, `RELAY_ACTIVE_LOW` umdrehen.

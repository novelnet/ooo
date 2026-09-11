# mac

```bash
bash mac/setup.sh                  # ESP32 anstecken, dann dieser eine Befehl
bash mac/setup.sh --install-auto   # danach nie wieder ein Befehl (empfohlen)
```

## Empfohlener Aufbau: ESP32 bleibt am MacBook stecken

Der ESP32 hängt dauerhaft am USB-Anschluss des MacBooks. Damit

- reist er mit und ist immer im selben Netz wie der Mac,
- bekommt er weiter Strom, auch wenn der Mac schläft (USB-Anschlüsse liefern im Schlaf Strom),
- und bekommt er bei **jedem WLAN-Wechsel** automatisch die neuen Zugangsdaten.

Das erledigt der Hintergrunddienst aus `--install-auto`. Er sieht alle zehn Sekunden nach, ob
sich etwas geändert hat, und erkennt einen Netzwechsel am Standard-Gateway und dessen
MAC-Adresse – nicht am WLAN-Namen, denn den gibt macOS ohne Ortungsdienste-Berechtigung nicht
heraus. Protokoll: `/tmp/ooo-watch.log`, abschalten mit `--remove-auto`.

## Was beim Einrichten passiert

1. Der ESP32 scannt, welche WLANs in Reichweite sind (nur 2,4 GHz, mehr kann er nicht).
2. Der Mac listet die WLANs auf, die er kennt. Alles, was in beiden Listen steht, wird übertragen.
3. Die Passwörter kommen aus dem Schlüsselbund, sonst fragt ein Fenster danach.
4. Der Name des Macs geht mit, damit der ESP32 ihn im Netz finden kann.
5. Wake-on-LAN wird aktiviert (`pmset womp 1`).

Der ESP32 merkt sich bis zu acht Netze und nimmt beim Start das stärkste, das er kennt.
Die Netzwerkadresse des Macs lernt er **pro Netz**, weil macOS in jedem WLAN eine andere
private Adresse benutzt.

## Voraussetzungen für Wake-on-LAN bei Apple

- Netzteil angeschlossen.
- Ethernet (USB-C-Adapter) **oder** im WLAN ein Apple TV/HomePod (Bonjour Sleep Proxy).
  Reines WLAN ohne Proxy wacht nicht auf.
- FileVault ist an → nie ausschalten, nur schlafen lassen.

# mac

Ein Befehl richtet alles ein, der ESP32 muss dabei per USB am Mac stecken:

```bash
bash mac/setup.sh
```

Ablauf: Der ESP32 scannt selbst nach WLANs, du wählst deins in einem Fenster aus. Das Passwort
kommt aus dem Schlüsselbund, sonst fragt ein Fenster danach. Dazu gehen der Name des Macs und
seine MAC-Adresse über das Kabel. Zum Schluss aktiviert das Skript Wake-on-LAN (`pmset womp 1`).
Sonst läuft auf dem Mac nichts.

Warum der ESP32 scannt und nicht der Mac gefragt wird: macOS gibt den Namen des aktuellen WLANs
nur an Programme heraus, die die Berechtigung für Ortungsdienste haben – sonst liefert es
wörtlich `<redacted>`. Der Umweg über den ESP32 spart diese Berechtigung und zeigt nebenbei nur
Netze, die er auch wirklich erreichen kann (2,4 GHz).

Klappt es, leuchtet die blaue LED am ESP32 dauerhaft. Die rote LED daneben ist die
Betriebsanzeige (PWR) und leuchtet immer.

## Voraussetzungen für Wake-on-LAN bei Apple

- Netzteil angeschlossen.
- Ethernet (USB-C-Adapter) **oder** im WLAN ein Apple TV/HomePod (Bonjour Sleep Proxy).
  Reines WLAN ohne Proxy wacht nicht auf. Dann: Adapter, oder Relais-Option in der Firmware.
- FileVault ist an → nie ausschalten, nur schlafen lassen.

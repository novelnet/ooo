# mac

Ein Befehl richtet alles ein, der ESP32 muss dabei per USB am Mac stecken:

```bash
bash mac/setup.sh
```

Das Skript liest WLAN-Name, WLAN-Passwort (aus dem Schlüsselbund, fragt einmal nach deinem
Passwort), den Namen des Macs und dessen MAC-Adresse aus und schickt alles über das Kabel an
den ESP32. Zum Schluss aktiviert es Wake-on-LAN (`pmset womp 1`). Sonst läuft auf dem Mac nichts.

Klappt es, leuchtet die blaue LED am ESP32 dauerhaft. Die rote LED daneben ist die
Betriebsanzeige (PWR) und leuchtet immer.

## Voraussetzungen für Wake-on-LAN bei Apple

- Netzteil angeschlossen.
- Ethernet (USB-C-Adapter) **oder** im WLAN ein Apple TV/HomePod (Bonjour Sleep Proxy).
  Reines WLAN ohne Proxy wacht nicht auf. Dann: Adapter, oder Relais-Option in der Firmware.
- FileVault ist an → nie ausschalten, nur schlafen lassen.

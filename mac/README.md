# mac

```bash
bash mac/setup.sh     # setzt `pmset womp 1` (Wake for network access)
```

Mehr läuft auf dem Mac nicht. Er bleibt wach, solange gearbeitet wird (Amphetamine, Sessions);
schläft er, weckt ihn der Kurzbefehl.

Voraussetzungen für Wake-on-LAN bei Apple:
- Netzteil angeschlossen.
- Ethernet (USB-C-Adapter) **oder** im WLAN ein Apple TV/HomePod (Bonjour Sleep Proxy).
  Reines WLAN ohne Proxy wacht nicht auf. Dann: Adapter, oder Relais-Option in der Firmware.
- FileVault ist an → nie ausschalten, nur schlafen lassen.

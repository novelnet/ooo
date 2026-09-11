# Roadmap

## v1 – Wecken (fertig)
- [x] Firmware: bis zu 8 bekannte WLANs, Einrichtung über USB, WoL, Ping-Status, LED-Anzeige
- [x] Server auf Deno Deploy mit Deno KV, zwei getrennte Schlüssel
- [x] MCP: Claude kann `wake_mac` und `mac_status` aufrufen
- [x] Mac: `pmset womp 1`, Hintergrunddienst führt WLAN und Adresse automatisch nach
- [x] Ende-zu-Ende geprüft: Handy → Claude → Server → ESP32 → Mac gefunden und gepingt
- [ ] **Offen: ein echter Weckvorgang aus dem Schlaf.** Braucht Ethernet-Adapter am Mac oder
      ein Apple TV / einen HomePod im Netz (Apple weckt über reines WLAN nicht).

## v2 – Wachhalten steuern
Kleiner Poll-Client auf dem Mac, der Befehle abholt:
- `awake on|off` → Amphetamine per AppleScript bzw. `caffeinate`
- `sleep` → `pmset sleepnow`
- `restart` → `sudo fdesetup authrestart` (FileVault-sicher)

## v3 – Arbeiten wie am Schreibtisch
- 1Password Service Account für Deploys ohne Touch ID ([`1password.md`](1password.md))
- Signing-Key ohne Touch-ID-Zwang
- `claude remote-control` beim Login automatisch starten

## Ideen
- Kleine Web-Oberfläche statt Kurzbefehl
- Push „Mac ist wach"

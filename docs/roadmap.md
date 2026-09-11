# Roadmap

## v1 – Wecken (jetzt)
- [x] Firmware: Long-Poll, WoL-Burst, Ping-Check, TLS-Pinning, Watchdog; Relais optional
- [x] API: Queue, ESP32-Status, zwei Tokens
- [x] Mac: `womp 1` + SSH
- [x] iPhone-Kurzbefehl
- [ ] Ende-zu-Ende-Test: `pmset sleepnow`, dann Kurzbefehl vom Handy
- [ ] Falls WLAN-WoL nicht weckt: Ethernet-Adapter oder `RELAY_ENABLED true`

## v2 – Wachhalten steuern
Kleiner Poll-Client auf dem Mac (launchd), der Kommandos abholt:
- `ooo awake on|off` → Amphetamine per AppleScript bzw. `caffeinate`
- `ooo sleep` → `pmset sleepnow`
- `ooo restart` → `sudo fdesetup authrestart` (FileVault-sicher)

## v3 – Arbeiten wie am Schreibtisch
- 1Password Service Account für headless Deploys (`docs/1password.md`)
- Signing-Key ohne Touch-ID-Zwang (1Password SSH Agent, Freigabe per Passwort)
- `claude remote-control` beim Login automatisch starten
- Tailscale-Watchdog

## Ideen
- Web-Oberfläche statt Kurzbefehl
- Push „Mac ist wach“ (ntfy.sh)

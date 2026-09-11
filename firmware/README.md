# firmware – ESP32 Wake-Trigger

Der ESP32 hängt im WLAN, pollt das Relay (Long-Poll, TLS mit gepinnter Root-CA) und weckt den
Mac per Wake-on-LAN. Bei jedem Poll pingt er den Mac und meldet das Ergebnis (`/status`).

Nichts von Hand eintragen:
- **WLAN** richtest du einmal per Handy ein: Der ESP32 öffnet beim ersten Start das WLAN
  `ooo-setup`, verbinden, Heim-WLAN wählen, Passwort eingeben, Hostname des Mac prüfen
  (Standard `MacBookPro`). Wird gespeichert. Ändert sich das WLAN, öffnet sich das Portal wieder
  von selbst; BOOT-Taste 3 s beim Einschalten halten löscht die Zugangsdaten.
- **MAC-Adresse** des Mac lernt der ESP32 aus dem Netz, sobald der Mac einmal wach war.

## Flashen

```bash
brew install platformio             # oder: pipx install platformio
../scripts/secrets.sh render        # include/secrets.h: nur URL + Device-Token aus 1Password
pio run -t upload && pio device monitor
```

Log: `[wifi] verbunden`, `[prefs] Mac-Adresse gelernt: …`, dann alle ~20 s ein stiller Poll.
Test: Mac schlafen legen (`pmset sleepnow`), Kurzbefehl „Mac wecken“ → `[wol] 5 magic packets`
und kurz darauf `ack … result=mac-up`.

## LED

| LED | Bedeutung |
|---|---|
| dauerhaft an | WLAN verbunden, alles gut |
| schnelles Blinken | verbindet gerade / kein WLAN |
| langsames Blinken | Setup-WLAN `ooo-setup` ist offen (oder Wake läuft) |
| 3× kurz | Relay nicht erreichbar, versucht es gleich wieder |

## Optionen (`include/config.h`)

- `RELAY_ENABLED` (Standard `false`): zusätzlich Relais am Netzteil pulsen, siehe `docs/hardware.md`.
- `MAC_HOSTNAME_DEFAULT`, `WOL_BURST`, `WAKE_VERIFY_SEC`, Poll-Intervalle, Watchdog.
- Anderes Board: `board =` in `platformio.ini`, `LED_PIN` prüfen.

## Sicherheit

- Root-CA gepinnt (ISRG Root X1, bis 2035), kein `setInsecure()`.
- Device-Token kann nur Kommandos abholen und bestätigen.
- Setup-Portal ist nur offen, solange kein WLAN da ist (max. 5 min, dann Neustart).

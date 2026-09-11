# ooo — Out Of Office

Dein Mac und deine Agenten laufen weiter, während du unterwegs bist.

Kurzbefehl „Mac wecken“ auf dem iPhone → ESP32 im Heimnetz schickt Wake-on-LAN → Mac ist wach.

```
 iPhone ──POST /wake──▶ Supabase Edge Function ◀──long-poll── ESP32 (WLAN)
                              ooo_commands                     │ Magic Packet + Ping
                                                               ▼
                                                             MacBook
```

| Ordner | Inhalt |
|---|---|
| `firmware/` | ESP32: WLAN-Setup per Handy, lernt Mac-Adresse selbst, Long-Poll, WoL, Ping-Check. Relais optional |
| `api/` | Supabase Edge Function + Migration |
| `mac/` | `setup.sh`: nur `pmset womp 1` |
| `ios/` | Kurzbefehl-Anleitung |
| `scripts/` | `secrets.sh`: alle Secrets in 1Password, Templates rendern |
| `docs/` | Architektur, Entscheidungen, Roadmap, Hardware, 1Password |

## Einrichten

1. `brew install --cask 1password-cli` → `scripts/secrets.sh init` → `scripts/secrets.sh render`
2. `bash mac/setup.sh`
3. API deployen: [`api/README.md`](api/README.md)
4. ESP32 flashen: `cd firmware && pio run -t upload`, dann mit dem Handy ins WLAN `ooo-setup` und Heim-WLAN wählen
5. Handy: [`ios/README.md`](ios/README.md)

**Wichtig:** WoL über WLAN funktioniert bei Apple nur mit Apple TV/HomePod im Netz oder per
Ethernet-Adapter am Mac. FileVault ist an → Mac nur schlafen lassen, nie ausschalten.

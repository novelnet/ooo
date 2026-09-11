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
| `firmware/` | ESP32: Long-Poll, WoL, Ping-Check, Einrichtung über USB. Relais optional |
| `api/` | Supabase Edge Function + Migration |
| `mac/` | `setup.sh`: richtet den ESP32 über USB ein, optional automatisch beim Anstecken |
| `ios/` | Kurzbefehl-Anleitung |
| `scripts/` | `secrets.sh`: alle Secrets in 1Password, Templates rendern |
| `docs/` | Architektur, Entscheidungen, Roadmap, Hardware, 1Password |

## Einrichten

1. `brew install --cask 1password-cli` → `scripts/secrets.sh init` → `scripts/secrets.sh render`
2. API deployen: [`api/README.md`](api/README.md)
3. ESP32 per USB anstecken und flashen: `cd firmware && pio run -t upload`
4. `bash mac/setup.sh` – wählt das WLAN selbst und überträgt alles über das Kabel. Blaue LED leuchtet = fertig.
   Mit `bash mac/setup.sh --install-auto` genügt künftig Anstecken, ganz ohne Befehl.
5. Handy: [`ios/README.md`](ios/README.md)

**Wichtig:** WoL über WLAN funktioniert bei Apple nur mit Apple TV/HomePod im Netz oder per
Ethernet-Adapter am Mac. FileVault ist an → Mac nur schlafen lassen, nie ausschalten.

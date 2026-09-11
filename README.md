# ooo — Out Of Office

Dein Mac und deine Agenten laufen weiter, während du unterwegs bist.

Kurzbefehl „Mac wecken“ auf dem iPhone → ESP32 im Heimnetz schickt Wake-on-LAN → Mac ist wach.

```
 iPhone ──POST /wake──┐
                      ├─▶ Deno Deploy (ooo) ◀──warten── ESP32 (WLAN)
 Claude ──MCP wake_mac┘        Deno KV                   │ Magic Packet + Ping
                                                         ▼
                                                      MacBook
```

| Ordner | Inhalt |
|---|---|
| `firmware/` | ESP32: bis zu 8 bekannte WLANs, WoL, Ping-Check, Einrichtung über USB. Relais optional |
| `server/` | Briefkasten auf Deno Deploy (Deno KV) inklusive MCP für Claude |
| `mac/` | `setup.sh`: richtet den ESP32 über USB ein und hält ihn bei Netzwechseln aktuell |
| `ios/` | Kurzbefehl-Anleitung |
| `scripts/` | `secrets.sh`: alle Secrets in 1Password, Templates rendern |
| `docs/` | Architektur, Entscheidungen, Roadmap, Hardware, 1Password |

## Einrichten

1. `brew install --cask 1password-cli` → `scripts/secrets.sh init` → `scripts/secrets.sh render`
2. Server veröffentlichen: [`server/README.md`](server/README.md)
3. ESP32 per USB anstecken und flashen: `cd firmware && pio run -t upload`
4. `bash mac/setup.sh --install-auto` – überträgt alle nutzbaren WLANs und hält sie aktuell.
   Der ESP32 bleibt am USB-Anschluss des MacBooks stecken und reist mit.
5. Handy: [`ios/README.md`](ios/README.md)

**Wichtig:** WoL über WLAN funktioniert bei Apple nur mit Apple TV/HomePod im Netz oder per
Ethernet-Adapter am Mac. FileVault ist an → Mac nur schlafen lassen, nie ausschalten.

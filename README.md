# ooo — Out Of Office

Dein Mac und deine Agenten laufen weiter, während du unterwegs bist.

Vom Handy aus: „Weck meinen Mac auf." Claude ruft das Werkzeug `wake_mac` auf, der ESP32 im
selben Netz wie der Mac schickt ein Wake-on-LAN-Paket, der Mac ist wach.

```
 iPhone ──POST /wake──┐
                      ├─▶ Deno Deploy „ooo" ◀──wartet── ESP32 (WLAN)
 Claude ──MCP wake_mac┘        Deno KV                   │ Magic Packet + Ping
                                                         ▼
                                                      MacBook
```

| Ordner | Inhalt |
|---|---|
| `firmware/` | ESP32: bis zu 8 bekannte WLANs, Einrichtung über USB, WoL, Ping-Status, LED |
| `server/` | Briefkasten auf Deno Deploy (Deno KV), inklusive MCP für Claude |
| `mac/` | Einrichtung über USB, Hintergrunddienst für Netzwechsel |
| `ios/` | Claude-Connector und Kurzbefehl |
| `scripts/` | `secrets.sh`: alle Schlüssel in 1Password |
| `docs/` | Architektur, Entscheidungen, Roadmap, Hardware, 1Password |

## Einrichten

```bash
brew install --cask 1password-cli && pipx install platformio
scripts/secrets.sh init            # zwei Schlüssel erzeugen (einmalig)
scripts/secrets.sh render          # firmware/include/secrets.h schreiben
scripts/secrets.sh push            # Schlüssel zu Deno Deploy schieben
cd server && deno deploy --org novelnet --app ooo --prod
cd ../firmware && pio run -t upload
cd .. && bash mac/setup.sh --install-auto
```

Danach den Connector in der Claude-App eintragen: [`ios/README.md`](ios/README.md).
Der ESP32 bleibt am USB-Anschluss des MacBooks stecken – er reist mit, bekommt im Schlaf
weiter Strom und bekommt bei jedem WLAN-Wechsel automatisch die neuen Zugangsdaten.

## Vor dem Loslegen wissen

- **Wake-on-LAN über reines WLAN weckt Apple-Macs nicht.** Es braucht ein Apple TV oder einen
  HomePod im Netz, oder einen USB-C-Ethernet-Adapter am Mac.
- **Der ESP32 kann nur 2,4 GHz**, keine reinen 5-GHz-Netze.
- **FileVault ist an** → den Mac nur schlafen legen, nie ausschalten.

Details und Begründungen: [`docs/architecture.md`](docs/architecture.md),
[`docs/decisions.md`](docs/decisions.md).

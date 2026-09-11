# ooo — Out Of Office

Dein Mac und deine Agenten laufen weiter, während du unterwegs bist.

Vom Handy aus: „Weck meinen Mac auf." Claude ruft das Werkzeug `wake_mac` auf, ein ESP32 im
selben WLAN wie der Mac schickt ein Wake-on-LAN-Paket, der Mac ist wach.

```
 iPhone ──POST /wake──┐
                      ├─▶ Deno Deploy „ooo" ◀──wartet── ESP32 (WLAN)
 Claude ──MCP wake_mac┘        Deno KV                   │ Magic Packet + Ping
                                                         ▼
                                                      MacBook
```

Der ESP32 wartet dauerhaft beim Server auf Befehle. Weil er die Verbindung selbst aufbaut,
braucht der Router keine Freigabe, und es funktioniert aus dem Mobilfunk. Ob der Mac wach ist,
stellt der ESP32 per Ping fest; auf dem Mac läuft dafür nichts.

| Ordner | Inhalt |
|---|---|
| `firmware/` | ESP32: bis zu 8 bekannte WLANs, Einrichtung über USB, WoL, Ping, LED, Relais-Option |
| `server/` | Deno Deploy mit Deno KV, inklusive MCP für Claude |
| `mac/` | Einrichtung über USB und Hintergrunddienst für Netzwechsel |
| `scripts/` | `secrets.sh`: alle Schlüssel in 1Password |
| `docs/` | [1Password als Entwickler-Werkzeug](docs/1password.md) |

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

Der ESP32 bleibt am USB-Anschluss des MacBooks stecken. Er reist mit, bekommt auch im Schlaf
Strom, und bei jedem WLAN-Wechsel schiebt der Mac ihm die neuen Zugangsdaten nach.

## Vom Handy

**Claude-App.** Einstellungen → Connectors → *Add custom connector*, Name `ooo`, Adresse:

```
https://ooo.novelnet.deno.net/mcp/<user-token>
```

Den Schlüssel liefert `op read op://Personal/ooo/tokens/user`. Danach im Chat über das
Werkzeug-Symbol aktivieren. Dann genügt „Weck meinen Mac auf" oder „Ist mein Mac wach?".

Connectors können keine getrennten Kopfzeilen mitschicken, deshalb steht der Schlüssel in der
Adresse. **Behandle sie wie ein Passwort** und schreib sie in keine Datei. Wer sie kennt, kann
den Mac wecken und den Status lesen – mehr nicht, anmelden kann sich damit niemand.

**Ohne Claude** geht es auch per Kurzbefehl: „Inhalt von URL abrufen", POST auf
`https://ooo.novelnet.deno.net/wake`, Kopfzeile `Authorization: Bearer <user-token>`.

## Grenzen

- **Wake-on-LAN über reines WLAN weckt Apple-Macs nicht.** Es braucht ein Apple TV oder einen
  HomePod im Netz (die dienen als Sleep-Proxy) oder einen USB-C-Ethernet-Adapter am Mac.
  Sonst bleibt die Relais-Option, siehe [`firmware/README.md`](firmware/README.md).
- **Der ESP32 kann nur 2,4 GHz.** Netze, die ausschließlich auf 5 GHz funken, sieht er nicht.
- **FileVault ist an** → den Mac nur schlafen legen, nie ausschalten. Ein Kaltstart bliebe am
  Pre-Boot-Login hängen. Neustart aus der Ferne nur mit `sudo fdesetup authrestart`.
- **Touch ID gibt es aus der Ferne nicht** → [`docs/1password.md`](docs/1password.md).

## Als Nächstes

- Ein echter Weckvorgang aus dem Schlaf ist noch nicht gelaufen (braucht eine der Lösungen oben).
- Wachhalten steuern: `caffeinate` bzw. Amphetamine, `pmset sleepnow`, `fdesetup authrestart`.
- `claude remote-control` beim Login automatisch starten.

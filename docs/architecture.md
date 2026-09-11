# Architektur

Ein zugeklapptes MacBook soll von unterwegs aufwachen. Danach hält Arbeit (Amphetamine,
SSH, Claude Code Remote Control) es wach; das ist nicht Aufgabe von ooo.

## Ablauf „Mac wecken“

1. Handy → `POST /wake` → Zeile in `ooo_commands`.
2. ESP32 hängt in `POST /poll` (bis 25 s). `kv.watch` weckt den Server, sobald der Befehl da ist; gemessene Latenz unter 1 s.
3. ESP32 prüft per mDNS + Ping, ob der Mac schon wach ist. Wenn nicht: 5 Magic Packets an
   die gelernte MAC-Adresse, dann bis 45 s auf Ping-Antwort warten.
4. `POST /ack` mit `mac-up`, `already-awake` oder `no-ping-response`.
5. `/status` zeigt bei jedem Poll den aktuellen Ping-Stand als `mac.awake`.

## Warum so

- **Briefkasten statt Port-Freigabe:** Der ESP32 baut nur ausgehende Verbindungen auf, Handy und
  Claude legen Befehle ab. Funktioniert aus Mobilfunk mit CGNAT.
- **Deno Deploy statt Supabase:** siehe Entscheidung 15. Kostenlos, kein Server, kein Datenbankschema.
- **MCP eingebaut:** Claude kann den Mac über das Werkzeug `wake_mac` selbst wecken.
- **Status per Ping vom ESP32:** Auf dem Mac läuft nichts. Schläft er, scheitert schon die
  mDNS-Auflösung, das ist die Antwort.
- **Keine Konfiguration von Hand:** Der Mac schickt WLAN-Zugangsdaten, seinen Namen und seine
  MAC-Adresse über das USB-Kabel an den ESP32 (`mac/setup.sh`), der speichert sie im Flash.
  Wechselt der Mac später das Interface, frischt der ESP32 die MAC-Adresse per ARP selbst auf.
  Nur URL + Device-Token werden beim Flashen aus 1Password gerendert.
- **Relais nur optional:** `RELAY_ENABLED true` schaltet zusätzlich das Netzteil aus/an
  (weckt Apple Silicon immer, auch ohne Ethernet). Standard ist aus, siehe `docs/hardware.md`.

## Sicherheit

| Wer | Token | darf |
|---|---|---|
| Handy | `user` | wecken, Status lesen, Relais schalten |
| ESP32 | `device` | Kommandos abholen und bestätigen |

Tokens 32 Byte zufällig, Vergleich über SHA-256, Verwaltung in 1Password. Tabellen mit RLS ohne
Policies (nur Service Role). ESP32 spricht TLS mit gepinnter Root-CA. Schlimmster Fall bei
geleaktem User-Token: jemand weckt den Mac. Der bleibt hinter FileVault, Login und Tailscale.

## Grenzen

- **WoL über WLAN:** Apple weckt nur über Bonjour Sleep Proxy (Apple TV/HomePod) oder Ethernet.
- **Kaltstart mit FileVault** endet am Pre-Boot-Login → nie ausschalten.
- **Touch ID** gibt es remote nicht → `docs/1password.md`.

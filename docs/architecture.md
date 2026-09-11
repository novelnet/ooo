# Architektur

Ein zugeklapptes MacBook soll von unterwegs aufwachen. Danach hält Arbeit (Amphetamine,
SSH, Claude Code Remote Control) es wach; das ist nicht Aufgabe von ooo.

## Ablauf „Mac wecken“

1. Handy → `POST /wake` → Zeile in `ooo_commands`.
2. ESP32 hängt in `POST /poll` (Long-Poll bis 25 s), bekommt das Kommando nach ≤ 1–2 s.
3. ESP32 prüft per mDNS + Ping, ob der Mac schon wach ist. Wenn nicht: 5 Magic Packets an
   die gelernte MAC-Adresse, dann bis 45 s auf Ping-Antwort warten.
4. `POST /ack` mit `mac-up`, `already-awake` oder `no-ping-response`.
5. `/status` zeigt bei jedem Poll den aktuellen Ping-Stand als `mac.awake`.

## Warum so

- **Cloud-Relay statt Port-Freigabe:** ESP32 baut nur ausgehende Verbindungen auf, das Handy
  schreibt in eine Queue. Funktioniert aus Mobilfunk mit CGNAT.
- **Status per Ping vom ESP32:** Auf dem Mac läuft nichts. Schläft er, scheitert schon die
  mDNS-Auflösung, das ist die Antwort.
- **Keine Konfiguration von Hand:** WLAN über Setup-Portal (`ooo-setup`) auf dem ESP32 gespeichert,
  MAC-Adresse des Mac aus der ARP-Tabelle gelernt. Nur URL + Device-Token werden beim Flashen
  aus 1Password gerendert.
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

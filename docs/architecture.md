# Architektur

Ein schlafendes MacBook soll von unterwegs aufwachen – vom Handy oder von Claude aus.
Wachhalten ist nicht Aufgabe von ooo, das erledigt die Arbeit selbst (Amphetamine, offene
Sitzungen).

```
 iPhone ──POST /wake──┐
                      ├─▶ Deno Deploy „ooo" ◀──wartet── ESP32 (im selben WLAN wie der Mac)
 Claude ──MCP wake_mac┘        Deno KV                   │ Magic Packet + Ping
                                                         ▼
                                                      MacBook
```

## Ablauf „Mac wecken"

1. Handy oder Claude legt einen Befehl ab (`POST /wake` bzw. MCP-Werkzeug `wake_mac`).
2. Der ESP32 wartet bereits in `POST /poll` (bis 25 s). `kv.watch` weckt den Server in dem
   Moment, in dem der Befehl eintrifft – gemessene Latenz unter einer Sekunde.
3. Der ESP32 prüft per mDNS und Ping, ob der Mac schon wach ist. Wenn nicht: fünf Runden
   Magic Packets an alle für dieses Netz bekannten Adressen, dann bis 45 s auf Antwort warten.
4. `POST /ack` meldet `mac-up`, `already-awake` oder `no-ping-response`.
5. `GET /status` zeigt den bei jedem Durchgang aktualisierten Ping-Stand als `mac.awake`.

## Warum so

- **Briefkasten statt Port-Freigabe.** Der ESP32 baut nur ausgehende Verbindungen auf, Handy
  und Claude legen Befehle ab. Funktioniert aus dem Mobilfunk, auch hinter CGNAT.
- **Deno Deploy statt eines eigenen Servers** (Entscheidung 15): kostenlos, kein Datenbankschema,
  und `kv.watch` macht das Warten ohne Rechenzeit.
- **MCP ist eine Route derselben Anwendung** (Entscheidung 16). Claude bekommt die Werkzeuge
  `wake_mac` und `mac_status`, ohne zusätzliche Infrastruktur.
- **Status per Ping vom ESP32.** Auf dem Mac läuft dafür nichts. Schläft er, scheitert schon die
  mDNS-Auflösung – das ist die Antwort.
- **Nichts von Hand konfigurieren.** Der Mac schickt WLAN-Zugangsdaten, seinen Namen und seine
  tatsächlich benutzte Netzwerkadresse über das USB-Kabel (`mac/setup.sh`). Beim Flashen kommen
  nur Server-Adresse und Geräteschlüssel aus 1Password.
- **Unterwegs tauglich.** Der ESP32 kennt bis zu acht Netze und nimmt das stärkste. Ein
  Hintergrunddienst auf dem Mac erkennt Netzwechsel und schiebt neue Zugangsdaten nach
  (Entscheidung 17). Empfohlen: ESP32 bleibt am USB-Anschluss des MacBooks stecken.
- **Relais nur optional.** `RELAY_ENABLED true` schaltet zusätzlich das Netzteil aus und an;
  das weckt Apple Silicon immer, auch ohne Ethernet. Standard ist aus, siehe `hardware.md`.

## Sicherheit

| Wer | Schlüssel | darf |
|---|---|---|
| Handy, Claude, CLI | `user` | wecken, Status lesen, Relais schalten, MCP nutzen |
| ESP32 | `device` | Befehle abholen und bestätigen |

Beide Schlüssel sind 32 Byte Zufall, liegen in 1Password und werden über SHA-256 verglichen,
damit die Prüfdauer nichts verrät. Der Zustand liegt in Deno KV, erreichbar nur über die
Anwendung. Der ESP32 spricht TLS mit gepinnten Wurzelzertifikaten (ISRG X1 und X2), kein
`setInsecure()`.

Schlimmster Fall bei einem geleakten `user`-Schlüssel: Jemand weckt den Mac oder liest, ob er
wach ist. Der Mac selbst bleibt hinter FileVault und Anmeldung. Die Adresse des
Claude-Connectors enthält diesen Schlüssel – deshalb gehört sie behandelt wie ein Passwort und
niemals ins Repository.

## Grenzen

- **Wake-on-LAN über reines WLAN weckt Apple-Macs nicht.** Es braucht einen Bonjour Sleep Proxy
  im Netz (Apple TV, HomePod) oder einen Ethernet-Adapter am Mac.
- **Der ESP32 kann nur 2,4 GHz.** Netze, die ausschließlich auf 5 GHz funken, sieht er nicht.
- **Kaltstart mit FileVault** endet am Pre-Boot-Login. Deshalb: nie ausschalten, nur schlafen.
- **Touch ID gibt es aus der Ferne nicht** → `1password.md`.

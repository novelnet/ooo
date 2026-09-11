# ooo – Hinweise für Claude Code

- Sprache in Dokumentation und Kommentaren: Deutsch. Bezeichner im Code: Englisch.
- **Einfach halten.** Wake-on-LAN ist der Weckweg, das Relais bleibt eine abschaltbare Option,
  auf dem Mac läuft nur das Nötigste. Jede Datei muss sich rechtfertigen.
- Schlüssel ausschließlich über 1Password (`op://Personal/ooo/…`) und die `*.tpl`-Vorlage.
  Niemals `secrets.h` committen oder einen Schlüssel in eine Datei schreiben – die Adresse des
  Claude-Connectors enthält einen, sie gehört nicht ins Repository.
- Struktur: `firmware/` (PlatformIO, ESP32) · `server/` (Deno Deploy, Deno KV, MCP) ·
  `mac/` (Einrichtung über USB, Hintergrunddienst) · `scripts/` · `docs/`.
  Jeder Ordner erklärt sich in seiner eigenen README.
- Prüfen: `cd firmware && pio run` · `cd server && deno check main.ts`
- Veröffentlichen: `cd server && deno deploy --org novelnet --app ooo --prod`
  (`DENO_DEPLOY_TOKEN` aus 1Password)
- Drei Dinge, die beim Ändern leicht kaputtgehen:
  1. Die Netzwerkadresse des Macs ist **pro WLAN verschieden** und wechselt gelegentlich.
     Deshalb schickt der Mac sie selbst (`MACADDR`, nach dem Verbinden) und der ESP32 behält
     je Netz die letzten drei.
  2. macOS gibt den Namen des verbundenen WLANs nur mit Ortungsdienste-Berechtigung heraus.
     Deshalb der Umweg über die Schnittmenge aus bekannten und sichtbaren Netzen.
  3. Die Relais-Option schaltet 230 V. Warnhinweise in `firmware/README.md` nicht entschärfen.

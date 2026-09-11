# ooo – Hinweise für Claude Code

- Sprache in Dokumentation und Kommentaren: Deutsch. Bezeichner im Code: Englisch.
- **Einfach und clean halten.** Wake-on-LAN ist der Weckweg, das Relais bleibt eine
  abschaltbare Option, auf dem Mac läuft nur das Nötigste.
- Schlüssel ausschließlich über 1Password (`op://Personal/ooo/…`) und die `*.tpl`-Vorlage.
  Niemals `secrets.h` committen oder einen Schlüssel in eine Datei schreiben – die Adresse
  des Claude-Connectors enthält einen, sie gehört nicht ins Repo.
- Struktur: `firmware/` (PlatformIO, ESP32) · `server/` (Deno Deploy, Deno KV, MCP) ·
  `mac/` (Einrichtung über USB, Hintergrunddienst) · `ios/` · `scripts/` · `docs/`.
- Prüfen: `cd firmware && pio run` · `cd server && deno check main.ts`
- Veröffentlichen: `cd server && deno deploy --org novelnet --app ooo --prod`
  (Schlüssel via `DENO_DEPLOY_TOKEN` aus 1Password)
- Entscheidungen stehen in `docs/decisions.md`: **ergänzen, nicht umschreiben.**
  `docs/planning-notes.md` ist ein historisches Dokument und wird nicht nachgeführt.
- Die Hardware-Option schaltet 230 V: Warnhinweise in `docs/hardware.md` nicht entschärfen.

# ooo – Hinweise für Claude Code

- Sprache in Docs und Kommentaren: Deutsch. Code-Bezeichner Englisch.
- Secrets nur über 1Password-Templates (`*.tpl` + `op inject`). Nie `.env` oder `secrets.h`
  committen oder Tokens in Docs schreiben.
- Einfach und clean halten: WoL ist der Weckweg, das Relais bleibt eine abschaltbare Option,
  auf dem Mac läuft nichts außer `womp 1`.
- Struktur: `firmware/` (PlatformIO, ESP32) · `api/` (Supabase Edge Function + Migration) ·
  `mac/` (setup.sh) · `ios/` · `scripts/` · `docs/`.
- Entscheidungen stehen in `docs/decisions.md` – bei Änderungen dort ergänzen.
- Firmware prüfen: `cd firmware && pio run` (secrets.h aus Template rendern oder Dummy anlegen).
- API prüfen: `cd api/supabase/functions/ooo && deno check index.ts`.
- Hardware schaltet 230 V: Warnhinweise in `docs/hardware.md` nicht entschärfen.

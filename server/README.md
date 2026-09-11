# server – der Briefkasten

Läuft auf [Deno Deploy](https://deno.com/deploy), Zustand in Deno KV. Kein eigener Server,
keine Datenbank, kostenlos.

| Route | Schlüssel | Zweck |
|---|---|---|
| `POST /wake` | user | Weckbefehl einreihen |
| `GET /status` | user | ESP32 online? Mac wach? |
| `POST /poll {"wait":20,"info":{…}}` | device | ESP32 wartet bis 25 s auf einen Befehl |
| `POST /ack {"id":…,"result":"…"}` | device | ESP32 bestätigt die Ausführung |
| `POST /mcp` | user | MCP für Claude: `wake_mac`, `mac_status` |
| `POST /mcp/<user-token>` | user | dasselbe, Schlüssel in der Adresse |

Das Warten kostet keine Rechenzeit: `kv.watch` meldet einen neuen Befehl von selbst, es wird
nicht im Kreis abgefragt. Gemessen: Ein Weckbefehl kommt in derselben Sekunde beim ESP32 an.

## Lokal testen

```bash
cd server
OOO_USER_TOKEN=... OOO_DEVICE_TOKEN=... deno run --unstable-kv --allow-net --allow-env --allow-read --allow-write main.ts
```

## Claude anbinden

```bash
claude mcp add --transport http ooo https://<app>.deno.net/mcp --header "Authorization: Bearer $(op read 'op://Personal/ooo/tokens/user')"
```

Für die Claude-App auf dem Handy, die keine eigenen Kopfzeilen mitschicken kann, stattdessen die
Adresse mit dem Schlüssel darin verwenden: `https://<app>.deno.net/mcp/<user-token>`.

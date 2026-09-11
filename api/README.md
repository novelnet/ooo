# api – Cloud-Relay (Supabase Edge Function)

Eine einzige Funktion `ooo` mit fünf Routen und zwei Tabellen. Zwei getrennte Bearer-Tokens
(Handy/CLI, ESP32) – jeder darf nur seine Routen.

| Route | Token | Zweck |
|---|---|---|
| `POST /wake` | user | Kommando `wake` einreihen |
| `POST /power {"state":"on\|off"}` | user | optionales Relais schalten |
| `GET /status` | user | ESP32 online?, `mac.awake` (Ping vom ESP32), letzte 5 Kommandos |
| `POST /poll {"wait":20,"info":{"mac_reachable":…}}` | device | ESP32 wartet bis 25 s auf ein Kommando (Long-Poll) |
| `POST /ack {"id":…,"result":"ok"}` | device | Kommando bestätigen |

Unbestätigte Kommandos verfallen nach 15 Minuten, damit ein ESP32, der lange offline war,
nicht mit Verspätung den Mac weckt.

## Deploy (einmalig, ~5 Minuten)

```bash
cd api
supabase login                                   # öffnet Browser
supabase link --project-ref <ref>                # bestehendes oder neues Projekt (supabase projects create ooo)
supabase db push                                 # legt ooo_devices / ooo_commands an
../scripts/secrets.sh push                       # Tokens aus 1Password → Function Secrets
supabase functions deploy ooo --no-verify-jwt
```

## Test

```bash
TOKEN=$(op read op://Dev/ooo/tokens/user); URL=https://<ref>.supabase.co/functions/v1/ooo
curl -s -H "Authorization: Bearer $TOKEN" $URL/status
curl -s -X POST -H "Authorization: Bearer $TOKEN" $URL/wake   # der ESP32 holt es innerhalb ~1 s ab
```

## Kosten

Long-Polling à 20 s ≈ 130 000 Aufrufe/Monat, weit unter den 500 000 des Free-Tiers.

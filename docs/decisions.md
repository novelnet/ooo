# Entscheidungen

Kurze ADRs. Wenn etwas später anders gemacht wird: Eintrag ergänzen, nicht umschreiben.

## 1. Wecken per Wake-on-LAN, Relais nur als Option
**Optionen:** (a) Wake-on-LAN, (b) Relais im Netzteilstrang, (c) ESP32 als BLE-Tastatur, (d) ESP32-S3 als USB-Tastatur.
**Entscheidung (2026-09-11):** (a). (b) bleibt im Code, per `RELAY_ENABLED` abschaltbar, Standard aus.
**Warum:** Einfach und clean: kein 230-V-Aufbau, nichts auf dem Mac außer `womp 1`. Der Mac bleibt
im Betrieb ohnehin wach (Amphetamine, Sessions). Preis: WoL über WLAN braucht bei Apple einen
Bonjour Sleep Proxy oder Ethernet; falls das im Alltag nicht reicht, ist das Relais mit einem Flag da.
(Ursprünglich war (b) primär geplant, wurde am 2026-09-11 als zu viel verworfen.)

## 2. Supabase Edge Function als Relay
**Optionen:** Supabase, Vercel Function + Redis, Cloudflare Worker + KV, MQTT-Broker.
**Entscheidung:** Supabase.
**Warum:** War im Planungsgespräch schon gesetzt; CLI ist installiert; Tabelle + Funktion in
einem Projekt; Free-Tier reicht (Long-Poll ≈ 130 k Aufrufe/Monat). Die Funktion hat sechs
Routen und keine Supabase-spezifische Logik – ein Port auf Vercel/Cloudflare wäre ein Nachmittag.

## 3. Long-Polling statt kurzem Polling oder WebSocket
**Warum:** ~1 s Latenz bei 1/10 der Aufrufe gegenüber 2-s-Polling. WebSocket/Realtime auf dem
ESP32 ist mehr Code und mehr Fehlerquellen für wenig Gewinn.

## 4. Zwei Tokens statt einem
**Warum:** Der Token im iPhone-Kurzbefehl (iCloud!) darf nicht das sein, was auf dem ESP32
im Flash liegt. Kleinster Schaden pro Leck.

## 9. WLAN per Setup-Portal, MAC-Adresse gelernt
**Warum:** Nichts eintippen, nichts neu flashen, wenn sich das WLAN ändert. Der ESP32 merkt
sich Zugangsdaten und die MAC des Macs selbst (NVS). Nur URL + Device-Token kommen aus 1Password.

## 8. Mac-Status per Ping vom ESP32, kein Heartbeat auf dem Mac
**Warum:** Null Setup auf dem Mac, kein dritter Token, kein LaunchAgent. mDNS + Ping im LAN
reicht als „wach/schläft“.

## 5. Root-CA gepinnt statt `setInsecure()`
**Warum:** Ein MITM im Heimnetz könnte sonst Kommandos unterschieben. ISRG Root X1 gilt bis
2035; sollte Supabase die CA wechseln, ist ein Header-Update nötig (steht im Firmware-Log als TLS-Fehler).

## 6. Secrets ausschließlich in 1Password
**Warum:** Ein Item, drei Templates, `op inject`. Kein `.env` im Repo, kein Copy-Paste über
Geräte hinweg – das gleiche Muster für alle weiteren Projekte.

## 7. FileVault bleibt an
**Warum:** Ein gestohlenes MacBook mit allen Repos und Tokens wäre der eigentliche GAU. Preis:
kein Kaltstart aus der Ferne. Regel: nur schlafen lassen, Neustart per `fdesetup authrestart`.

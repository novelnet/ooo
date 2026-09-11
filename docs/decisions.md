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

## 9. Einrichtung über das USB-Kabel statt Setup-WLAN
**Optionen:** (a) WLAN-Setup-Portal am Handy (WiFiManager), (b) Zugangsdaten fest in die Firmware,
(c) der Mac schickt alles über USB.
**Entscheidung (2026-09-11):** (c). (a) war dem User zu umständlich, (b) heißt neu flashen bei jedem
WLAN-Wechsel.
**Warum:** Anstecken, einen Befehl, LED leuchtet. Der Mac kennt WLAN-Name, Passwort (Schlüsselbund),
seinen Namen und seine MAC-Adresse ohnehin. Spart zusätzlich 50 % Flash (kein WiFiManager).
**Preis:** Zum Einrichten muss der ESP32 einmal am Mac stecken, und das Skript braucht einmal `sudo`
für den Schlüsselbund.

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

## 10. Zustandsanzeige nur über die blaue LED
**Warum:** Auf dem ESP32-DevKit ist die rote LED mit PWR beschriftet und fest mit der
Stromversorgung verdrahtet – per Software nicht schaltbar (am 2026-09-11 gemessen: alle GPIOs
durchgeschaltet, Rot blieb unverändert). Steuerbar ist nur Blau an GPIO 2. Deshalb bedeutet
"blau aus" = nur Rot sichtbar = nicht bereit, und die übrigen Zustände unterscheiden sich über
das Blinkmuster. Wer echte Farbwechsel will, löten eine zweite LED an einen freien GPIO.

## 11. WLAN-Auswahl über den Scan des ESP32
**Warum:** macOS 26 gibt den Namen des verbundenen WLANs nur an Programme mit Berechtigung für
Ortungsdienste heraus; ohne sie liefert `ipconfig getsummary` wörtlich `<redacted>` (am
2026-09-11 aufgetreten: der ESP32 versuchte, sich mit einem Netz namens "<redacted>" zu
verbinden). Statt eine Systemberechtigung zu verlangen, scannt der ESP32 selbst und der Nutzer
wählt aus einer Liste. Nebeneffekt: Es erscheinen nur 2,4-GHz-Netze, die er auch erreichen kann.

## 12. WLAN automatisch über die Schnittmenge bestimmen
**Warum:** Der Nutzer will nicht jedes Mal auswählen. Der Mac gibt zwar nicht heraus, in welchem
Netz er steckt (Ortungsdienste-Sperre, siehe 11), aber `networksetup -listpreferredwirelessnetworks`
liefert ohne Berechtigung alle bekannten Netze in Reihenfolge. Geschnitten mit dem Scan des ESP32
bleibt praktisch immer genau eins übrig; bei mehreren gewinnt das oberste der Mac-Reihenfolge.
`--waehlen` erzwingt weiterhin die Liste.

## 13. Optionaler Hintergrunddienst statt Befehl
**Warum:** "Automatisch beim Anstecken" geht nur mit einem Dienst auf dem Mac. Deshalb `com.ooo.watch`
(alle 10 s ein `ls` auf /dev), aber **opt-in** über `--install-auto` – der Standard bleibt der eine
Befehl. Der Dienst reagiert nur auf neu erschienene Geräte, weil das Öffnen der seriellen
Schnittstelle den ESP32 neu startet.

## 14. Die MAC-Adresse fürs Wecken muss gelernt werden, nicht übertragen
**Beobachtung (2026-09-11, Test im iPhone-Hotspot):** Die Hardware-Adresse des Macs ist
`f4:d4:88:84:76:a6`, im Netz benutzt er aber `12:ac:41:9b:31:54`. macOS vergibt pro WLAN eine
**private, zufällige WLAN-Adresse** (erkennbar am gesetzten "locally administered"-Bit).
**Folge:** Ein Magic Packet an die Hardware-Adresse weckt den Mac **nicht**. Richtig ist die
Adresse aus der ARP-Tabelle, die der ESP32 sich nach jedem erfolgreichen Ping merkt. Die beim
Einrichten übertragene Hardware-Adresse ist deshalb nur ein Startwert und wird überschrieben,
sobald der Mac einmal wach im selben Netz gesehen wurde.
**Empfehlung fürs Heimnetz:** In den WLAN-Einstellungen des Macs "Private WLAN-Adresse" auf
"Fest" stellen (nicht "Rotierend"), sonst ändert sie sich und das Wecken schlägt fehl, bis der
Mac wieder einmal wach gesehen wurde.

## 15. Deno Deploy statt Supabase
**Entscheidung (2026-09-11):** Der Briefkasten läuft auf Deno Deploy mit Deno KV.
**Warum:** Supabase schied aus Kostengründen aus – pro Konto ist nur **eine** kostenlose
Organisation erlaubt, und die vorhandene hatte ihre zwei Projekte bereits belegt; ein weiteres
hätte 10 $/Monat gekostet. Ein Mitlaufen im Produktivprojekt fluss.ai wollte der Nutzer nicht
(zwei Fremdtabellen in der Produktionsdatenbank). Deno Deploy ist dauerhaft kostenlos
(1 Mio. Anfragen/Monat gegen unseren Bedarf von ~130.000), bringt den Schlüsselspeicher mit und
der vorhandene Code war bereits Deno-TypeScript.
**Nebengewinn:** `kv.watch` ersetzt die sekündliche Datenbankabfrage aus dem Supabase-Entwurf.
Der Server verbraucht beim Warten praktisch keine Rechenzeit.

## 16. MCP direkt im Briefkasten
**Warum:** Claude soll den Mac selbst wecken können. Ein MCP-Server braucht einen Endpunkt im
Internet – Briefkastendienste wie ntfy oder Upstash können das nicht. Deshalb ist MCP eine
weitere Route derselben Anwendung, rund 60 Zeilen, ohne zusätzliche Abhängigkeit.
**Anmeldung:** Bearer-Schlüssel im Kopf der Anfrage (Claude Code) oder im Pfad `/mcp/<token>`
für Clients, die keine Kopfzeilen mitschicken können.

## 17. Mehrere Netze und automatische Nachführung
**Anforderung (2026-09-11):** "unabhängig vom wifi ob hotspot etc" und "ist aber dynamisch,
immer wieder neue netze".
**Entscheidung:** Der ESP32 merkt sich bis zu acht Netze samt der je Netz gelernten
MAC-Adresse des Macs und nimmt beim Start das stärkste bekannte. Zusätzlich hält ein
Hintergrunddienst auf dem Mac ihn aktuell: Er erkennt einen Netzwechsel am Standard-Gateway
plus dessen MAC-Adresse (der WLAN-Name ist ohne Ortungsdienste-Berechtigung nicht lesbar) und
überträgt dann die Zugangsdaten des neuen Netzes.
**Empfohlener Aufbau:** ESP32 bleibt am USB-Anschluss des MacBooks – er reist mit, bekommt im
Schlaf weiter Strom und ist damit immer im selben Netz wie der Mac.
**Warum die MAC-Adresse je Netz:** macOS vergibt pro WLAN eine eigene private Adresse. Gemessen:
Hardware `f4:d4:88:84:76:a6`, im Hotspot `12:ac:41:9b:31:54`, im Heimnetz `86:83:c3:fa:70:58`.

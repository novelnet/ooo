# ios – Vom iPhone aus

## Kurzbefehl „Mac wecken“ (2 Minuten)

1. Kurzbefehle-App → „+“ → Name **Mac wecken**
2. Aktion **„Inhalt von URL abrufen“**
   - URL: `https://<ref>.supabase.co/functions/v1/ooo/wake`
   - Methode: **POST**
   - Header: `Authorization` = `Bearer <user-token aus 1Password: op://Dev/ooo/tokens/user>`
3. Aktion **„Mitteilung anzeigen“** mit dem Ergebnis
4. Zum Home-Bildschirm hinzufügen und/oder per Siri: „Hey Siri, Mac wecken“

Zweiter Kurzbefehl **Mac Status** genauso mit `GET …/ooo/status`.

Der User-Token kann nur wecken und Status abfragen – das ist bewusst so, damit ein
Kurzbefehl in iCloud kein größeres Risiko ist. Bei Verdacht: Token in 1Password neu
generieren, `scripts/secrets.sh push`, Kurzbefehl anpassen.

## Danach weiterarbeiten

Auf dem Mac läuft `claude remote-control`; die Session erscheint in der Claude-App bzw. auf
claude.ai/code. Mehr braucht es für KI-Arbeit vom Handy nicht. Bildschirm sehen (VNC) und
Terminal (SSH) sind Optionen für später, nicht Teil von v1.

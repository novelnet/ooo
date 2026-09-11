# ios – Vom Handy aus

## Claude-App (empfohlen)

Der Server bringt einen MCP-Zugang mit. In der Claude-App heißt das **Connector**.

1. Einstellungen → Connectors → **Add custom connector**
2. Name: `ooo`
3. Adresse: `https://ooo.novelnet.deno.net/mcp/<user-token>`
   Den Schlüssel aus 1Password holen: `op read op://Personal/ooo/tokens/user`
4. Speichern, dann im Chat über das Werkzeug-Symbol aktivieren.

Danach genügt „Weck meinen Mac auf" oder „Ist mein Mac wach?".

Connectors können keine getrennten Kopfzeilen mitschicken, deshalb steht der Schlüssel in der
Adresse. **Behandle diese Adresse wie ein Passwort.** Wer sie kennt, kann den Mac wecken und den
Status abfragen – mehr nicht; anmelden kann sich damit niemand.

## Kurzbefehl (ohne Claude)

1. Kurzbefehle-App → „+" → Name **Mac wecken**
2. Aktion **„Inhalt von URL abrufen"**
   - URL: `https://ooo.novelnet.deno.net/wake`
   - Methode: **POST**
   - Header: `Authorization` = `Bearer <user-token>`
3. Aktion **„Mitteilung anzeigen"** mit dem Ergebnis
4. Zum Home-Bildschirm hinzufügen, oder per Siri: „Hey Siri, Mac wecken"

Ein zweiter Kurzbefehl **Mac Status** genauso mit `GET …/status`.

## Danach weiterarbeiten

Auf dem Mac läuft `claude remote-control`; die Session erscheint in der Claude-App bzw. auf
claude.ai/code. Mehr braucht es für KI-Arbeit vom Handy nicht.

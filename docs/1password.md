# 1Password als Entwickler-Werkzeug

Ziel: von jedem Gerät aus an jedem Projekt arbeiten, ohne Secrets zu kopieren, und ohne
dass ein Agent auf dem Mac an Touch ID scheitert.

## 1. CLI einrichten (einmalig auf dem Mac)

```bash
brew install 1password-cli
# 1Password-App → Einstellungen → Entwickler → „Mit 1Password CLI integrieren“ aktivieren
op whoami        # fragt beim ersten Mal per Touch ID / Passwort
```

## 2. Muster für jedes Projekt: Template + `op inject`

Im Repo liegt nur `*.tpl` mit `op://Vault/Item/Abschnitt/Feld`-Referenzen. Lokal rendert
`op inject` daraus die echte Datei; `.gitignore` sorgt dafür, dass sie nie ins Repo kommt.

```bash
op inject -i .env.tpl -o .env            # einmal rendern
op run --env-file=.env.tpl -- npm run dev # oder: Secrets nur als Prozess-Umgebung, keine Datei
```

Für ooo: `scripts/secrets.sh init|render|push`. Item-Layout:

```
op://Dev/ooo/supabase/project-ref
op://Dev/ooo/tokens/user          (Handy-Kurzbefehl)
op://Dev/ooo/tokens/device        (ESP32)
```

## 3. Headless: Service Account statt Touch ID

Über SSH oder aus einem LaunchAgent gibt es kein Touch ID. Für Agenten, die deployen sollen,
ist ein **1Password Service Account** der richtige Weg:

- 1Password.com → Entwickler → Service Accounts → neu, **nur Lesezugriff auf den Vault `Dev`**
  (nicht auf Private!).
- Token einmalig in den macOS-Schlüsselbund legen, nie in eine Datei:
  ```bash
  security add-generic-password -a "$USER" -s op-service-account -w '<token>' -U
  ```
- In einer SSH-/Remote-Control-Session:
  ```bash
  export OP_SERVICE_ACCOUNT_TOKEN=$(security find-generic-password -a "$USER" -s op-service-account -w)
  op run --env-file=.env.tpl -- vercel deploy --prod
  ```
- Damit funktioniert `op read`, `op inject`, `op run` ohne App, ohne Biometrie.
  Der Schlüsselbund ist nach dem Aufwachen aus dem Schlaf entsperrt (Login-Session läuft weiter);
  nach einem Kaltstart erst nach dem lokalen Login – ein weiterer Grund, den Mac nur schlafen zu lassen.

Rotation: Token im 1Password-Web löschen/neu erzeugen, `security add-generic-password … -U` erneut.

## 4. Git-Signing und SSH ohne Touch ID

- **1Password SSH Agent** statt Secretive: Freigabe geht auch per Systempasswort
  (1Password → Einstellungen → Entwickler → „Freigabe mit Systemauthentifizierung“).
  In einer VNC-Session erscheint der Dialog und lässt sich mit dem Login-Passwort bestätigen.
- Für rein per SSH/Remote Control laufende Agenten: eigenen Deploy-Key ohne Freigabe-Zwang,
  nur für die Repos, die der Agent braucht (GitHub → Deploy keys, write access).
- `sudo`: kein `pam_tid`, Passwort bleibt der Fallback.

## 5. Vom iPhone aus

Die 1Password-App liefert den User-Token für den Kurzbefehl (einmal kopieren, eintragen). Mehr braucht das iPhone nicht.

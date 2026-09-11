#!/bin/bash
# Alle Schlüssel liegen in 1Password: op://Personal/ooo
#
#   scripts/secrets.sh init     Eintrag anlegen (zwei Zufallsschlüssel erzeugen)
#   scripts/secrets.sh render   firmware/include/secrets.h aus der Vorlage schreiben
#   scripts/secrets.sh push     Schlüssel als Umgebungsvariablen zu Deno Deploy schieben
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VAULT="${OOO_VAULT:-Personal}"
ITEM="ooo"
ACCOUNT="${OOO_OP_ACCOUNT:-my.1password.com}"   # es sind mehrere Konten registriert

type -P op >/dev/null || { echo "1Password CLI fehlt:  brew install --cask 1password-cli"; exit 1; }
op() { command op --account "$ACCOUNT" "$@"; }

# Absichtlich nicht "op whoami": das meldet bei App-Integration fälschlich einen Fehler,
# obwohl der Zugriff funktioniert. "op vault list" ist der verlässliche Test.
op vault list >/dev/null 2>&1 || {
  echo "1Password antwortet nicht. App entsperren, dann:  op signin --account $ACCOUNT"
  echo "(Einstellungen → Entwickler → 'Mit 1Password-CLI integrieren' muss an sein.)"
  exit 1
}

read_field() { op read "op://$VAULT/$ITEM/$1"; }

case "${1:-}" in
  init)
    op item get "$ITEM" --vault "$VAULT" >/dev/null 2>&1 && { echo "Eintrag '$ITEM' existiert schon."; exit 1; }
    gen() { openssl rand -hex 32; }
    op item create --vault "$VAULT" --category "Secure Note" --title "$ITEM" --tags ooo,esp32 \
      "tokens.user[password]=$(gen)" \
      "tokens.device[password]=$(gen)" \
      "deno.org[text]=novelnet" \
      "deno.app[text]=ooo" \
      "deno.url[text]=https://ooo.novelnet.deno.net" >/dev/null
    echo "✓ op://$VAULT/$ITEM angelegt (2 Schlüssel erzeugt)."
    echo "  Fehlt noch: deno.deploy-token – unter console.deno.com/account erzeugen und eintragen."
    ;;
  render)
    op inject -f -i "$ROOT/firmware/include/secrets.h.tpl" -o "$ROOT/firmware/include/secrets.h"
    chmod 600 "$ROOT/firmware/include/secrets.h"
    echo "✓ firmware/include/secrets.h gerendert (steht in .gitignore)."
    ;;
  push)
    export DENO_DEPLOY_TOKEN=$(read_field "deno/deploy-token")
    ORG=$(read_field "deno/org"); APP=$(read_field "deno/app")
    deno deploy env add --secret OOO_USER_TOKEN   "$(read_field 'tokens/user')"   --org "$ORG" --app "$APP" --non-interactive >/dev/null
    deno deploy env add --secret OOO_DEVICE_TOKEN "$(read_field 'tokens/device')" --org "$ORG" --app "$APP" --non-interactive >/dev/null
    echo "✓ Schlüssel bei Deno Deploy gesetzt ($ORG/$APP). Danach neu veröffentlichen:"
    echo "  cd server && deno deploy --org $ORG --app $APP --prod"
    ;;
  *)
    sed -n '2,7p' "$0"
    exit 1
    ;;
esac

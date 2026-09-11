#!/bin/bash
# Alle ooo-Secrets leben in 1Password (privates Konto, Vault "Personal", Item "ooo").
#   scripts/secrets.sh init      Item mit frisch generierten Tokens anlegen
#   scripts/secrets.sh render    Template → firmware/include/secrets.h
#   scripts/secrets.sh push      Tokens als Supabase Function Secrets setzen
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VAULT="${OOO_VAULT:-Personal}"; ITEM="ooo"
ACCOUNT="${OOO_OP_ACCOUNT:-my.1password.com}"   # es sind mehrere Konten registriert

type -P op >/dev/null || { echo "1Password CLI fehlt:  brew install --cask 1password-cli"; exit 1; }
op() { command op --account "$ACCOUNT" "$@"; }
# Absichtlich nicht "op whoami": das meldet bei App-Integration faelschlich einen Fehler,
# obwohl der Zugriff funktioniert. "op vault list" ist der verlaessliche Test.
op vault list >/dev/null 2>&1 || {
  echo "1Password antwortet nicht. App entsperren, dann:  op signin --account $ACCOUNT"
  echo "(Einstellungen → Entwickler → 'Mit 1Password-CLI integrieren' muss an sein.)"
  exit 1
}

case "${1:-}" in
  init)
    if op item get "$ITEM" --vault "$VAULT" >/dev/null 2>&1; then echo "Item '$ITEM' existiert schon im Vault '$VAULT'."; exit 1; fi
    read -rp "Supabase Project-Ref (xxxxxxxxxxxxxxxxxxxx): " ref
    gen() { openssl rand -hex 32; }
    op item create --vault "$VAULT" --category "Secure Note" --title "$ITEM" \
      "supabase.project-ref[text]=$ref" \
      "tokens.user[password]=$(gen)" "tokens.device[password]=$(gen)" >/dev/null
    echo "✓ op://$VAULT/$ITEM angelegt (2 Tokens generiert)."
    ;;
  render)
    op inject -f -i "$ROOT/firmware/include/secrets.h.tpl" -o "$ROOT/firmware/include/secrets.h"
    chmod 600 "$ROOT/firmware/include/secrets.h"
    echo "✓ firmware/include/secrets.h gerendert (in .gitignore)."
    ;;
  push)
    cd "$ROOT/api"
    supabase secrets set \
      OOO_USER_TOKEN="$(op read "op://$VAULT/$ITEM/tokens/user")" \
      OOO_DEVICE_TOKEN="$(op read "op://$VAULT/$ITEM/tokens/device")"
    echo "✓ Function Secrets gesetzt."
    ;;
  *) sed -n '2,5p' "$0"; exit 1 ;;
esac

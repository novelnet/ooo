#!/bin/bash
# Alle ooo-Secrets leben in 1Password (Vault "Dev", Item "ooo").
#   scripts/secrets.sh init      Item mit frisch generierten Tokens anlegen (fragt WLAN/Project-Ref ab)
#   scripts/secrets.sh render    Template → firmware/include/secrets.h
#   scripts/secrets.sh push      Tokens als Supabase Function Secrets setzen
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VAULT="${OOO_VAULT:-Dev}"; ITEM="ooo"
command -v op >/dev/null || { echo "1Password CLI fehlt:  brew install 1password-cli  (dann in der 1Password-App: Einstellungen → Entwickler → CLI-Integration)"; exit 1; }

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

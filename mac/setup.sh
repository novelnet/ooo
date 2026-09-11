#!/bin/bash
# ooo einrichten – ein Befehl, danach ist alles fertig.
#
#   bash mac/setup.sh
#
# Der ESP32 muss per USB am Mac stecken. Er sucht selbst nach WLANs, du wählst deins
# aus einer Liste, und der Mac schickt Zugangsdaten, seinen Namen und seine MAC-Adresse
# über das Kabel. Danach braucht der ESP32 nur noch Strom.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

MODE=normal
for a in "$@"; do
  case "$a" in
    --waehlen)     MODE=waehlen ;;   # WLAN von Hand aus der Liste waehlen
    --auto)        MODE=auto ;;      # vom Hintergrunddienst aufgerufen, keine Rueckfragen
    --install-auto) MODE=install ;;  # Hintergrunddienst einrichten
    --remove-auto) MODE=remove ;;
  esac
done

AGENT="$HOME/Library/LaunchAgents/com.ooo.watch.plist"
if [ "$MODE" = install ]; then
  sed "s|__OOO_DIR__|$ROOT/mac|g" "$ROOT/mac/launchd/com.ooo.watch.plist" > "$AGENT"
  launchctl bootout "gui/$(id -u)/com.ooo.watch" 2>/dev/null
  launchctl bootstrap "gui/$(id -u)" "$AGENT" && echo "✅ Automatik aktiv: ESP32 anstecken genügt ab jetzt."
  echo "   Ausschalten mit:  bash mac/setup.sh --remove-auto"
  exit 0
fi
if [ "$MODE" = remove ]; then
  launchctl bootout "gui/$(id -u)/com.ooo.watch" 2>/dev/null
  rm -f "$AGENT" && echo "✅ Automatik ausgeschaltet."
  exit 0
fi

PY=$(command -v python3)
python3 -c "import serial" 2>/dev/null || PY="$HOME/.local/pipx/venvs/platformio/bin/python3"
[ -x "$PY" ] || { echo "❌ Python mit pyserial fehlt. Einmal ausführen:  pipx install platformio"; exit 1; }

# Aktives Interface (fuer Wake-on-LAN) und WLAN-Interface (fuer die Netzliste)
IFACE=$(route -n get default 2>/dev/null | awk '/interface:/{print $2}'); IFACE=${IFACE:-en0}
WIFI_IF=$(networksetup -listallhardwareports | awk '/Hardware Port: Wi-Fi/{getline; print $2}')
WIFI_IF=${WIFI_IF:-en0}

echo "== ESP32 suchen =="
PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1)
[ -n "$PORT" ] || { echo "❌ Kein ESP32 gefunden. Steckt er per USB am Mac? (Datenkabel, kein reines Ladekabel)"; exit 1; }
echo "   gefunden: $PORT"

echo "== WLAN bestimmen =="
# Der Mac verrät nicht, in welchem Netz er steckt (dafür bräuchte das Terminal die
# Berechtigung für Ortungsdienste). Er verrät aber, welche Netze er kennt – und der ESP32
# weiß, welche in Reichweite sind. Die Schnittmenge ist praktisch immer genau das richtige.
"$PY" "$ROOT/mac/provision.py" scan "$PORT" > "$TMP/sichtbar.txt" || { echo "❌ Scan fehlgeschlagen."; exit 1; }
networksetup -listpreferredwirelessnetworks "$WIFI_IF" 2>/dev/null | tail -n +2 | sed $'s/^[ \t]*//' > "$TMP/bekannt.txt"
: > "$TMP/treffer.txt"
while IFS= read -r n; do
  [ -n "$n" ] && grep -Fxq "$n" "$TMP/sichtbar.txt" && echo "$n" >> "$TMP/treffer.txt"
done < "$TMP/bekannt.txt"

SSID=""
if [ "$MODE" != waehlen ]; then
  SSID=$(head -1 "$TMP/treffer.txt")
  if [ -n "$SSID" ]; then
    N=$(wc -l < "$TMP/treffer.txt" | tr -d ' ')
    echo "   automatisch gewählt: $SSID"
    [ "$N" -gt 1 ] && echo "   ($N bekannte Netze in Reichweite, das oberste aus der Mac-Reihenfolge gewinnt; 'bash mac/setup.sh --waehlen' für die Liste)"
  fi
fi

if [ -z "$SSID" ]; then
  echo "   Kein bekanntes Netz in Reichweite – bitte auswählen. Sichtbar sind:"
  nl -w6 -s'  ' "$TMP/sichtbar.txt" | sed 's/^/   /'
  SSID=$(OOO_NETS="$TMP/sichtbar.txt" osascript <<'APPLESCRIPT' 2>/dev/null
set f to POSIX file (system attribute "OOO_NETS")
set t to read f as «class utf8»
set AppleScript's text item delimiters to linefeed
set L to text items of t
if L's last item = "" then set L to items 1 thru -2 of L
tell application "System Events"
    activate
    set c to choose from list L with prompt "Mit welchem WLAN soll sich der ESP32 verbinden?" with title "ooo einrichten"
end tell
if c is false then return ""
return item 1 of c
APPLESCRIPT
)
fi
[ -n "$SSID" ] || { echo "❌ Kein WLAN ausgewählt."; exit 1; }

echo "== Daten vom Mac =="
MAC=$(networksetup -getmacaddress "$IFACE" 2>/dev/null | awk '{print $3}')
HOST=$(scutil --get LocalHostName)
echo "   Mac: $HOST ($IFACE, $MAC)"

echo "== WLAN-Passwort =="
SYS_KC=/Library/Keychains/System.keychain
PASS=$(security find-generic-password -D "AirPort network password" -a "$SSID" -w "$SYS_KC" 2>/dev/null)
[ -n "$PASS" ] || PASS=$(security find-generic-password -s "AirPort" -a "$SSID" -w "$SYS_KC" 2>/dev/null)
if [ -n "$PASS" ]; then
  echo "   aus dem Schlüsselbund gelesen."
else
  echo "   nicht im Schlüsselbund – es öffnet sich ein Eingabefenster."
  PASS=$(OOO_SSID="$SSID" osascript \
    -e 'set s to system attribute "OOO_SSID"' \
    -e 'display dialog "WLAN-Passwort für " & s default answer "" with hidden answer with title "ooo einrichten"' \
    -e 'text returned of result' 2>/dev/null)
fi
[ -n "$PASS" ] || { echo "❌ Ohne Passwort geht es nicht."; exit 1; }

echo "== An den ESP32 senden =="
if "$PY" "$ROOT/mac/provision.py" prov "$PORT" "$SSID" "$PASS" "$HOST" "$MAC"; then
  echo "   ✅ ESP32 ist im WLAN. Die blaue LED leuchtet jetzt dauerhaft."
else
  echo "   ❌ Nicht verbunden. Meist ist das Passwort falsch – einfach nochmal starten."
  exit 1
fi

echo "== Mac fürs Aufwecken vorbereiten =="
if [ "$(pmset -g | awk '/womp/{print $2}')" = "1" ]; then
  echo "   Wake-on-LAN ist schon aktiv."
elif [ "$MODE" = auto ]; then
  echo "   ⚠️  Wake-on-LAN noch nicht aktiv. Einmal 'bash mac/setup.sh' von Hand ausführen."
elif sudo -n pmset -a womp 1 2>/dev/null \
   || osascript -e 'do shell script "pmset -a womp 1" with administrator privileges' >/dev/null 2>&1; then
  echo "   Wake-on-LAN aktiviert."
else
  echo "   ⚠️  Wake-on-LAN nicht gesetzt. Bitte einmal selbst ausführen:  sudo pmset -a womp 1"
fi

echo
echo "Fertig. Der ESP32 braucht ab jetzt nur noch Strom, egal woher."
[ -f "$AGENT" ] || echo "Tipp: 'bash mac/setup.sh --install-auto' – dann reicht künftig Anstecken, ohne Befehl."

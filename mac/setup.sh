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

PY=$(command -v python3)
python3 -c "import serial" 2>/dev/null || PY="$HOME/.local/pipx/venvs/platformio/bin/python3"
[ -x "$PY" ] || { echo "❌ Python mit pyserial fehlt. Einmal ausführen:  pipx install platformio"; exit 1; }

echo "== ESP32 suchen =="
PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1)
[ -n "$PORT" ] || { echo "❌ Kein ESP32 gefunden. Steckt er per USB am Mac? (Datenkabel, kein reines Ladekabel)"; exit 1; }
echo "   gefunden: $PORT"

echo "== WLANs suchen (der ESP32 scannt selbst, dauert ~15 s) =="
"$PY" "$ROOT/mac/provision.py" scan "$PORT" > "$TMP/nets.txt" || { echo "❌ Scan fehlgeschlagen."; exit 1; }
COUNT=$(wc -l < "$TMP/nets.txt" | tr -d ' ')
echo "   $COUNT Netze gefunden (nur 2,4 GHz – mehr kann der ESP32 nicht), stärkstes zuerst:"
nl -w6 -s'  ' "$TMP/nets.txt" | sed 's/^/   /'

# Auswahlfenster. "activate" holt es nach vorn, sonst erscheint es hinter anderen Fenstern.
echo "   → Auswahlfenster geöffnet (ggf. hinter diesem Fenster nachsehen)."
SSID=$(OOO_NETS="$TMP/nets.txt" osascript <<'APPLESCRIPT' 2>/dev/null
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

# Falls das Fenster nicht kam oder abgebrochen wurde: Nummer aus der Liste eintippen.
if [ -z "$SSID" ]; then
  NUM=$(osascript <<'APPLESCRIPT' 2>/dev/null
tell application "System Events"
    activate
    display dialog "Nummer des WLANs aus der Liste im Terminal:" default answer "1" with title "ooo einrichten"
end tell
return text returned of result
APPLESCRIPT
)
  [ -n "$NUM" ] && SSID=$(sed -n "${NUM}p" "$TMP/nets.txt")
fi
[ -n "$SSID" ] || { echo "❌ Kein WLAN ausgewählt."; exit 1; }
echo "   gewählt: $SSID"

echo "== Daten vom Mac =="
IFACE=$(route -n get default 2>/dev/null | awk '/interface:/{print $2}'); IFACE=${IFACE:-en0}
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
if sudo -n pmset -a womp 1 2>/dev/null \
   || osascript -e 'do shell script "pmset -a womp 1" with administrator privileges' >/dev/null 2>&1; then
  echo "   Wake-on-LAN aktiviert."
else
  echo "   ⚠️  Wake-on-LAN nicht gesetzt. Bitte einmal selbst ausführen:  sudo pmset -a womp 1"
fi

echo
echo "Fertig. Der ESP32 braucht ab jetzt nur noch Strom, egal woher."

#!/bin/bash
# ooo einrichten – ein Befehl, danach ist alles fertig.
#
#   bash mac/setup.sh
#
# Der ESP32 muss per USB am Mac stecken. Das Skript liest WLAN-Name, WLAN-Passwort,
# den Namen des Macs und seine MAC-Adresse aus und schickt alles über das Kabel.
# Der ESP32 merkt sich das dauerhaft; danach braucht er nur noch Strom.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

PY=$(command -v python3)
python3 -c "import serial" 2>/dev/null || PY="$HOME/.local/pipx/venvs/platformio/bin/python3"
[ -x "$PY" ] || { echo "❌ Python mit pyserial fehlt. Einmal ausführen:  pipx install platformio"; exit 1; }

echo "== ESP32 suchen =="
PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1)
[ -n "$PORT" ] || { echo "❌ Kein ESP32 gefunden. Steckt er per USB am Mac? (Datenkabel, kein reines Ladekabel)"; exit 1; }
echo "   gefunden: $PORT"

echo "== Daten vom Mac sammeln =="
IFACE=$(route -n get default 2>/dev/null | awk '/interface:/{print $2}')
IFACE=${IFACE:-en0}
SSID=$(ipconfig getsummary "$IFACE" 2>/dev/null | awk -F ' SSID : ' '/ SSID : / {print $2}' | head -1)
[ -n "$SSID" ] || { echo "❌ Der Mac hängt gerade in keinem WLAN. Erst verbinden, dann nochmal starten."; exit 1; }
MAC=$(networksetup -getmacaddress "$IFACE" 2>/dev/null | awk '{print $3}')
HOST=$(scutil --get LocalHostName)
echo "   WLAN     : $SSID"
echo "   Mac      : $HOST ($IFACE, $MAC)"

echo "== WLAN-Passwort besorgen =="
# WLAN-Passwoerter liegen im System-Schluesselbund. Beim Lesen zeigt macOS einen
# Freigabe-Dialog (Touch ID oder Passwort) – das braucht kein sudo und kein Terminal.
SYS_KC=/Library/Keychains/System.keychain
PASS=$(security find-generic-password -D "AirPort network password" -a "$SSID" -w "$SYS_KC" 2>/dev/null)
[ -n "$PASS" ] || PASS=$(security find-generic-password -s "AirPort" -a "$SSID" -w "$SYS_KC" 2>/dev/null)
[ -n "$PASS" ] || PASS=$(security find-generic-password -a "$SSID" -w "$SYS_KC" 2>/dev/null)

if [ -n "$PASS" ]; then
  echo "   aus dem Schlüsselbund gelesen."
else
  # Fallback: Eingabefenster von macOS. Funktioniert auch ohne Terminal-Eingabe.
  echo "   Schlüsselbund hat nichts geliefert – es öffnet sich ein Eingabefenster."
  SSID_ESC=${SSID//\\/\\\\}; SSID_ESC=${SSID_ESC//\"/\\\"}
  PASS=$(osascript \
    -e "display dialog \"WLAN-Passwort für $SSID_ESC\" default answer \"\" with hidden answer with title \"ooo einrichten\"" \
    -e "text returned of result" 2>/dev/null)
fi
[ -n "$PASS" ] || { echo "❌ Ohne Passwort geht es nicht."; exit 1; }

echo "== An den ESP32 senden =="
if "$PY" "$ROOT/mac/provision.py" "$PORT" "$SSID" "$PASS" "$HOST" "$MAC"; then
  echo "   ✅ ESP32 ist im WLAN. Die blaue LED leuchtet jetzt dauerhaft."
else
  echo "   ❌ Hat nicht geklappt. Blaue LED aus oder Doppelblitz = nicht verbunden."
  echo "      Passwort falsch? Dann einfach nochmal starten."
  exit 1
fi

echo "== Mac fürs Aufwecken vorbereiten =="
# Auch hier per macOS-Dialog, damit es ohne Terminal-Eingabe funktioniert.
if sudo -n pmset -a womp 1 2>/dev/null; then
  echo "   Wake-on-LAN aktiviert."
elif osascript -e 'do shell script "pmset -a womp 1" with administrator privileges' >/dev/null 2>&1; then
  echo "   Wake-on-LAN aktiviert."
else
  echo "   ⚠️  Wake-on-LAN nicht gesetzt. Bitte einmal selbst ausführen:  sudo pmset -a womp 1"
fi

echo
echo "Fertig. Der ESP32 braucht ab jetzt nur noch Strom, egal woher."

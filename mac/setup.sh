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

echo "== WLAN-Passwort aus dem Schlüsselbund =="
echo "   macOS fragt gleich nach deinem Passwort – das Passwort geht nur über das USB-Kabel zum ESP32."
PASS=$(sudo security find-generic-password -wa "$SSID" /Library/Keychains/System.keychain 2>/dev/null)
[ -n "$PASS" ] || { echo "❌ Passwort für \"$SSID\" nicht gefunden."; exit 1; }
echo "   ok"

echo "== An den ESP32 senden =="
if "$PY" "$ROOT/mac/provision.py" "$PORT" "$SSID" "$PASS" "$HOST" "$MAC"; then
  echo "   ✅ ESP32 ist im WLAN. Die LED leuchtet jetzt dauerhaft."
else
  echo "   ❌ Hat nicht geklappt. LED blinkt langsam = wartet weiter. Nochmal starten oder Kabel prüfen."
  exit 1
fi

echo "== Mac fürs Aufwecken vorbereiten =="
sudo pmset -a womp 1 && echo "   Wake-on-LAN aktiviert."

echo
echo "Fertig. Der ESP32 braucht ab jetzt nur noch Strom, egal woher."

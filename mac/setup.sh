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
PASS=""
# 1) Schlüsselbund. macOS zeigt dabei einen Freigabe-Dialog (Passwort oder Touch ID).
for query in "-D AirPort network password -a $SSID" "-s AirPort -a $SSID" "-a $SSID"; do
  # shellcheck disable=SC2086
  OUT=$(security find-generic-password $query -w 2>&1)
  if [ $? -eq 0 ] && [ -n "$OUT" ]; then PASS="$OUT"; echo "   aus dem Schlüsselbund gelesen."; break; fi
done
# 2) System-Schlüsselbund mit sudo, falls sudo gerade ohne Nachfrage darf.
if [ -z "$PASS" ]; then
  OUT=$(sudo -n security find-generic-password -a "$SSID" -w /Library/Keychains/System.keychain 2>/dev/null)
  [ -n "$OUT" ] && { PASS="$OUT"; echo "   aus dem System-Schlüsselbund gelesen."; }
fi
# 3) Von Hand. Funktioniert immer.
if [ -z "$PASS" ]; then
  echo "   Schlüsselbund hat nichts geliefert (kein Eintrag oder Freigabe abgelehnt)."
  read -rsp "   WLAN-Passwort für \"$SSID\" eingeben: " PASS
  echo
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
if sudo -n pmset -a womp 1 2>/dev/null; then
  echo "   Wake-on-LAN aktiviert."
elif sudo pmset -a womp 1 2>/dev/null; then
  echo "   Wake-on-LAN aktiviert."
else
  echo "   ⚠️  Konnte Wake-on-LAN nicht setzen (sudo brauchte eine Eingabe)."
  echo "      Bitte einmal selbst ausführen:  sudo pmset -a womp 1"
fi

echo
echo "Fertig. Der ESP32 braucht ab jetzt nur noch Strom, egal woher."

#!/bin/bash
# Hintergrunddienst: hält den ESP32 auf dem aktuellen WLAN-Stand.
#
# Läuft alle 10 Sekunden und richtet neu ein, wenn sich etwas geändert hat:
#   - der ESP32 wurde neu angesteckt
#   - der Mac hängt in einem anderen Netz (erkannt am Router, nicht am WLAN-Namen –
#     den gibt macOS ohne Ortungsdienste-Berechtigung nicht heraus)
#
# Dadurch reicht es, den ESP32 dauerhaft am USB-Anschluss des MacBooks zu lassen:
# Er reist mit, bleibt im Schlaf mit Strom versorgt und kennt immer das aktuelle Netz.
set -uo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
STATE="$HOME/Library/Application Support/ooo/zustand"
mkdir -p "$(dirname "$STATE")"

PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
  rm -f "$STATE"            # abgezogen – beim nächsten Anstecken neu einrichten
  exit 0
fi

# Fingerabdruck des Netzes: Standard-Gateway plus dessen MAC-Adresse. Ändert sich beides
# nicht, ist es dasselbe Netz. Ohne WLAN-Verbindung bleibt der Fingerabdruck leer.
GW=$(route -n get default 2>/dev/null | awk '/gateway:/{print $2}')
[ -n "$GW" ] || exit 0
GWMAC=$(arp -n "$GW" 2>/dev/null | awk '{print $4}')
# Die eigene Adresse gehoert dazu: macOS wechselt die private WLAN-Adresse gelegentlich,
# und dann muss der ESP32 die neue erfahren, sonst weckt sein Paket niemanden mehr.
SELF=$(ifconfig 2>/dev/null | awk '/^en0:/{f=1} f&&/ether/{print $2; exit}')
FP="$PORT|$GW|$GWMAC|$SELF"

LAST=$(cat "$STATE" 2>/dev/null || true)
[ "$FP" = "$LAST" ] && exit 0

echo "$FP" > "$STATE"
echo "$(date '+%F %T') Änderung erkannt ($FP) – Einrichtung startet"
exec bash "$DIR/setup.sh" --auto

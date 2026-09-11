#!/bin/bash
# Läuft als Hintergrunddienst und richtet den ESP32 ein, sobald er angesteckt wird.
# Reagiert nur auf neu erschienene Geräte, damit ein steckender ESP32 nicht dauernd
# neu gestartet wird (das Öffnen der seriellen Schnittstelle löst einen Reset aus).
set -uo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
STATE="$HOME/Library/Application Support/ooo/letztes-geraet"
mkdir -p "$(dirname "$STATE")"

PORT=$(ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.usbmodem* 2>/dev/null | head -1)
LAST=$(cat "$STATE" 2>/dev/null || true)

if [ -z "$PORT" ]; then
  rm -f "$STATE"          # abgezogen – beim nächsten Anstecken wieder einrichten
  exit 0
fi
[ "$PORT" = "$LAST" ] && exit 0

echo "$PORT" > "$STATE"
echo "$(date '+%F %T') ESP32 angesteckt an $PORT – Einrichtung startet" 
exec bash "$DIR/setup.sh" --auto

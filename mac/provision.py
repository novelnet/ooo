#!/usr/bin/env python3
"""Schickt WLAN-Zugangsdaten, Hostnamen und MAC-Adresse über USB an den ESP32.
Wird von mac/setup.sh aufgerufen, nicht direkt."""
import base64, sys, time
import serial

port, ssid, password, host, mac = sys.argv[1:6]
b64 = lambda s: base64.b64encode(s.encode()).decode()
line = f"PROV {b64(ssid)} {b64(password)} {b64(host)} {mac}\n"

with serial.Serial(port, 115200, timeout=1) as s:
    s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)   # Reset
    time.sleep(1.5)
    s.reset_input_buffer()
    s.write(line.encode())
    s.flush()

    ok_prov = ok_wifi = False
    end = time.time() + 45
    while time.time() < end:
        raw = s.readline().decode("utf-8", "replace").strip()
        if not raw:
            continue
        if raw.startswith(("PROV ", "WIFI ", "WARTE", "[wifi]", "[prefs]")):
            print("   " + raw)
        if raw.startswith("PROV OK"):
            ok_prov = True
        if raw.startswith("WIFI OK"):
            ok_wifi = True
            break
        if raw.startswith(("PROV FEHLER", "WIFI FEHLER")):
            break

if ok_wifi:
    sys.exit(0)
print("   (keine Bestaetigung vom ESP32)" if not ok_prov else "", file=sys.stderr)
sys.exit(1)

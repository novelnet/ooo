#!/usr/bin/env python3
"""Redet mit dem ESP32 über USB. Wird von mac/setup.sh aufgerufen.

  provision.py scan <port>                            → sichtbare Netze, stärkstes zuerst
  provision.py prov <port> <ssid> <pass> <host> <mac> → einrichten
"""
import base64, sys, time
import serial


def open_port(port, reset=True):
    s = serial.Serial(port, 115200, timeout=1)
    if reset:
        s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
        time.sleep(2.0)
    s.reset_input_buffer()
    return s


def scan(port):
    with open_port(port) as s:
        s.write(b"SCAN\n"); s.flush()
        nets = {}
        end = time.time() + 30
        while time.time() < end:
            raw = s.readline().decode("utf-8", "replace").strip()
            if raw.startswith("SCANNET"):
                parts = raw.split(None, 4)
                if len(parts) < 5:
                    continue
                rssi, ssid = int(parts[1]), parts[4]
                if ssid and rssi > nets.get(ssid, (-999,))[0]:
                    nets[ssid] = (rssi, parts[3])
            elif raw.startswith("SCANEND"):
                break
    if not nets:
        print("Kein Netz gefunden.", file=sys.stderr)
        return 1
    for ssid, (rssi, enc) in sorted(nets.items(), key=lambda kv: -kv[1][0]):
        print(ssid)
    return 0


def prov(port, ssid, password, host, mac):
    b64 = lambda s: base64.b64encode(s.encode()).decode()
    line = f"PROV {b64(ssid)} {b64(password)} {b64(host)} {mac}\n"
    ok = False
    with open_port(port) as s:
        s.write(line.encode()); s.flush()
        end = time.time() + 120
        while time.time() < end:
            raw = s.readline().decode("utf-8", "replace").strip()
            if not raw:
                continue
            if raw.startswith(("PROV ", "WIFI ", "[wifi] Versuch")):
                print("   " + raw)
            if raw.startswith("WIFI OK"):
                ok = True
                break
            if raw.startswith(("PROV FEHLER", "WIFI FEHLER")):
                break
    return 0 if ok else 1


if __name__ == "__main__":
    mode = sys.argv[1]
    sys.exit(scan(sys.argv[2]) if mode == "scan" else prov(*sys.argv[2:7]))

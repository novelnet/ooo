#!/usr/bin/env python3
"""Redet mit dem ESP32 über USB. Wird von mac/setup.sh aufgerufen.

  provision.py scan <port>              → sichtbare Netze, stärkstes zuerst
  provision.py prov <port> <json-datei> → Netze und Mac-Name übertragen, dann verbinden

Die JSON-Datei enthält {"host": "...", "nets": [{"ssid": "...", "pass": "..."}, ...]}.
Passwörter laufen bewusst über eine Datei mit Rechten 600 und nicht über die Kommandozeile,
damit sie nicht in der Prozessliste auftauchen.
"""
import base64, json, sys, time
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
                if ssid and rssi > nets.get(ssid, -999):
                    nets[ssid] = rssi
            elif raw.startswith("SCANEND"):
                break
    if not nets:
        print("Kein Netz gefunden.", file=sys.stderr)
        return 1
    for ssid in sorted(nets, key=lambda k: -nets[k]):
        print(ssid)
    return 0


def prov(port, cfg_path):
    cfg = json.load(open(cfg_path))
    b64 = lambda s: base64.b64encode(s.encode()).decode()
    host = cfg.get("host", "")
    ok_wifi = False

    with open_port(port) as s:
        for net in cfg["nets"]:
            s.write(f"PROV {b64(net['ssid'])} {b64(net['pass'])} {b64(host)}\n".encode())
            s.flush()
            time.sleep(0.4)
        s.write(b"CONNECT\n"); s.flush()

        end = time.time() + 120
        while time.time() < end:
            raw = s.readline().decode("utf-8", "replace").strip()
            if not raw:
                continue
            if raw.startswith(("PROV ", "WIFI ", "[wifi] verbinde")):
                print("   " + raw)
            if raw.startswith("WIFI OK"):
                ok_wifi = True
                break
            if raw.startswith("WIFI FEHLER"):
                break
    return 0 if ok_wifi else 1


if __name__ == "__main__":
    mode = sys.argv[1]
    sys.exit(scan(sys.argv[2]) if mode == "scan" else prov(sys.argv[2], sys.argv[3]))

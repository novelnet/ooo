#!/bin/bash
# Einmalig: Wake-on-LAN erlauben. Mehr braucht der Mac nicht.
sudo pmset -a womp 1      # "Wake for network access"
echo "ok – Hostname für das ESP32-Setup-Portal: $(hostname -s)"
echo "Hinweis: WoL über WLAN weckt Apple-Macs nur mit Apple TV/HomePod im Netz; sonst USB-C-Ethernet-Adapter."

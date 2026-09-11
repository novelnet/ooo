# Planungsnotizen (Stand vor diesem Repo)

Zusammenfassung des Gesprächs, das zu `ooo` geführt hat – als Kontext für spätere Arbeit.

## Ausgangslage
- MacBook Pro (M1 Pro, macOS 26), FileVault an, meist zugeklappt im Standby, nur WLAN.
- Ziel: vom iPhone (bald Fold) unterwegs KI-Sessions bedienen, gelegentlich den Bildschirm sehen,
  Commits/Freigaben machen. Tailscale ist installiert, Router ist ein Mercusys.
- Erste Idee war „Mac aus der Ferne einschalten“.

## Erkenntnisse
1. **Kaltstart aus der Ferne gibt es bei Apple Silicon nicht** – kein WoL aus „aus“, kein Netzboot.
   Einzige Ausnahme: Strom anschließen bootet (Standard-`BootPreference`). Mit FileVault endet
   das aber am Pre-Boot-Login → Lösung: nie ausschalten, nur schlafen lassen.
2. **Zugeklappt wach bleiben** braucht Netzteil + externen Monitor oder HDMI-Dummy;
   `pmset -c sleep 0`. `disablesleep 1` würde auch den Deckel ignorieren – dann aber nie in
   die Tasche stecken.
3. **Steuern vom iPhone:** SSH via Tailscale (Blink/Termius, tmux), Bildschirm via VNC
   (Screens 5 / Jump Desktop). Claude Code Remote Control ersetzt für KI-Arbeit das meiste davon.
4. **Touch ID remote gibt es nicht.** Signing-Keys so einrichten, dass sie ohne Biometrie
   freigegeben werden können (1Password SSH Agent mit Passwort-Freigabe, kein pam_tid für sudo).
5. Schlafen erzwingen: `pmset sleepnow`; wer wach hält: `pmset -g assertions`.

## Namensfindung
Kandidaten: leash, relay, tether, tobiday, strandbüro, urlaubsvertretung, fernbedienung.
Gewählt: **ooo** – Out Of Office. Kürzester CLI-Name, international verständlich.

## Ursprünglich geplante Struktur
`firmware/` (ESP32) · `api/` (Supabase Edge Functions) · `mac/` (launchd, pmset) · `ios/` · `docs/`
– so umgesetzt, ergänzt um `cli/` und `scripts/`.

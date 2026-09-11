# mac

```bash
bash mac/setup.sh                  # ESP32 anstecken, dann dieser eine Befehl
bash mac/setup.sh --install-auto   # danach reicht Anstecken, ganz ohne Befehl
```

## Was passiert

1. Der ESP32 scannt, welche WLANs in Reichweite sind (nur 2,4 GHz, mehr kann er nicht).
2. Der Mac listet die WLANs auf, die er kennt. Die Schnittmenge ist praktisch immer genau
   ein Netz – das wird automatisch genommen. Mit `--waehlen` erscheint stattdessen eine Liste.
3. Das Passwort kommt aus dem Schlüsselbund, sonst fragt ein Fenster danach.
4. Name und MAC-Adresse des Macs gehen mit über das Kabel.
5. Wake-on-LAN wird aktiviert (`pmset womp 1`), falls noch nicht geschehen.

Klappt es, leuchtet die blaue LED am ESP32 dauerhaft. Die rote daneben ist die
Betriebsanzeige (PWR) und leuchtet immer.

## Warum der Mac nicht einfach sein eigenes WLAN verrät

macOS gibt den Namen des verbundenen Netzes nur an Programme heraus, die die Berechtigung für
Ortungsdienste haben; sonst liefert es wörtlich `<redacted>`. Die Liste der **bekannten** Netze
ist dagegen frei lesbar. Deshalb der Umweg über die Schnittmenge – keine Berechtigung nötig,
und es erscheinen nur Netze, die der ESP32 auch wirklich erreicht.

## Automatik

`--install-auto` richtet einen Hintergrunddienst ein (`com.ooo.watch`), der alle 10 Sekunden
nachsieht, ob ein ESP32 neu angesteckt wurde, und dann die Einrichtung startet. Er reagiert nur
auf neu erschienene Geräte, damit ein dauerhaft steckender ESP32 nicht ständig neu startet.
Protokoll: `/tmp/ooo-watch.log`. Ausschalten mit `--remove-auto`.

## Voraussetzungen für Wake-on-LAN bei Apple

- Netzteil angeschlossen.
- Ethernet (USB-C-Adapter) **oder** im WLAN ein Apple TV/HomePod (Bonjour Sleep Proxy).
  Reines WLAN ohne Proxy wacht nicht auf.
- FileVault ist an → nie ausschalten, nur schlafen lassen.

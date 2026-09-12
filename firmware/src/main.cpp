// ooo – ESP32 Wake-Trigger
//
// Einrichten: ESP32 per USB an den Mac stecken und dort `bash mac/setup.sh` laufen lassen.
// Der Mac schickt alle WLANs, die er kennt und die der ESP32 sieht, dazu seinen Namen.
// Der ESP32 merkt sich bis zu MAX_NETS Netze und nimmt beim Start das staerkste bekannte.
// Damit funktioniert er zu Hause, im Buero und am iPhone-Hotspot, ohne erneutes Einrichten.
//
// Im Betrieb: wartet an der ooo-Cloud auf Befehle (TLS mit gepinnter Root-CA) und weckt den
// Mac per Wake-on-LAN. Bei jedem Durchgang pingt er den Mac und meldet das Ergebnis mit.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <time.h>
#include "lwip/etharp.h"
#include "lwip/netif.h"

#include "config.h"
#include "secrets.h"
#include "isrg_roots.h"

static Preferences g_prefs;

// Mehrere bekannte Netze. Die MAC-Adresse des Macs wird je Netz gemerkt, weil macOS
// pro WLAN eine andere private Adresse benutzt – und sie zusaetzlich rotieren lassen kann.
// Deshalb je Netz die letzten MAX_MACS Adressen behalten und beim Wecken alle ansprechen.
struct Net {
  String  ssid;
  String  pass;
  uint8_t macs[MAX_MACS][6];
  int     macCount;
};
static Net      g_nets[MAX_NETS];
static int      g_netCount = 0;
static int      g_current = -1;          // Index des verbundenen Netzes
static String   g_host;                  // Name des Macs, fuer mDNS
static bool     g_macReachable = false;
static bool     g_powered = true;
static uint32_t g_lastOkMs = 0;
static uint32_t g_backoffMs = BACKOFF_MIN_MS;
static uint8_t  g_lastDisconnectReason = 0;

// Client und Verbindung bleiben bestehen. Der Aufbau einer verschluesselten Verbindung ist
// der mit Abstand teuerste Teil – auf dem ESP32 (Rechenzeit, also Strom) und beim Server.
// Bei rund 3.500 Abfragen am Tag lohnt sich das Offenhalten deutlich.
static WiFiClientSecure g_tls;
static bool             g_tlsReady = false;

// ---------------------------------------------------------------------------
// LED:  aus = nicht bereit · blinkt = arbeitet · an = bereit · Doppelblitz = Fehler
// (Die rote LED des DevKits haengt fest am Strom und ist nicht schaltbar.)
// ---------------------------------------------------------------------------
enum LedMode { LED_IDLE, LED_WORKING, LED_READY, LED_ERROR };
static LedMode g_led = LED_IDLE;
static void ledMode(LedMode m) { g_led = m; }
static void ledTick() {
  bool on = false;
  switch (g_led) {
    case LED_IDLE:    on = false; break;
    case LED_READY:   on = true;  break;
    case LED_WORKING: on = (millis() % 400) < 200; break;
    case LED_ERROR: {
      uint32_t t = millis() % 1500;
      on = (t < 120) || (t >= 300 && t < 420);
      break;
    }
  }
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

static void relaySet(bool powered) {
  g_powered = powered;
  if (!RELAY_ENABLED) return;
  digitalWrite(RELAY_PIN, (powered ? !RELAY_ACTIVE_LOW : RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
static String macToString(const uint8_t m[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

static bool parseMac(const String& s, uint8_t out[6]) {
  unsigned v[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}

static String b64decode(const String& in) {
  size_t len = 0;
  unsigned char buf[256];
  if (mbedtls_base64_decode(buf, sizeof(buf) - 1, &len, (const unsigned char*)in.c_str(), in.length()) != 0) return "";
  buf[len] = 0;
  return String((char*)buf);
}

// ---------------------------------------------------------------------------
// Gespeicherte Netze
// ---------------------------------------------------------------------------
static String keyOf(int i, const char* suffix) { return String("n") + i + suffix; }

static void loadPrefs() {
  g_prefs.begin("ooo", false);
  g_host = g_prefs.isKey("host") ? g_prefs.getString("host", "") : "";
  g_netCount = g_prefs.isKey("count") ? g_prefs.getInt("count", 0) : 0;
  if (g_netCount > MAX_NETS) g_netCount = MAX_NETS;
  for (int i = 0; i < g_netCount; i++) {
    g_nets[i].ssid = g_prefs.getString(keyOf(i, "s").c_str(), "");
    g_nets[i].pass = g_prefs.getString(keyOf(i, "p").c_str(), "");
    g_nets[i].macCount = 0;
    if (g_prefs.isKey(keyOf(i, "m").c_str())) {
      uint8_t buf[MAX_MACS * 6];
      size_t got = g_prefs.getBytes(keyOf(i, "m").c_str(), buf, sizeof(buf));
      g_nets[i].macCount = got / 6;
      if (g_nets[i].macCount > MAX_MACS) g_nets[i].macCount = MAX_MACS;
      memcpy(g_nets[i].macs, buf, g_nets[i].macCount * 6);
    }
  }
}

static void saveNets() {
  g_prefs.putInt("count", g_netCount);
  for (int i = 0; i < g_netCount; i++) {
    g_prefs.putString(keyOf(i, "s").c_str(), g_nets[i].ssid);
    g_prefs.putString(keyOf(i, "p").c_str(), g_nets[i].pass);
    if (g_nets[i].macCount > 0)
      g_prefs.putBytes(keyOf(i, "m").c_str(), g_nets[i].macs, g_nets[i].macCount * 6);
  }
}

// Neues Netz nach vorn, vorhandenes aktualisieren. Aeltestes faellt raus.
static int addNet(const String& ssid, const String& pass) {
  int found = -1;
  for (int i = 0; i < g_netCount; i++)
    if (g_nets[i].ssid == ssid) { found = i; break; }

  Net entry;
  if (found >= 0) {
    entry = g_nets[found];
    entry.pass = pass;
    for (int i = found; i > 0; i--) g_nets[i] = g_nets[i - 1];
  } else {
    entry.ssid = ssid;
    entry.pass = pass;
    entry.macCount = 0;
    if (g_netCount < MAX_NETS) g_netCount++;
    for (int i = g_netCount - 1; i > 0; i--) g_nets[i] = g_nets[i - 1];
  }
  g_nets[0] = entry;
  saveNets();
  return 0;
}

// Neu gesehene Adresse nach vorn. Aeltere bleiben erhalten, damit ein Wecken auch dann
// klappt, wenn macOS die private Adresse zwischenzeitlich gewechselt hat.
static void addMacTo(int idx, const uint8_t m[6]) {
  if (idx < 0 || idx >= g_netCount) return;
  Net& n = g_nets[idx];
  if (n.macCount > 0 && memcmp(m, n.macs[0], 6) == 0) return;   // unveraendert

  int found = -1;
  for (int i = 0; i < n.macCount; i++)
    if (memcmp(m, n.macs[i], 6) == 0) { found = i; break; }

  int upto = (found >= 0) ? found : (n.macCount < MAX_MACS ? n.macCount++ : MAX_MACS - 1);
  for (int i = upto; i > 0; i--) memcpy(n.macs[i], n.macs[i - 1], 6);
  memcpy(n.macs[0], m, 6);
  g_prefs.putBytes(keyOf(idx, "m").c_str(), n.macs, n.macCount * 6);
  Serial.printf("[prefs] MAC-Adresse fuer \"%s\": %s (%d gespeichert)\n",
                n.ssid.c_str(), macToString(m).c_str(), n.macCount);
}

static void rememberMac(const uint8_t m[6]) { addMacTo(g_current, m); }

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------
static const char* encName(wifi_auth_mode_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN: return "offen";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "andere";
  }
}

// Liefert die Anzahl sichtbarer Netze; bei quiet=false zusaetzlich als Liste ueber USB.
static int scanNetworks(bool quiet) {
  bool wasConnected = WiFi.status() == WL_CONNECTED;
  if (!wasConnected) { WiFi.mode(WIFI_STA); WiFi.disconnect(false); delay(100); }
  int n = WiFi.scanNetworks();
  if (n < 0) { delay(500); n = WiFi.scanNetworks(); }
  if (!quiet) {
    for (int i = 0; i < n; i++)
      Serial.printf("SCANNET %d %d %s %s\n", WiFi.RSSI(i), WiFi.channel(i),
                    encName(WiFi.encryptionType(i)), WiFi.SSID(i).c_str());
    Serial.printf("SCANEND %d\n", n);
  }
  return n;
}

// ---------------------------------------------------------------------------
// Verbinden: staerkstes sichtbares Netz nehmen, das wir kennen
// ---------------------------------------------------------------------------
static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    g_lastDisconnectReason = info.wifi_sta_disconnected.reason;
}

static const char* wifiReasonText(uint8_t r) {
  switch (r) {
    case 2: case 15: case 204: case 205: return "Passwort falsch oder Handshake abgelehnt";
    case 201: return "Netz nicht gefunden – ESP32 kann nur 2,4 GHz, nicht 5 GHz";
    case 202: return "Authentifizierung fehlgeschlagen";
    case 203: return "Access Point hat abgelehnt";
    case 3: case 4: return "Verbindung vom Router beendet";
    default: return "unbekannt";
  }
}

static void handleSerial();
static void waitTicking(uint32_t ms) {
  uint32_t end = millis() + ms;
  while (millis() < end) { handleSerial(); ledTick(); delay(10); }
}

static bool tryNet(int idx) {
  Serial.printf("[wifi] verbinde mit \"%s\" …\n", g_nets[idx].ssid.c_str());
  WiFi.disconnect(true);
  waitTicking(300);
  WiFi.begin(g_nets[idx].ssid.c_str(), g_nets[idx].pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TRY_MS) waitTicking(50);
  if (WiFi.status() == WL_CONNECTED) {
    g_current = idx;
    ledMode(LED_READY);
    Serial.printf("WIFI OK %s ip=%s rssi=%d\n", g_nets[idx].ssid.c_str(),
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    MDNS.begin("ooo-esp");
    return true;
  }
  Serial.printf("[wifi] \"%s\" fehlgeschlagen, grund=%u (%s)\n", g_nets[idx].ssid.c_str(),
                g_lastDisconnectReason, wifiReasonText(g_lastDisconnectReason));
  return false;
}

static bool connectWifi() {
  if (g_netCount == 0) return false;
  ledMode(LED_WORKING);
  WiFi.mode(WIFI_STA);
  // Stromsparmodus des Funkmoduls an: halbiert den Ruheverbrauch. Kostet beim Empfang
  // hoechstens ein DTIM-Intervall (~100-300 ms) – bei unserem Wartemuster ohne Bedeutung.
  WiFi.setSleep(true);
  WiFi.setHostname("ooo-esp");
  g_current = -1;

  // Der Scan liefert die Reihenfolge: das staerkste bekannte Netz zuerst.
  int n = scanNetworks(true);
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < g_netCount; k++) {
      if (WiFi.SSID(i) != g_nets[k].ssid) continue;
      WiFi.scanDelete();
      if (tryNet(k)) return true;
      n = scanNetworks(true);          // nach einem Fehlversuch neu schauen
      i = -1;                          // und von vorn, aber dieses Netz ueberspringen
      g_nets[k].ssid += "\x01";        // Marker: in diesem Durchlauf schon probiert
      break;
    }
  }
  WiFi.scanDelete();
  // Marker wieder entfernen
  for (int k = 0; k < g_netCount; k++)
    if (g_nets[k].ssid.endsWith("\x01")) g_nets[k].ssid.remove(g_nets[k].ssid.length() - 1);

  Serial.printf("WIFI FEHLER kein bekanntes Netz erreichbar (%d bekannt, %d sichtbar)\n", g_netCount, n);
  ledMode(LED_ERROR);
  return false;
}

// Ohne bekannte Netze: LED aus, auf Einrichtung ueber USB warten.
static void waitForSetup() {
  Serial.println("WARTE AUF EINRICHTUNG – ESP32 per USB an den Mac, dort: bash mac/setup.sh");
  ledMode(LED_IDLE);
  while (g_netCount == 0) waitTicking(100);
}

static void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) { if (g_led == LED_ERROR) ledMode(LED_READY); return; }
  Serial.println("[wifi] Verbindung weg – neu verbinden");
  g_tls.stop();                          // alte Verbindung gehoert zum alten Netz
  if (!connectWifi()) {
    if (g_netCount == 0) { waitForSetup(); connectWifi(); }
    else waitTicking(10000);
  }
}

static void ensureTime() {
  time_t now = time(nullptr);
  if (now > 1700000000) return;
  configTime(0, 0, "pool.ntp.org", "time.apple.com");
  uint32_t start = millis();
  while ((now = time(nullptr)) < 1700000000 && millis() - start < 20000) waitTicking(100);
}

// ---------------------------------------------------------------------------
// Mac im LAN: mDNS → IP → Ping. Antwortet er, MAC-Adresse aus der ARP-Tabelle auffrischen.
// ---------------------------------------------------------------------------
static bool macReachable() {
  if (g_host.isEmpty() || WiFi.status() != WL_CONNECTED) return false;
  IPAddress ip = MDNS.queryHost(g_host, 1500);
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  if (!Ping.ping(ip, 1)) return false;
  ip4_addr_t ip4;
  ip4.addr = (uint32_t)ip;
  struct eth_addr* eth = nullptr;
  const ip4_addr_t* found = nullptr;
  if (etharp_find_addr(netif_default, &ip4, &eth, &found) >= 0 && eth) rememberMac(eth->addr);
  return true;
}

// Schickt an alle fuer dieses Netz bekannten Adressen, weil macOS die private
// WLAN-Adresse wechseln kann und dann nur noch eine aeltere passt.
static bool sendWol() {
  if (g_current < 0 || g_nets[g_current].macCount == 0) {
    Serial.println("[wol] fuer dieses Netz ist noch keine MAC-Adresse bekannt");
    return false;
  }
  Net& n = g_nets[g_current];
  WiFiUDP udp;
  udp.begin(0);
  for (int r = 0; r < WOL_BURST; r++) {
    for (int k = 0; k < n.macCount; k++) {
      uint8_t pkt[102];
      memset(pkt, 0xFF, 6);
      for (int i = 1; i <= 16; i++) memcpy(pkt + i * 6, n.macs[k], 6);
      udp.beginPacket(IPAddress(255, 255, 255, 255), 9);
      udp.write(pkt, sizeof(pkt));
      udp.endPacket();
    }
    waitTicking(300);
  }
  udp.stop();
  Serial.printf("[wol] %d Runden an %d Adresse(n), neueste %s\n", WOL_BURST, n.macCount, macToString(n.macs[0]).c_str());
  return true;
}

// ---------------------------------------------------------------------------
// Einrichtung ueber USB
//   PROV <b64 ssid> <b64 pass> <b64 host>   Netz hinzufuegen (Host optional leer)
//   CONNECT                                  jetzt verbinden
//   STATUS / SCAN / RESET
// ---------------------------------------------------------------------------
static bool g_reconnectRequested = false;

static void printStatus() {
  Serial.printf("STATUS fw=%s verbunden=%s ssid=%s ip=%s host=%s mac_erreichbar=%s netze=%d\n",
                FW_VERSION, WiFi.status() == WL_CONNECTED ? "ja" : "nein",
                g_current >= 0 ? g_nets[g_current].ssid.c_str() : "-",
                WiFi.localIP().toString().c_str(), g_host.c_str(),
                g_macReachable ? "ja" : "nein", g_netCount);
  for (int i = 0; i < g_netCount; i++)
    Serial.printf("STATUSNET %d %s macs=%d neueste=%s\n", i + 1, g_nets[i].ssid.c_str(),
                  g_nets[i].macCount,
                  g_nets[i].macCount ? macToString(g_nets[i].macs[0]).c_str() : "-");
}

static void processLine(const String& line) {
  if (line.startsWith("PROV ")) {
    // PROV <b64 ssid> <b64 pass> <b64 host>
    String rest = line.substring(5);
    int a = rest.indexOf(' '), b = rest.indexOf(' ', a + 1);
    if (a < 0) { Serial.println("PROV FEHLER format"); return; }
    String ssid = b64decode(rest.substring(0, a));
    String pass = b64decode(b < 0 ? rest.substring(a + 1) : rest.substring(a + 1, b));
    String host = b < 0 ? "" : b64decode(rest.substring(b + 1));
    if (ssid.isEmpty()) { Serial.println("PROV FEHLER ssid leer"); return; }
    if (!host.isEmpty() && host != g_host) { g_host = host; g_prefs.putString("host", g_host); }
    addNet(ssid, pass);
    Serial.printf("PROV OK ssid=%s host=%s netze=%d\n", ssid.c_str(), g_host.c_str(), g_netCount);
    return;
  }
  if (line.startsWith("MACADDR ")) {
    // Die Adresse, die der Mac im *aktuell verbundenen* Netz benutzt. Wird erst nach dem
    // Verbinden geschickt, damit sie eindeutig zu diesem Netz gehoert.
    String v = line.substring(8); v.trim();
    uint8_t m[6];
    if (g_current < 0) { Serial.println("MACADDR FEHLER nicht verbunden"); return; }
    if (!parseMac(v, m)) { Serial.println("MACADDR FEHLER format"); return; }
    addMacTo(g_current, m);
    Serial.printf("MACADDR OK %s fuer \"%s\"\n", macToString(m).c_str(), g_nets[g_current].ssid.c_str());
    return;
  }
  if (line.startsWith("CONNECT")) { g_reconnectRequested = true; Serial.println("CONNECT OK"); return; }
  if (line.startsWith("RESET")) {
    g_prefs.clear();
    Serial.println("RESET OK – Neustart");
    delay(200);
    ESP.restart();
  }
  if (line.startsWith("STATUS")) printStatus();
  if (line.startsWith("SCAN")) scanNetworks(false);
}

static void handleSerial() {
  static String line;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') { processLine(line); line = ""; }
    else if (ch != '\r' && line.length() < 512) line += ch;
  }
}

// ---------------------------------------------------------------------------
// HTTP zur ooo-Cloud
// ---------------------------------------------------------------------------
// Minimaler HTTP/1.1-Client direkt auf der verschluesselten Verbindung.
// Absichtlich ohne HTTPClient: die Bibliothek setzt bei jedem Aufruf neu an, dadurch wurde
// die Verbindung rund 3.500 Mal am Tag neu ausgehandelt. Das ist der teuerste Teil.
// Hier bleibt sie offen; nur bei Abbruch wird neu verbunden.
static String apiHost() {
  static String host;
  if (host.length()) return host;
  host = String(OOO_URL);
  host.replace("https://", "");
  int slash = host.indexOf('/');
  if (slash >= 0) host = host.substring(0, slash);
  return host;
}

// Lesen mit eigener Zeitgrenze. setTimeout() ist hier unbrauchbar: auf einer
// verschluesselten Verbindung schlaegt es fehl ("Bad file number") und die Einheit
// (Sekunden oder Millisekunden) unterscheidet sich je nach Core-Version.
static int readByte(uint32_t deadline) {
  while ((int32_t)(millis() - deadline) < 0) {
    int c = g_tls.read();
    if (c >= 0) return c;
    if (!g_tls.connected() && !g_tls.available()) return -1;
    ledTick();
    delay(2);
  }
  return -1;
}

static bool readLine(String& out, uint32_t deadline) {
  out = "";
  while (true) {
    int c = readByte(deadline);
    if (c < 0) return false;
    if (c == '\n') return true;
    if (c != '\r') out += (char)c;
    if (out.length() > 1024) return false;
  }
}

// Antwort lesen: Statuszeile, Kopfzeilen, Rumpf. Gibt den Statuscode zurueck, sonst -1.
static int readResponse(String& response, bool& keepAlive, uint32_t deadline) {
  keepAlive = true;
  String line;
  if (!readLine(line, deadline) || !line.startsWith("HTTP/1.")) return -1;
  int code = line.substring(9, 12).toInt();

  long length = -1;
  bool chunked = false;
  while (readLine(line, deadline)) {
    if (line.isEmpty()) break;                        // Leerzeile: Kopf zu Ende
    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) length = line.substring(15).toInt();
    else if (lower.startsWith("connection:") && lower.indexOf("close") >= 0) keepAlive = false;
    else if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) chunked = true;
  }

  response = "";
  if (chunked) {
    while (true) {
      if (!readLine(line, deadline)) return -1;
      long n = strtol(line.c_str(), nullptr, 16);
      if (n <= 0) { readLine(line, deadline); break; }
      while (n-- > 0) {
        int c = readByte(deadline);
        if (c < 0) return -1;
        response += (char)c;
      }
      readLine(line, deadline);                       // CRLF nach dem Block
    }
  } else if (length > 0) {
    response.reserve(length);
    while (length-- > 0) {
      int c = readByte(deadline);
      if (c < 0) return -1;
      response += (char)c;
    }
  }
  return code;
}

static int requestOnce(const char* path, const String& body, String& response, uint32_t timeoutMs) {
  uint32_t deadline = millis() + timeoutMs;
  if (!g_tls.connected()) {
    if (!g_tlsReady) { g_tls.setCACert(ISRG_ROOTS); g_tls.setHandshakeTimeout(20); g_tlsReady = true; }
    if (!g_tls.connect(apiHost().c_str(), 443)) return -1;
    Serial.println("[tls] neue Verbindung aufgebaut");
  }

  g_tls.print(String("POST ") + path + " HTTP/1.1\r\n");
  g_tls.print("Host: " + apiHost() + "\r\n");
  g_tls.print("Authorization: Bearer " OOO_DEVICE_TOKEN "\r\n");
  g_tls.print("Content-Type: application/json\r\n");
  g_tls.print("Connection: keep-alive\r\n");
  g_tls.print("Content-Length: " + String(body.length()) + "\r\n\r\n");
  g_tls.print(body);

  bool keepAlive = true;
  int code = readResponse(response, keepAlive, deadline);
  if (code < 0 || !keepAlive) g_tls.stop();
  return code;
}

static int request(const char* path, const String& body, String& response, uint32_t timeoutMs) {
  int code = requestOnce(path, body, response, timeoutMs);
  // Eine offene Verbindung kann zwischenzeitlich vom Server geschlossen worden sein.
  // Dann genau einmal mit frischer Verbindung wiederholen.
  if (code < 0) {
    g_tls.stop();
    code = requestOnce(path, body, response, timeoutMs);
  }
  return code;
}

static String infoJson() {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["rssi"] = WiFi.RSSI();
  doc["ssid"] = g_current >= 0 ? g_nets[g_current].ssid : "";
  doc["uptime_s"] = millis() / 1000;
  doc["mac_host"] = g_host;
  doc["mac_reachable"] = g_macReachable;
  doc["mac_addr"] = (g_current >= 0 && g_nets[g_current].macCount > 0) ? macToString(g_nets[g_current].macs[0]) : "";
  doc["known_nets"] = g_netCount;
  if (RELAY_ENABLED) doc["relay"] = g_powered ? "on" : "off";
  String out;
  serializeJson(doc, out);
  return out;
}

static void ack(long id, const char* result) {
  String body = String("{\"id\":") + id + ",\"result\":\"" + result + "\"}";
  String resp;
  int code = request("/ack", body, resp, 15000);
  Serial.printf("[ack] id=%ld result=%s http=%d\n", id, result, code);
}

static const char* execute(const char* action, JsonObjectConst payload) {
  if (strcmp(action, "wake") == 0) {
    Serial.println("[cmd] wake");
    ledMode(LED_WORKING);
    if (macReachable()) { ledMode(LED_READY); return "already-awake"; }
    bool sent = sendWol();
    if (RELAY_ENABLED) { relaySet(false); waitTicking(POWER_PULSE_MS); relaySet(true); }
    if (!sent && !RELAY_ENABLED) { ledMode(LED_ERROR); return "no-mac-address-yet"; }
    uint32_t until = millis() + WAKE_VERIFY_SEC * 1000UL;
    while (millis() < until) {
      waitTicking(2000);
      if (macReachable()) { g_macReachable = true; ledMode(LED_READY); return "mac-up"; }
    }
    ledMode(LED_ERROR);
    return "no-ping-response";
  }
  if (strcmp(action, "power") == 0) {
    if (!RELAY_ENABLED) return "relay-disabled";
    relaySet(strcmp(payload["state"] | "on", "on") == 0);
    return "ok";
  }
  return "unknown-action";
}

static void pollOnce() {
  g_macReachable = macReachable();
  String body = String("{\"wait\":") + POLL_WAIT_SEC + ",\"info\":" + infoJson() + "}";
  String resp;
  int code = request("/poll", body, resp, (POLL_WAIT_SEC + 15) * 1000UL);

  if (code != 200) {
    Serial.printf("[poll] http=%d – naechster Versuch in %lu ms\n", code, (unsigned long)g_backoffMs);
    ledMode(LED_ERROR);
    waitTicking(g_backoffMs);
    g_backoffMs = min<uint32_t>(g_backoffMs * 2, BACKOFF_MAX_MS);
    return;
  }
  g_lastOkMs = millis();
  g_backoffMs = BACKOFF_MIN_MS;
  ledMode(LED_READY);

  JsonDocument doc;
  if (deserializeJson(doc, resp)) return;
  JsonObjectConst cmd = doc["command"].as<JsonObjectConst>();
  if (cmd.isNull()) return;
  ack(cmd["id"] | 0L, execute(cmd["action"] | "", cmd["payload"].as<JsonObjectConst>()));
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n\nooo firmware %s\n", FW_VERSION);
  pinMode(LED_PIN, OUTPUT);
  ledMode(LED_IDLE);
  ledTick();
  if (RELAY_ENABLED) { pinMode(RELAY_PIN, OUTPUT); relaySet(true); }

  WiFi.onEvent(onWifiEvent);
  loadPrefs();
  Serial.printf("[prefs] %d bekannte Netze, Mac-Name \"%s\"\n", g_netCount, g_host.c_str());
  if (g_netCount == 0) waitForSetup();
  connectWifi();
  ensureTime();
  g_lastOkMs = millis();
}

void loop() {
  handleSerial();
  if (g_reconnectRequested) { g_reconnectRequested = false; connectWifi(); }
  ensureWifi();
  if (WiFi.status() == WL_CONNECTED) pollOnce();
  if (millis() - g_lastOkMs > REBOOT_AFTER_OFFLINE_MS) {
    Serial.println("[watchdog] lange kein erfolgreicher Poll – Neustart");
    ESP.restart();
  }
  waitTicking(POLL_MIN_GAP_MS);
}

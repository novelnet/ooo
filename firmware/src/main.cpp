// ooo – ESP32 Wake-Trigger
//
// Einrichten: ESP32 per USB an den Mac stecken und dort `bash mac/setup.sh` laufen lassen.
// Der Mac schickt WLAN-Zugangsdaten, seinen Namen und seine MAC-Adresse über das USB-Kabel.
// Danach merkt sich der ESP32 alles (Flash) und braucht den Mac nie wieder zum Starten.
//
// Im Betrieb: pollt die ooo-Edge-Function (Long-Poll, TLS mit gepinnter Root-CA) und weckt
// den Mac per Wake-on-LAN. Bei jedem Poll pingt er den Mac und meldet das Ergebnis mit.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
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
#include "isrg_root_x1.h"

static Preferences g_prefs;
static String   g_ssid, g_pass, g_host;
static uint8_t  g_macAddr[6] = {0};
static bool     g_haveMac = false;
static bool     g_macReachable = false;
static bool     g_powered = true;
static uint32_t g_lastOkMs = 0;
static uint32_t g_backoffMs = BACKOFF_MIN_MS;

// ---------------------------------------------------------------------------
// LED:  dauerhaft an = WLAN verbunden · schnell = verbindet
//       langsam = wartet auf Einrichtung per USB · 3× kurz = Relay nicht erreichbar
// ---------------------------------------------------------------------------
enum LedMode { LED_OFF, LED_ON, LED_FAST, LED_SLOW };
static LedMode g_led = LED_OFF;
static void led(bool on) { digitalWrite(LED_PIN, on ? HIGH : LOW); }
static void ledMode(LedMode m) { g_led = m; if (m == LED_ON) led(true); if (m == LED_OFF) led(false); }
static void ledTick() {
  if (g_led == LED_FAST) led((millis() / 100) % 2);
  if (g_led == LED_SLOW) led((millis() / 500) % 2);
}
static void blink(int times, int ms = 80) {
  for (int i = 0; i < times; i++) { led(true); delay(ms); led(false); delay(ms); }
  ledMode(g_led);
}

// ---------------------------------------------------------------------------
// Relais (optional)
// ---------------------------------------------------------------------------
static void relaySet(bool powered) {
  g_powered = powered;
  if (!RELAY_ENABLED) return;
  digitalWrite(RELAY_PIN, (powered ? !RELAY_ACTIVE_LOW : RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// Einstellungen im Flash
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

static void loadPrefs() {
  g_prefs.begin("ooo", false);
  // isKey() vorweg, sonst meldet die NVS-Bibliothek beim ersten Start vier "NOT_FOUND"-Fehler.
  if (g_prefs.isKey("ssid")) g_ssid = g_prefs.getString("ssid", "");
  if (g_prefs.isKey("pass")) g_pass = g_prefs.getString("pass", "");
  if (g_prefs.isKey("host")) g_host = g_prefs.getString("host", "");
  g_haveMac = g_prefs.isKey("mac") && g_prefs.getBytes("mac", g_macAddr, 6) == 6;
}

static void rememberMac(const uint8_t m[6]) {
  if (g_haveMac && memcmp(m, g_macAddr, 6) == 0) return;
  memcpy(g_macAddr, m, 6);
  g_haveMac = true;
  g_prefs.putBytes("mac", g_macAddr, 6);
  Serial.printf("[prefs] MAC-Adresse gelernt: %s\n", macToString(g_macAddr).c_str());
}

// ---------------------------------------------------------------------------
// Einrichtung über USB. Der Mac schickt eine Zeile:
//   PROV <base64 ssid> <base64 passwort> <base64 hostname> <mac-adresse>
// Antwort: "PROV OK". Außerdem: STATUS (Zustand ausgeben), RESET (alles vergessen).
// ---------------------------------------------------------------------------
static String b64decode(const String& in) {
  size_t len = 0;
  unsigned char buf[256];
  if (mbedtls_base64_decode(buf, sizeof(buf) - 1, &len, (const unsigned char*)in.c_str(), in.length()) != 0) return "";
  buf[len] = 0;
  return String((char*)buf);
}

static bool g_reconnectRequested = false;

static void printStatus() {
  Serial.printf("STATUS fw=%s ssid=%s verbunden=%s ip=%s mac=%s host=%s mac_erreichbar=%s\n",
                FW_VERSION, g_ssid.c_str(), WiFi.status() == WL_CONNECTED ? "ja" : "nein",
                WiFi.localIP().toString().c_str(), g_haveMac ? macToString(g_macAddr).c_str() : "-",
                g_host.c_str(), g_macReachable ? "ja" : "nein");
}

static void processLine(const String& line) {
  if (line.startsWith("PROV ")) {
    String rest = line.substring(5);
    int a = rest.indexOf(' '), b = rest.indexOf(' ', a + 1), c = rest.indexOf(' ', b + 1);
    if (a < 0 || b < 0 || c < 0) { Serial.println("PROV FEHLER format"); return; }
    String ssid = b64decode(rest.substring(0, a));
    String pass = b64decode(rest.substring(a + 1, b));
    String host = b64decode(rest.substring(b + 1, c));
    String mac  = rest.substring(c + 1);
    mac.trim();
    if (ssid.isEmpty()) { Serial.println("PROV FEHLER ssid leer"); return; }

    g_ssid = ssid; g_pass = pass; g_host = host;
    g_prefs.putString("ssid", g_ssid);
    g_prefs.putString("pass", g_pass);
    g_prefs.putString("host", g_host);
    uint8_t m[6];
    if (parseMac(mac, m)) { memcpy(g_macAddr, m, 6); g_haveMac = true; g_prefs.putBytes("mac", m, 6); }
    g_reconnectRequested = true;
    Serial.printf("PROV OK ssid=%s host=%s mac=%s\n", g_ssid.c_str(), g_host.c_str(),
                  g_haveMac ? macToString(g_macAddr).c_str() : "-");
    return;
  }
  if (line.startsWith("RESET")) {
    g_prefs.clear();
    Serial.println("RESET OK – Neustart");
    delay(200);
    ESP.restart();
  }
  if (line.startsWith("STATUS")) printStatus();
}

static void handleSerial() {
  static String line;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n') { processLine(line); line = ""; }
    else if (ch != '\r' && line.length() < 512) line += ch;
  }
}

static void waitTicking(uint32_t ms) {
  uint32_t end = millis() + ms;
  while (millis() < end) { handleSerial(); ledTick(); delay(10); }
}

// ---------------------------------------------------------------------------
// WLAN
// ---------------------------------------------------------------------------
static bool connectWifi() {
  if (g_ssid.isEmpty()) return false;
  ledMode(LED_FAST);
  Serial.printf("[wifi] verbinde mit %s …\n", g_ssid.c_str());
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname("ooo-esp");
  WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 25000) waitTicking(50);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WIFI FEHLER – Zugangsdaten pruefen (bash mac/setup.sh)");
    ledMode(LED_SLOW);
    return false;
  }
  ledMode(LED_ON);
  Serial.printf("WIFI OK %s ip=%s rssi=%d\n", g_ssid.c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
  MDNS.begin("ooo-esp");
  return true;
}

// Ohne Zugangsdaten: langsam blinken und auf die Einrichtung über USB warten.
static void waitForSetup() {
  Serial.println("WARTE AUF EINRICHTUNG – ESP32 per USB an den Mac, dort: bash mac/setup.sh");
  ledMode(LED_SLOW);
  while (g_ssid.isEmpty()) waitTicking(100);
}

static void ensureWifi() {
  if (g_reconnectRequested) { g_reconnectRequested = false; connectWifi(); return; }
  if (WiFi.status() == WL_CONNECTED) { if (g_led != LED_ON) ledMode(LED_ON); return; }
  Serial.println("[wifi] Verbindung weg – neu verbinden");
  if (!connectWifi()) { waitForSetup(); connectWifi(); }
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

// ---------------------------------------------------------------------------
// Wake-on-LAN
// ---------------------------------------------------------------------------
static bool sendWol() {
  if (!g_haveMac) { Serial.println("[wol] keine MAC-Adresse bekannt"); return false; }
  uint8_t pkt[102];
  memset(pkt, 0xFF, 6);
  for (int i = 1; i <= 16; i++) memcpy(pkt + i * 6, g_macAddr, 6);
  WiFiUDP udp;
  udp.begin(0);
  for (int i = 0; i < WOL_BURST; i++) {
    udp.beginPacket(IPAddress(255, 255, 255, 255), 9);
    udp.write(pkt, sizeof(pkt));
    udp.endPacket();
    waitTicking(300);
  }
  udp.stop();
  Serial.printf("[wol] %d Magic Packets an %s\n", WOL_BURST, macToString(g_macAddr).c_str());
  return true;
}

// ---------------------------------------------------------------------------
// HTTP zur Edge Function
// ---------------------------------------------------------------------------
static int request(const char* path, const String& body, String& response, uint32_t timeoutMs) {
  WiFiClientSecure client;
  client.setCACert(ISRG_ROOT_X1);
  client.setTimeout(timeoutMs / 1000);
  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  if (!http.begin(client, String(OOO_URL) + path)) return -1;
  http.addHeader("Authorization", String("Bearer ") + OOO_DEVICE_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  if (code > 0) response = http.getString();
  http.end();
  return code;
}

static String infoJson() {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["rssi"] = WiFi.RSSI();
  doc["ssid"] = g_ssid;
  doc["uptime_s"] = millis() / 1000;
  doc["mac_host"] = g_host;
  doc["mac_reachable"] = g_macReachable;
  doc["mac_addr"] = g_haveMac ? macToString(g_macAddr) : "";
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

// ---------------------------------------------------------------------------
// Kommandos
// ---------------------------------------------------------------------------
static const char* execute(const char* action, JsonObjectConst payload) {
  if (strcmp(action, "wake") == 0) {
    Serial.println("[cmd] wake");
    ledMode(LED_SLOW);
    if (macReachable()) { ledMode(LED_ON); return "already-awake"; }
    bool sent = sendWol();
    if (RELAY_ENABLED) { relaySet(false); waitTicking(POWER_PULSE_MS); relaySet(true); }
    if (!sent && !RELAY_ENABLED) { ledMode(LED_ON); return "no-mac-address-yet"; }
    uint32_t until = millis() + WAKE_VERIFY_SEC * 1000UL;
    while (millis() < until) {
      waitTicking(2000);
      if (macReachable()) { g_macReachable = true; ledMode(LED_ON); return "mac-up"; }
    }
    ledMode(LED_ON);
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
    blink(3);
    waitTicking(g_backoffMs);
    g_backoffMs = min<uint32_t>(g_backoffMs * 2, BACKOFF_MAX_MS);
    return;
  }
  g_lastOkMs = millis();
  g_backoffMs = BACKOFF_MIN_MS;

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
  if (RELAY_ENABLED) { pinMode(RELAY_PIN, OUTPUT); relaySet(true); }

  loadPrefs();
  if (g_ssid.isEmpty()) waitForSetup();
  connectWifi();
  ensureTime();
  g_lastOkMs = millis();
}

void loop() {
  handleSerial();
  ensureWifi();
  pollOnce();
  if (millis() - g_lastOkMs > REBOOT_AFTER_OFFLINE_MS) {
    Serial.println("[watchdog] lange kein erfolgreicher Poll – Neustart");
    ESP.restart();
  }
  waitTicking(POLL_MIN_GAP_MS);
}

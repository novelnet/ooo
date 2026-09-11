// ooo – ESP32 Wake-Trigger
//
// Pollt die ooo-Edge-Function (Long-Poll, TLS mit gepinnter Root-CA) und weckt den Mac
// per Wake-on-LAN. Bei jedem Poll meldet er mit, ob der Mac im LAN auf Ping antwortet.
//
// Nichts von Hand eintragen:
//   WLAN        → einmal per Handy im Setup-WLAN "ooo-setup" (wird im Flash gespeichert)
//   MAC-Adresse → lernt der ESP32 aus der ARP-Tabelle, sobald der Mac einmal wach war

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>
#include "lwip/etharp.h"
#include "lwip/netif.h"

#include "config.h"
#include "secrets.h"
#include "isrg_root_x1.h"

static Preferences g_prefs;                // NVS: Mac-Hostname + gelernte MAC-Adresse
static String   g_macHost;
static uint8_t  g_macAddr[6] = {0};
static bool     g_haveMac = false;
static bool     g_macReachable = false;
static bool     g_powered = true;
static uint32_t g_lastOkMs = 0;
static uint32_t g_backoffMs = BACKOFF_MIN_MS;

// ---------------------------------------------------------------------------
// LED – zeigt den Zustand:
//   dauerhaft an      WLAN verbunden, alles gut
//   schnelles Blinken verbindet / kein WLAN
//   langsames Blinken Setup-WLAN "ooo-setup" ist offen (oder Wake läuft)
//   3× kurz           Relay nicht erreichbar (HTTP-Fehler)
// ---------------------------------------------------------------------------
enum LedMode { LED_OFF, LED_ON, LED_FAST, LED_SLOW };
static LedMode g_led = LED_OFF;
static void led(bool on) { digitalWrite(LED_PIN, on ? HIGH : LOW); }
static void ledMode(LedMode m) { g_led = m; if (m == LED_ON) led(true); if (m == LED_OFF) led(false); }
static void ledTick() {
  if (g_led == LED_FAST) led((millis() / 100) % 2);
  if (g_led == LED_SLOW) led((millis() / 500) % 2);
}
static void waitTicking(uint32_t ms) { uint32_t end = millis() + ms; while (millis() < end) { ledTick(); delay(10); } }
static void blink(int times, int ms = 80) {
  for (int i = 0; i < times; i++) { led(true); delay(ms); led(false); delay(ms); }
  ledMode(g_led);
}
static void relaySet(bool powered) {
  g_powered = powered;
  if (!RELAY_ENABLED) return;
  bool level = powered ? !RELAY_ACTIVE_LOW : RELAY_ACTIVE_LOW;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  Serial.printf("[relay] Mac power %s\n", powered ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Gespeicherte Einstellungen
// ---------------------------------------------------------------------------
static String macToString(const uint8_t m[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

static void loadPrefs() {
  g_prefs.begin("ooo", false);
  g_macHost = g_prefs.getString("host", MAC_HOSTNAME_DEFAULT);
  g_haveMac = g_prefs.getBytes("mac", g_macAddr, 6) == 6;
  Serial.printf("[prefs] host=%s mac=%s\n", g_macHost.c_str(), g_haveMac ? macToString(g_macAddr).c_str() : "(noch nicht gelernt)");
}

static void rememberMac(const uint8_t m[6]) {
  if (g_haveMac && memcmp(m, g_macAddr, 6) == 0) return;
  memcpy(g_macAddr, m, 6);
  g_haveMac = true;
  g_prefs.putBytes("mac", g_macAddr, 6);
  Serial.printf("[prefs] Mac-Adresse gelernt: %s\n", macToString(g_macAddr).c_str());
}

// ---------------------------------------------------------------------------
// WLAN: gespeicherte Zugangsdaten (im Flash), sonst Setup-Portal. BOOT 3 s → vergessen.
// ---------------------------------------------------------------------------
static bool tryStoredWifi() {
  ledMode(LED_FAST);
  WiFi.begin();                              // nutzt die gespeicherten Zugangsdaten
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) waitTicking(50);
  return WiFi.status() == WL_CONNECTED;
}

static void runSetupPortal() {
  Serial.printf("[wifi] kein WLAN – Setup-WLAN \"%s\" offen (%d s)\n", SETUP_AP_NAME, SETUP_TIMEOUT_SEC);
  ledMode(LED_SLOW);
  WiFiManager wm;
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(SETUP_TIMEOUT_SEC);
  WiFiManagerParameter hostParam("host", "Hostname des Mac (hostname -s)", g_macHost.c_str(), 32);
  wm.addParameter(&hostParam);
  wm.setSaveParamsCallback([&]() {
    g_macHost = hostParam.getValue();
    g_prefs.putString("host", g_macHost);
  });
  wm.startConfigPortal(SETUP_AP_NAME);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    wm.process();
    ledTick();
    delay(10);
    if (millis() - start > SETUP_TIMEOUT_SEC * 1000UL) { Serial.println("[wifi] Portal-Timeout – Neustart"); ESP.restart(); }
  }
  wm.stopConfigPortal();
  WiFi.mode(WIFI_STA);
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname("ooo-esp");

  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(3000);
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("[wifi] BOOT gehalten – WLAN-Zugangsdaten gelöscht");
      WiFi.disconnect(true, true);
      blink(10, 50);
    }
  }

  while (!tryStoredWifi()) runSetupPortal();
  ledMode(LED_ON);
  Serial.printf("[wifi] verbunden mit %s, ip=%s rssi=%d\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

static void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) { ledMode(LED_ON); return; }
  Serial.println("[wifi] Verbindung weg – reconnect");
  ledMode(LED_FAST);
  WiFi.reconnect();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) waitTicking(50);
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  ledMode(LED_ON);
}

static void ensureTime() {
  time_t now = time(nullptr);
  if (now > 1700000000) return;
  configTime(0, 0, "pool.ntp.org", "time.apple.com");
  uint32_t start = millis();
  while ((now = time(nullptr)) < 1700000000 && millis() - start < 20000) delay(250);
  Serial.printf("[time] %s", now > 1700000000 ? ctime(&now) : "sync FAILED (TLS may fail)\n");
}

// ---------------------------------------------------------------------------
// Mac im LAN: mDNS → IP → Ping. Wenn erreichbar: MAC-Adresse aus der ARP-Tabelle merken.
// ---------------------------------------------------------------------------
static bool macReachable() {
  IPAddress ip = MDNS.queryHost(g_macHost, 1500);
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  if (!Ping.ping(ip, 1)) return false;

  ip4_addr_t ip4;
  ip4.addr = (uint32_t)ip;
  struct eth_addr* eth = nullptr;
  const ip4_addr_t* ipRet = nullptr;
  if (etharp_find_addr(netif_default, &ip4, &eth, &ipRet) >= 0 && eth) rememberMac(eth->addr);
  return true;
}

// ---------------------------------------------------------------------------
// Wake-on-LAN
// ---------------------------------------------------------------------------
static bool sendWol() {
  if (!g_haveMac) { Serial.println("[wol] MAC-Adresse noch nicht gelernt – Mac muss einmal wach im LAN gewesen sein"); return false; }
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
  Serial.printf("[wol] %d magic packets an %s\n", WOL_BURST, macToString(g_macAddr).c_str());
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
  doc["ssid"] = WiFi.SSID();
  doc["uptime_s"] = millis() / 1000;
  doc["mac_host"] = g_macHost;
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
    const char* state = payload["state"] | "on";
    relaySet(strcmp(state, "on") == 0);
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
    Serial.printf("[poll] http=%d %s – backoff %lu ms\n", code, resp.substring(0, 120).c_str(), (unsigned long)g_backoffMs);
    blink(3);
    waitTicking(g_backoffMs);
    g_backoffMs = min<uint32_t>(g_backoffMs * 2, BACKOFF_MAX_MS);
    return;
  }
  g_lastOkMs = millis();
  g_backoffMs = BACKOFF_MIN_MS;

  JsonDocument doc;
  if (deserializeJson(doc, resp)) { Serial.println("[poll] bad json"); return; }
  JsonObjectConst cmd = doc["command"].as<JsonObjectConst>();
  if (cmd.isNull()) return;
  long id = cmd["id"] | 0L;
  const char* action = cmd["action"] | "";
  ack(id, execute(action, cmd["payload"].as<JsonObjectConst>()));
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n\nooo firmware %s\n", FW_VERSION);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  if (RELAY_ENABLED) { pinMode(RELAY_PIN, OUTPUT); relaySet(true); }

  loadPrefs();
  connectWifi();
  ensureTime();
  MDNS.begin("ooo-esp");
  g_lastOkMs = millis();
  ledMode(LED_ON);
}

void loop() {
  ensureWifi();
  pollOnce();
  if (millis() - g_lastOkMs > REBOOT_AFTER_OFFLINE_MS) {
    Serial.println("[watchdog] lange kein erfolgreicher Poll – Neustart");
    ESP.restart();
  }
  delay(POLL_MIN_GAP_MS);
}

// ooo – Briefkasten zwischen Handy, Claude und dem ESP32.
//
// Läuft auf Deno Deploy, Zustand in Deno KV. Kein eigener Server, keine Datenbank.
//
// Routen:
//   POST /wake                 user-token   → Weckbefehl einreihen
//   GET  /status               user-token   → Ist der ESP32 online? Antwortet der Mac?
//   POST /poll  {wait, info}   device-token → ESP32 wartet auf einen Befehl (bis 25 s)
//   POST /ack   {id, result}   device-token → ESP32 bestätigt die Ausführung
//   POST /mcp                  user-token   → MCP für Claude (wake_mac, mac_status)
//        /mcp/<user-token>                  → gleiche Schnittstelle, Schlüssel in der Adresse,
//                                             für Clients ohne eigene Kopfzeilen
//
// Schlüssel als Umgebungsvariablen: OOO_USER_TOKEN, OOO_DEVICE_TOKEN

const kv = await Deno.openKv();

const KEY_CMD = ["cmd"];
const KEY_ESP = ["esp"];
const KEY_LAST = ["last"];

const MAX_WAIT_SEC = 25;
const COMMAND_TTL_MS = 15 * 60_000;
const ESP_ONLINE_SEC = 90;

const USER_TOKEN = Deno.env.get("OOO_USER_TOKEN") ?? "";
const DEVICE_TOKEN = Deno.env.get("OOO_DEVICE_TOKEN") ?? "";

type Role = "user" | "device";

async function sha256(s: string): Promise<Uint8Array> {
  return new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s)));
}

// Vergleich über Hashes: gleiche Laufzeit unabhängig vom Inhalt.
async function sameToken(given: string, expected: string): Promise<boolean> {
  if (!expected || expected.length < 16 || !given) return false;
  const [a, b] = await Promise.all([sha256(given), sha256(expected)]);
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a[i] ^ b[i];
  return diff === 0;
}

async function roleOf(token: string): Promise<Role | null> {
  if (await sameToken(token, USER_TOKEN)) return "user";
  if (await sameToken(token, DEVICE_TOKEN)) return "device";
  return null;
}

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });

async function readJson(req: Request): Promise<Record<string, unknown>> {
  try {
    const t = await req.text();
    return t ? JSON.parse(t) : {};
  } catch {
    return {};
  }
}

interface Command { id: number; action: string; payload: Record<string, unknown>; created_at: number }
interface EspState { last_seen: number; info: Record<string, unknown> }

async function enqueue(action: string, payload: Record<string, unknown> = {}): Promise<Command> {
  const cmd: Command = { id: Date.now(), action, payload, created_at: Date.now() };
  await kv.set(KEY_CMD, cmd);
  return cmd;
}

async function pendingCommand(): Promise<Command | null> {
  const e = await kv.get<Command>(KEY_CMD);
  if (!e.value) return null;
  if (Date.now() - e.value.created_at > COMMAND_TTL_MS) {
    await kv.delete(KEY_CMD);
    return null;
  }
  return e.value;
}

// Wartet auf einen Befehl, ohne im Kreis zu fragen: Deno KV meldet Änderungen von selbst.
async function waitForCommand(seconds: number): Promise<Command | null> {
  const now = await pendingCommand();
  if (now || seconds <= 0) return now;

  const stream = kv.watch<[Command]>([KEY_CMD]);
  const reader = stream.getReader();
  const timer = setTimeout(() => { reader.cancel().catch(() => {}); }, seconds * 1000);
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) return null;
      const cmd = value?.[0]?.value ?? null;
      if (cmd && Date.now() - cmd.created_at <= COMMAND_TTL_MS) return cmd;
    }
  } catch {
    return null;
  } finally {
    clearTimeout(timer);
    try { reader.releaseLock(); } catch { /* schon freigegeben */ }
  }
}

async function statusReport() {
  const [esp, last] = await Promise.all([kv.get<EspState>(KEY_ESP), kv.get(KEY_LAST)]);
  const ageSec = esp.value ? Math.round((Date.now() - esp.value.last_seen) / 1000) : null;
  const online = ageSec !== null && ageSec < ESP_ONLINE_SEC;
  const info = (esp.value?.info ?? {}) as { mac_reachable?: boolean };
  return {
    esp: { online, age_sec: ageSec, info: esp.value?.info ?? {} },
    // Der ESP32 pingt den Mac im LAN; ohne ihn gibt es keine Aussage.
    mac: { awake: online ? Boolean(info.mac_reachable) : null },
    last_command: last.value ?? null,
    pending: await pendingCommand(),
  };
}

// --- MCP (JSON-RPC über HTTP), damit Claude den Mac wecken kann ---------------
const TOOLS = [
  {
    name: "wake_mac",
    description: "Weckt das MacBook per Wake-on-LAN über den ESP32 im Heimnetz. " +
      "Nutze das, wenn der Mac schläft und wieder erreichbar sein soll.",
    inputSchema: { type: "object", properties: {}, additionalProperties: false },
  },
  {
    name: "mac_status",
    description: "Zeigt, ob der ESP32 online ist und ob das MacBook gerade im Netz antwortet.",
    inputSchema: { type: "object", properties: {}, additionalProperties: false },
  },
];

async function callTool(name: string): Promise<string> {
  if (name === "wake_mac") {
    const s = await statusReport();
    if (s.mac.awake) return "Der Mac ist bereits wach und im Netz erreichbar.";
    const cmd = await enqueue("wake");
    if (!s.esp.online) {
      return `Weckbefehl eingereiht (Nummer ${cmd.id}). Achtung: Der ESP32 hat sich seit über ` +
        `${ESP_ONLINE_SEC} Sekunden nicht gemeldet, der Befehl wird erst ausgeführt, sobald er wieder online ist.`;
    }
    return `Weckbefehl eingereiht (Nummer ${cmd.id}). Der ESP32 holt ihn in wenigen Sekunden ab und ` +
      "schickt ein Wake-on-LAN-Paket. Der Mac braucht danach etwa 10 bis 30 Sekunden.";
  }
  if (name === "mac_status") {
    const s = await statusReport();
    const esp = s.esp.online ? `online (zuletzt vor ${s.esp.age_sec} s)` : "nicht erreichbar";
    const mac = s.mac.awake === null ? "unbekannt, da der ESP32 offline ist" : s.mac.awake ? "wach" : "schläft";
    return `ESP32: ${esp}. MacBook: ${mac}.`;
  }
  throw new Error(`Unbekanntes Werkzeug: ${name}`);
}

async function handleMcp(req: Request): Promise<Response> {
  const msg = await readJson(req) as { id?: unknown; method?: string; params?: Record<string, unknown> };
  const id = msg.id ?? null;
  const ok = (result: unknown) => json({ jsonrpc: "2.0", id, result });

  switch (msg.method) {
    case "initialize": {
      // Die vom Client gewuenschte Protokollversion zurueckspiegeln, sonst unsere.
      const wanted = (msg.params as { protocolVersion?: string })?.protocolVersion;
      return ok({
        protocolVersion: typeof wanted === "string" && wanted ? wanted : "2024-11-05",
        capabilities: { tools: {} },
        serverInfo: { name: "ooo", version: "1.0.0" },
      });
    }
    case "notifications/initialized":
      return new Response(null, { status: 202 });
    case "ping":
      return ok({});
    case "tools/list":
      return ok({ tools: TOOLS });
    case "tools/call": {
      const name = String((msg.params as { name?: string })?.name ?? "");
      try {
        return ok({ content: [{ type: "text", text: await callTool(name) }] });
      } catch (e) {
        return ok({ content: [{ type: "text", text: String(e) }], isError: true });
      }
    }
    default:
      return json({ jsonrpc: "2.0", id, error: { code: -32601, message: `Methode nicht unterstützt: ${msg.method}` } });
  }
}

Deno.serve(async (req) => {
  const url = new URL(req.url);
  const path = url.pathname.replace(/\/+$/, "") || "/";
  const method = req.method.toUpperCase();

  if (path === "/" && method === "GET") return new Response("ooo läuft.\n");

  const mcpInPath = path.match(/^\/mcp\/(.+)$/);
  const bearer = (req.headers.get("authorization") ?? "").replace(/^Bearer\s+/i, "").trim();
  const role = await roleOf(mcpInPath ? mcpInPath[1] : bearer);

  if (!role) return json({ error: "unauthorized" }, 401);
  const deny = () => json({ error: "forbidden for this token" }, 403);

  try {
    if (path === "/mcp" || mcpInPath) {
      if (role !== "user") return deny();
      if (method === "POST") return await handleMcp(req);
      // Dieser Server ist zustandslos und bietet keinen SSE-Datenstrom an. Laut Protokoll
      // gehoert darauf 405 mit Allow-Kopfzeile, nicht 404 – sonst brechen manche Clients ab.
      return new Response(null, { status: 405, headers: { allow: "POST" } });
    }

    if (path === "/wake" && method === "POST") {
      if (role !== "user") return deny();
      return json({ queued: await enqueue("wake") }, 202);
    }

    if (path === "/status" && method === "GET") {
      if (role !== "user") return deny();
      return json(await statusReport());
    }

    if (path === "/poll" && method === "POST") {
      if (role !== "device") return deny();
      const body = await readJson(req);
      const state: EspState = { last_seen: Date.now(), info: (body.info ?? {}) as Record<string, unknown> };
      await kv.set(KEY_ESP, state);
      const wait = Math.min(MAX_WAIT_SEC, Math.max(0, Number(body.wait ?? 0)));
      return json({ command: await waitForCommand(wait) });
    }

    if (path === "/ack" && method === "POST") {
      if (role !== "device") return deny();
      const { id, result } = await readJson(req);
      if (typeof id !== "number") return json({ error: "id required" }, 400);
      const cur = await kv.get<Command>(KEY_CMD);
      if (cur.value?.id === id) await kv.delete(KEY_CMD);
      await kv.set(KEY_LAST, { id, result: String(result ?? "ok"), at: Date.now() });
      return json({ ok: true });
    }

    return json({ error: "not found" }, 404);
  } catch (e) {
    console.error(e);
    return json({ error: "internal", detail: String(e) }, 500);
  }
});

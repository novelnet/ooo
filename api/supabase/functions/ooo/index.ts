// ooo – Cloud-Relay zwischen Handy, ESP32 und Mac.
//
// Routen (Basis: https://<ref>.supabase.co/functions/v1/ooo):
//   POST /wake                  user-token   → Kommando "wake" einreihen
//   POST /power  {state}        user-token   → Kommando "power" (on|off) einreihen
//   GET  /status                user-token   → Heartbeats + offene Kommandos
//   POST /poll   {wait, info}   device-token → ESP32 holt nächstes Kommando (long-poll);
//                                              info.mac_reachable = Ping-Ergebnis aus dem LAN
//   POST /ack    {id, result}   device-token → ESP32 bestätigt Ausführung
//
// Secrets (supabase secrets set): OOO_USER_TOKEN, OOO_DEVICE_TOKEN
// SUPABASE_URL / SUPABASE_SERVICE_ROLE_KEY stellt Supabase automatisch bereit.

import { createClient } from "npm:@supabase/supabase-js@2";

const COMMAND_TTL_MIN = 15;   // ältere, unbestätigte Kommandos werden nicht mehr ausgeliefert
const MAX_WAIT_SEC = 25;      // Obergrenze fürs Long-Polling
const ACTIONS = new Set(["wake", "power"]);

type Role = "user" | "device";

const db = createClient(
  Deno.env.get("SUPABASE_URL")!,
  Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
  { auth: { persistSession: false } },
);

const tokens: Record<Role, string | undefined> = {
  user: Deno.env.get("OOO_USER_TOKEN"),
  device: Deno.env.get("OOO_DEVICE_TOKEN"),
};

async function sha256(s: string): Promise<Uint8Array> {
  return new Uint8Array(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s)));
}

// Vergleich über Hashes: konstante Länge, kein Early-Exit auf dem Klartext.
async function tokenMatches(given: string, expected?: string): Promise<boolean> {
  if (!expected || expected.length < 16) return false;
  const [a, b] = await Promise.all([sha256(given), sha256(expected)]);
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a[i] ^ b[i];
  return diff === 0;
}

async function authenticate(req: Request): Promise<Role | null> {
  const header = req.headers.get("authorization") ?? "";
  const given = header.replace(/^Bearer\s+/i, "").trim();
  if (!given) return null;
  for (const role of ["user", "device"] as Role[]) {
    if (await tokenMatches(given, tokens[role])) return role;
  }
  return null;
}

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });

async function readJson(req: Request): Promise<Record<string, unknown>> {
  try {
    const text = await req.text();
    return text ? JSON.parse(text) : {};
  } catch {
    return {};
  }
}

async function touch(name: "esp", info: unknown) {
  const { error } = await db
    .from("ooo_devices")
    .upsert({ name, last_seen: new Date().toISOString(), info: info ?? {} });
  if (error) throw error;
}

async function enqueue(action: string, payload: Record<string, unknown> = {}) {
  const { data, error } = await db
    .from("ooo_commands")
    .insert({ action, payload })
    .select("id, created_at")
    .single();
  if (error) throw error;
  return data;
}

async function nextPending() {
  const cutoff = new Date(Date.now() - COMMAND_TTL_MIN * 60_000).toISOString();
  const { data, error } = await db
    .from("ooo_commands")
    .select("id, action, payload, created_at")
    .is("acked_at", null)
    .gt("created_at", cutoff)
    .order("created_at", { ascending: true })
    .limit(1)
    .maybeSingle();
  if (error) throw error;
  return data;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

Deno.serve(async (req) => {
  const url = new URL(req.url);
  // Pfad nach dem Funktionsnamen: /functions/v1/ooo/<route>  oder lokal /ooo/<route>
  const route = "/" + url.pathname.split("/").filter(Boolean).slice(-1)[0];
  const role = await authenticate(req);
  if (!role) return json({ error: "unauthorized" }, 401);

  const method = req.method.toUpperCase();
  const deny = () => json({ error: "forbidden for this token" }, 403);

  try {
    if (route === "/wake" && method === "POST") {
      if (role !== "user") return deny();
      const cmd = await enqueue("wake");
      return json({ queued: cmd }, 202);
    }

    if (route === "/power" && method === "POST") {
      if (role !== "user") return deny();
      const { state } = await readJson(req);
      if (state !== "on" && state !== "off") return json({ error: "state must be on|off" }, 400);
      const cmd = await enqueue("power", { state });
      return json({ queued: cmd }, 202);
    }

    if (route === "/status" && method === "GET") {
      if (role !== "user") return deny();
      const [{ data: esp }, { data: commands }] = await Promise.all([
        db.from("ooo_devices").select("last_seen, info").eq("name", "esp").maybeSingle(),
        db.from("ooo_commands")
          .select("id, action, payload, created_at, acked_at, result")
          .order("created_at", { ascending: false })
          .limit(5),
      ]);
      const ageSec = esp ? Math.round((Date.now() - Date.parse(esp.last_seen)) / 1000) : null;
      const espOnline = ageSec !== null && ageSec < 90;
      return json({
        esp: { online: espOnline, last_seen: esp?.last_seen ?? null, age_sec: ageSec, info: esp?.info ?? {} },
        // Ping-Ergebnis des ESP32 aus dem LAN; nur aussagekräftig, wenn der ESP32 selbst online ist
        mac: { awake: espOnline ? Boolean((esp?.info as { mac_reachable?: boolean })?.mac_reachable) : null },
        recent_commands: commands ?? [],
      });
    }

    if (route === "/poll" && method === "POST") {
      if (role !== "device") return deny();
      const body = await readJson(req);
      const wait = Math.min(MAX_WAIT_SEC, Math.max(0, Number(body.wait ?? 0)));
      await touch("esp", body.info);
      const deadline = Date.now() + wait * 1000;
      let cmd = await nextPending();
      while (!cmd && Date.now() < deadline) {
        await sleep(1000);
        cmd = await nextPending();
      }
      return json({ command: cmd ?? null });
    }

    if (route === "/ack" && method === "POST") {
      if (role !== "device") return deny();
      const { id, result } = await readJson(req);
      if (typeof id !== "number") return json({ error: "id required" }, 400);
      const { error } = await db
        .from("ooo_commands")
        .update({ acked_at: new Date().toISOString(), result: String(result ?? "ok") })
        .eq("id", id);
      if (error) throw error;
      return json({ ok: true });
    }

    return json({ error: "not found" }, 404);
  } catch (e) {
    console.error(e);
    return json({ error: "internal", detail: String(e) }, 500);
  }
});

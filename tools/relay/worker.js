// FULLDEX RELAY (Cloudflare Worker). Two jobs:
//
// 1. TRADE ROOMS (/v1): two devices that typed the same short code pass the launcher's trade messages
//    (hub_link.cpp) to each other. Each device asks every second or so:
//
//      POST /v1/<CODE>/<device id>    body: its messages since last time (may be empty)
//      reply: one byte, how many devices are in the room (1 or 2), then the other's new messages
//
//    A room holds two devices; a third is turned away (409). Nothing is kept: a room is forgotten
//    15 minutes after anyone last asked. The relay does not read the messages.
//
// 2. THEME GALLERY (/v2): players share themes (the launcher's .zip: theme.txt, preview.jpg, pictures/).
//    Uploads wait in a pending queue in R2 until the moderator approves them from a link in Discord.
//
//      POST   /v2/themes?author=NAME        body: the .zip (max 20 MB). Reply: {code, status, owner}
//      GET    /v2/themes?cursor=&limit=     approved themes, newest first: {themes:[{code,name,author,ts,size}], cursor}
//      GET    /v2/themes/<CODE>             the .zip (approved only; the code may have a dash and any case)
//      GET    /v2/themes/<CODE>/preview     preview.jpg (approved only)
//      GET    /v2/themes/<CODE>/status      {status}: pending, approved, rejected or removed
//      POST   /v2/themes/<CODE>/report      body: a short reason. 3 different reporters hide it until re-checked
//      DELETE /v2/themes/<CODE>?o=OWNER     the uploader takes their own theme down
//      GET/POST /v2/mod/<CODE>/<action>?t=  moderator links (approve, reject, remove, zip, preview) and
//      GET      /v2/mod/queue?t=            the pending queue. GET only shows a page; POST does it, so a
//                                           link preview in Discord can never approve anything.
//
// Secrets (npx wrangler secret put NAME): MOD_SECRET (any long random text), DISCORD_WEBHOOK (the webhook URL).
// Optional vars: PUBLIC_URL (links in Discord), UPLOAD_LIMIT (uploads per IP per hour, 5), REPORT_HOLD (3).
const MAX_BODY = 64 * 1024, MAX_ROOM = 512 * 1024, FORGET_MS = 15 * 60 * 1000;
const MAX_ZIP = 20 * 1024 * 1024, MAX_PREVIEW = 2 * 1024 * 1024, MAX_THEME_TXT = 16 * 1024;
const MAX_ENTRIES = 40, MAX_UNPACKED = 96 * 1024 * 1024;
const CODE_CHARS = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
const NAME_OK = /^(theme\.txt|preview\.jpg|pictures\/[^\/\\\x00-\x1f"<>|:*?]{1,100}\.(png|jpe?g|bmp|gif|tga))$/iu;

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    if (url.pathname === "/") return new Response("Fulldex relay\n");
    const m = url.pathname.match(/^\/v1\/([A-Z0-9]{4,12})\/([a-z0-9]{8,32})$/);
    if (m) {
      if (request.method !== "POST") return nf();
      return env.ROOMS.get(env.ROOMS.idFromName(m[1])).fetch(request);
    }
    if (url.pathname.startsWith("/v2/")) {
      try {
        return await v2(request, env, url);
      } catch (e) {
        return text("something went wrong\n", 500);
      }
    }
    return nf();
  },
};

// ---------------------------------------------------------------- small helpers
const nf = () => new Response("not found\n", { status: 404 });
const text = (s, status = 200) => new Response(s, { status, headers: { "Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store" } });
const json = (o, status = 200, extra = {}) => new Response(JSON.stringify(o), { status, headers: { "Content-Type": "application/json", "Cache-Control": "no-store", ...extra } });
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const md = (s) => String(s).replace(/[\\`*_~|>\[\]()@#:]/g, "\\$&");
const kb = (n) => (n >= 1048576 ? (n / 1048576).toFixed(1) + " MB" : Math.max(1, Math.round(n / 1024)) + " KB");
const fmt = (c) => c.slice(0, 4) + "-" + c.slice(4);
const clean = (s, n) => String(s || "").replace(/[\x00-\x1f\x7f]/g, " ").replace(/\s+/g, " ").trim().slice(0, n);
const hex = (buf) => [...new Uint8Array(buf)].map((x) => x.toString(16).padStart(2, "0")).join("");
const baseUrl = (env, url) => (env.PUBLIC_URL || url.origin).replace(/\/$/, "");

function normCode(s) {
  const c = String(s || "").toUpperCase().replace(/[^A-Z0-9]/g, "");
  if (c.length !== 8) return null;
  for (const ch of c) if (!CODE_CHARS.includes(ch)) return null;
  return c;
}
function newCode() {
  const r = crypto.getRandomValues(new Uint8Array(8));
  let c = "";
  for (const x of r) c += CODE_CHARS[x % CODE_CHARS.length];
  return c;
}
async function sha(s) {
  return hex(await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s)));
}
async function sign(env, what) {
  const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(env.MOD_SECRET), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  return hex(await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(what))).slice(0, 32);
}
async function verify(env, what, token) {
  const good = await sign(env, what);
  if (token.length !== good.length) return false;
  let d = 0;
  for (let i = 0; i < good.length; i++) d |= good.charCodeAt(i) ^ token.charCodeAt(i);
  return d === 0;
}
function page(title, body, status = 200) {
  return new Response(
    `<!doctype html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><title>${esc(title)}</title>` +
      `<style>body{font:16px system-ui;background:#0c0c0e;color:#eee;max-width:560px;margin:2em auto;padding:0 1em}a{color:#8cf}img{max-width:100%;border-radius:10px;margin:.6em 0}` +
      `button{font:inherit;padding:.6em 1.4em;border:0;border-radius:8px;cursor:pointer;margin-right:.6em;color:#fff}.ok{background:#2f8f64}.no{background:#c44}.s{color:#aaa}li{margin:.5em 0}</style>${body}`,
    { status, headers: { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" } }
  );
}

// ---------------------------------------------------------------- storage layout (R2 bucket THEMES)
//   z/<CODE>  the zip        p/<CODE>  preview.jpg        m/<CODE>  info (json, the truth about status)
//   idx/<state>/<9999999999999 - time>-<CODE>  empty objects with the name in their metadata, for listing newest first
const getMeta = async (env, code) => {
  const o = await env.THEMES.get("m/" + code);
  return o ? JSON.parse(await o.text()) : null;
};
const idxKey = (state, meta) => `idx/${state}/${String(9999999999999 - meta.ts).padStart(13, "0")}-${meta.code}`;
async function setStatus(env, meta, status) {
  const old = meta.status;
  if (old && old !== status) await env.THEMES.delete(idxKey(old, meta));
  meta.status = status;
  if (status === "pending" || status === "approved" || status === "held") {
    await env.THEMES.put(idxKey(status, meta), "", { customMetadata: { name: meta.name, author: meta.author, size: String(meta.size), ts: String(meta.ts) } });
  } else {
    await env.THEMES.delete(["z/" + meta.code, "p/" + meta.code]);
  }
  await env.THEMES.put("m/" + meta.code, JSON.stringify(meta));
}
async function allow(env, kind, ip, max, windowSec) {
  const id = env.LIMITER.idFromName(kind + ":" + (await sha(ip)).slice(0, 16));
  const r = await env.LIMITER.get(id).fetch(`https://limit/?max=${max}&win=${windowSec}`);
  return r.ok;
}

// ---------------------------------------------------------------- reading a theme zip (stored or deflated)
async function inflateCapped(data, cap) {
  const reader = new Blob([data]).stream().pipeThrough(new DecompressionStream("deflate-raw")).getReader();
  const parts = [];
  let n = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    n += value.length;
    if (n > cap) {
      await reader.cancel();
      return null;
    }
    parts.push(value);
  }
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}
function entryData(b, dv, en) {
  const o = en.off;
  if (o + 30 > b.length || dv.getUint32(o, true) !== 0x04034b50) return null;
  const start = o + 30 + dv.getUint16(o + 26, true) + dv.getUint16(o + 28, true);
  if (start + en.csize > b.length) return null;
  return b.subarray(start, start + en.csize);
}
async function extract(b, dv, en, cap) {
  const raw = entryData(b, dv, en);
  if (!raw || en.usize > cap) return null;
  if (en.method === 0) return en.csize === en.usize ? raw : null;
  const out = await inflateCapped(raw, cap);
  return out && out.length === en.usize ? out : null;
}
const MAGIC = { png: [0x89, 0x50, 0x4e, 0x47], jpg: [0xff, 0xd8, 0xff], jpeg: [0xff, 0xd8, 0xff], bmp: [0x42, 0x4d], gif: [0x47, 0x49, 0x46] };
async function checkZip(b) {
  const bad = (m) => ({ error: m });
  if (b.length < 22 || b[0] !== 0x50 || b[1] !== 0x4b) return bad("that is not a zip file");
  const dv = new DataView(b.buffer, b.byteOffset, b.length);
  let e = -1;
  for (let i = b.length - 22; i >= Math.max(0, b.length - 22 - 65535); i--) if (dv.getUint32(i, true) === 0x06054b50) { e = i; break; }
  if (e < 0) return bad("that zip is damaged");
  const count = dv.getUint16(e + 10, true), cdSize = dv.getUint32(e + 12, true), cdOff = dv.getUint32(e + 16, true);
  if (count === 0 || count > MAX_ENTRIES) return bad("a theme has at most " + MAX_ENTRIES + " files");
  if (cdOff + cdSize > e) return bad("that zip is damaged");
  const entries = new Map();
  let p = cdOff, unpacked = 0;
  for (let i = 0; i < count; i++) {
    if (p + 46 > b.length || dv.getUint32(p, true) !== 0x02014b50) return bad("that zip is damaged");
    const flags = dv.getUint16(p + 8, true), method = dv.getUint16(p + 10, true);
    const csize = dv.getUint32(p + 20, true), usize = dv.getUint32(p + 24, true);
    const nlen = dv.getUint16(p + 28, true), elen = dv.getUint16(p + 30, true), clen = dv.getUint16(p + 32, true);
    const off = dv.getUint32(p + 42, true);
    const name = new TextDecoder().decode(b.subarray(p + 46, p + 46 + nlen));
    p += 46 + nlen + elen + clen;
    if (flags & 1) return bad("encrypted zips are not allowed");
    if (method !== 0 && method !== 8) return bad("unsupported zip");
    if (!NAME_OK.test(name)) return bad("not allowed in a gallery theme: " + clean(name, 40));
    if (entries.has(name)) return bad("that zip is damaged");
    unpacked += usize;
    if (unpacked > MAX_UNPACKED) return bad("that theme is too big once unpacked");
    entries.set(name, { method, csize, usize, off });
  }
  if (!entries.has("theme.txt") || !entries.has("preview.jpg")) return bad("a theme needs theme.txt and preview.jpg");
  const txt = await extract(b, dv, entries.get("theme.txt"), MAX_THEME_TXT);
  if (!txt) return bad("theme.txt is damaged");
  const t = new TextDecoder().decode(txt);
  if (!t.startsWith("fulldex_theme\t1")) return bad("that is not a Fulldex theme");
  const preview = await extract(b, dv, entries.get("preview.jpg"), MAX_PREVIEW);
  if (!preview || preview.length < 8 || preview[0] !== 0xff || preview[1] !== 0xd8 || preview[2] !== 0xff) return bad("preview.jpg is not a JPEG under 2 MB");
  for (const [n, en] of entries) {
    const ext = n.slice(n.lastIndexOf(".") + 1).toLowerCase();
    if (!n.startsWith("pictures/") || en.method !== 0 || !MAGIC[ext]) continue;
    const d = entryData(b, dv, en);
    if (!d || !MAGIC[ext].every((x, i) => d[i] === x)) return bad("a picture is not what its name says: " + clean(n, 40));
  }
  const nm = t.match(/^name\t(.*)$/m);
  return { name: clean(nm ? nm[1] : "", 40) || "Untitled", preview };
}

// ---------------------------------------------------------------- Discord
async function discord(env, base, meta, kind, extra = {}) {
  if (!env.DISCORD_WEBHOOK) return false;
  const code = meta.code;
  const link = async (a, label) => `[${label}](${base}/v2/mod/${code}/${a}?t=${await sign(env, code + ":" + a)})`;
  const actions =
    kind === "new"
      ? [await link("approve", "Approve"), await link("reject", "Reject"), await link("zip", "Download zip")]
      : [await link("remove", "Remove"), await link("approve", "Keep"), await link("zip", "Download zip")];
  actions.push(`[Queue](${base}/v2/mod/queue?t=${await sign(env, "queue")})`);
  const embed = {
    title: kind === "new" ? "New theme to review" : "Theme reported",
    description:
      `**${md(meta.name)}** by ${md(meta.author)}\nCode ${fmt(code)} · ${kb(meta.size)}` +
      (extra.reason ? `\nReason: ${md(extra.reason)}` : "") +
      (extra.note ? `\n${extra.note}` : ""),
    color: kind === "new" ? 0x4aa3ff : 0xff8800,
    fields: [{ name: "Actions", value: actions.join("  ·  ") }],
  };
  if (extra.preview) embed.image = { url: "attachment://preview.jpg" };
  const payload = { username: "Fulldex Gallery", allowed_mentions: { parse: [] }, embeds: [embed] };
  let init;
  if (extra.preview) {
    const fd = new FormData();
    fd.append("payload_json", JSON.stringify(payload));
    fd.append("files[0]", new Blob([extra.preview], { type: "image/jpeg" }), "preview.jpg");
    init = { method: "POST", body: fd };
  } else {
    init = { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(payload) };
  }
  try {
    return (await fetch(env.DISCORD_WEBHOOK, init)).ok;
  } catch (e) {
    return false;
  }
}

// ---------------------------------------------------------------- /v2 routes
async function v2(request, env, url) {
  const p = url.pathname.split("/").filter(Boolean); // ["v2", "themes" | "mod", ...]
  const method = request.method;
  if (!env.THEMES || !env.MOD_SECRET) return text("the gallery is not set up yet\n", 503);
  if (p[1] === "themes") {
    if (p.length === 2) {
      if (method === "GET") return list(env, url);
      if (method === "POST") return upload(request, env, url);
      return nf();
    }
    const code = normCode(p[2]);
    if (!code) return nf();
    const sub = p[3];
    if (!sub && method === "GET") return download(env, code, "z/", "application/zip");
    if (!sub && method === "DELETE") return ownerDelete(env, code, url);
    if (sub === "preview" && method === "GET") return download(env, code, "p/", "image/jpeg");
    if (sub === "status" && method === "GET") return status(env, code);
    if (sub === "report" && method === "POST") return report(request, env, url, code);
    return nf();
  }
  if (p[1] === "mod") return mod(request, env, url, p);
  return nf();
}

async function list(env, url) {
  const limit = Math.min(48, Math.max(1, parseInt(url.searchParams.get("limit") || "24", 10) || 24));
  const r = await env.THEMES.list({ prefix: "idx/approved/", limit, cursor: url.searchParams.get("cursor") || undefined, include: ["customMetadata"] });
  const themes = r.objects.map((o) => {
    const md = o.customMetadata || {};
    return { code: fmt(o.key.slice(-8)), name: md.name || "", author: md.author || "", ts: +md.ts || 0, size: +md.size || 0 };
  });
  return json({ themes, cursor: r.truncated ? r.cursor : null }, 200, { "Cache-Control": "public, max-age=60" });
}

async function upload(request, env, url) {
  const ip = request.headers.get("CF-Connecting-IP") || "local";
  if (!(await allow(env, "up", ip, +env.UPLOAD_LIMIT || 5, 3600))) return text("too many uploads from here, try again later\n", 429);
  const cl = parseInt(request.headers.get("Content-Length") || "0", 10);
  if (!cl) return text("send the zip with its length\n", 411);
  if (cl > MAX_ZIP) return text("over 20 MB\n", 413);
  const buf = new Uint8Array(await request.arrayBuffer());
  if (buf.length > MAX_ZIP) return text("over 20 MB\n", 413);
  const info = await checkZip(buf);
  if (info.error) return text(info.error + "\n", 400);
  let code = newCode();
  for (let i = 0; i < 5 && (await env.THEMES.head("m/" + code)); i++) code = newCode();
  const meta = { code, name: info.name, author: clean(url.searchParams.get("author"), 24) || "Anonymous", ts: Date.now(), size: buf.length, reports: [] };
  await env.THEMES.put("z/" + code, buf);
  await env.THEMES.put("p/" + code, info.preview, { httpMetadata: { contentType: "image/jpeg" } });
  await setStatus(env, meta, "pending");
  await discord(env, baseUrl(env, url), meta, "new", { preview: info.preview });
  return json({ code: fmt(code), status: "pending", owner: await sign(env, code + ":own") }, 201);
}

async function download(env, code, prefix, type) {
  const meta = await getMeta(env, code);
  if (!meta || meta.status !== "approved") return nf();
  const o = await env.THEMES.get(prefix + code);
  if (!o) return nf();
  return new Response(o.body, { headers: { "Content-Type": type, "Cache-Control": "public, max-age=3600", "X-Theme-Name": encodeURIComponent(meta.name) } });
}

async function status(env, code) {
  const meta = await getMeta(env, code);
  if (!meta) return nf();
  return json({ status: meta.status === "held" ? "pending" : meta.status });
}

async function ownerDelete(env, code, url) {
  if (!(await verify(env, code + ":own", url.searchParams.get("o") || ""))) return text("no\n", 403);
  const meta = await getMeta(env, code);
  if (!meta) return nf();
  await setStatus(env, meta, "removed");
  return json({ status: "removed" });
}

async function report(request, env, url, code) {
  const meta = await getMeta(env, code);
  if (!meta || meta.status !== "approved") return nf();
  const ip = request.headers.get("CF-Connecting-IP") || "local";
  if (!(await allow(env, "rep", ip, 10, 3600))) return text("too many reports from here, try again later\n", 429);
  const reason = clean((await request.text()).slice(0, 400), 200) || "(no reason given)";
  const who = (await sha(ip)).slice(0, 12);
  meta.reports = meta.reports || [];
  if (!meta.reports.includes(who)) meta.reports.push(who);
  meta.reports = meta.reports.slice(-20);
  const hold = meta.reports.length >= (+env.REPORT_HOLD || 3);
  if (hold) await setStatus(env, meta, "held");
  else await env.THEMES.put("m/" + code, JSON.stringify(meta));
  await discord(env, baseUrl(env, url), meta, "report", {
    reason,
    note: hold ? `Hidden from the gallery after ${meta.reports.length} reports until you decide.` : `${meta.reports.length} report(s) so far.`,
  });
  return json({ ok: true });
}

async function mod(request, env, url, p) {
  const t = url.searchParams.get("t") || "";
  const base = baseUrl(env, url);
  if (p[2] === "queue") {
    if (request.method !== "GET") return nf();
    if (!(await verify(env, "queue", t))) return text("bad link\n", 403);
    const items = [];
    for (const st of ["pending", "held"]) {
      const r = await env.THEMES.list({ prefix: `idx/${st}/`, limit: 100, include: ["customMetadata"] });
      for (const o of r.objects) items.push({ code: o.key.slice(-8), st, md: o.customMetadata || {} });
    }
    let rows = "";
    for (const it of items) {
      const a = await sign(env, it.code + ":approve"), r = await sign(env, it.code + ":reject");
      rows += `<li><b>${esc(it.md.name || "")}</b> by ${esc(it.md.author || "")} <span class=s>${fmt(it.code)} ${it.st === "held" ? "(reported)" : ""}</span><br>` +
        `<a href="${base}/v2/mod/${it.code}/approve?t=${a}">Approve</a> · <a href="${base}/v2/mod/${it.code}/reject?t=${r}">Reject</a></li>`;
    }
    return page("Theme queue", `<h2>Theme queue (${items.length})</h2><ul>${rows || "<li class=s>Nothing waiting.</li>"}</ul>`);
  }
  const code = normCode(p[2]);
  const action = p[3];
  if (!code || !["approve", "reject", "remove", "zip", "preview"].includes(action)) return nf();
  if (!(await verify(env, code + ":" + action, t))) return text("bad link\n", 403);
  const meta = await getMeta(env, code);
  if (!meta) return nf();
  if (request.method === "GET") {
    if (action === "zip" || action === "preview") {
      const o = await env.THEMES.get((action === "zip" ? "z/" : "p/") + code);
      if (!o) return nf();
      return new Response(o.body, {
        headers: action === "zip"
          ? { "Content-Type": "application/zip", "Content-Disposition": `attachment; filename="${code}.zip"`, "Cache-Control": "no-store" }
          : { "Content-Type": "image/jpeg", "Cache-Control": "no-store" },
      });
    }
    const pv = `${base}/v2/mod/${code}/preview?t=${await sign(env, code + ":preview")}`;
    const label = { approve: "Approve", reject: "Reject", remove: "Remove" }[action];
    return page(label + " theme",
      `<h2>${esc(meta.name)}</h2><p class=s>by ${esc(meta.author)} · ${fmt(code)} · ${kb(meta.size)} · now: ${esc(meta.status)}</p>` +
      `<img src="${pv}" alt=""><form method=post action=""><button class="${action === "approve" ? "ok" : "no"}">${label}</button></form>`);
  }
  if (request.method !== "POST") return nf();
  if (action === "zip" || action === "preview") return nf();
  if (action === "approve") {
    if (!(await env.THEMES.head("z/" + code))) return page("Cannot approve", "<p>That theme was already rejected or removed.</p>", 409);
    meta.reports = [];
    await setStatus(env, meta, "approved");
    return page("Approved", `<h2>Approved</h2><p>${esc(meta.name)} (${fmt(code)}) is in the gallery.</p>`);
  }
  await setStatus(env, meta, action === "reject" ? "rejected" : "removed");
  return page("Done", `<h2>${action === "reject" ? "Rejected" : "Removed"}</h2><p>${esc(meta.name)} (${fmt(code)}) is gone.</p>`);
}

// ---------------------------------------------------------------- Durable Objects
function join(a, b) {
  const out = new Uint8Array(a.length + b.length);
  out.set(a, 0);
  out.set(b, a.length);
  return out;
}

// Counts how often one address did one thing in a time window (uploads, reports).
export class Limiter {
  constructor(state) {
    this.state = state;
  }
  async fetch(request) {
    const u = new URL(request.url);
    const max = +u.searchParams.get("max"), win = +u.searchParams.get("win") * 1000;
    const now = Date.now();
    let s = (await this.state.storage.get("s")) || { start: now, n: 0 };
    if (now - s.start >= win) s = { start: now, n: 0 };
    if (s.n >= max) return new Response("no", { status: 429 });
    s.n++;
    await this.state.storage.put("s", s);
    await this.state.storage.setAlarm(s.start + win + 1000);
    return new Response("ok");
  }
  async alarm() {
    await this.state.storage.deleteAll();
  }
}

export class Room {
  constructor(state) {
    this.state = state;
  }
  async fetch(request) {
    const me = new URL(request.url).pathname.split("/")[3];
    const s = this.state.storage;
    const body = new Uint8Array(await request.arrayBuffer());
    if (body.length > MAX_BODY) return new Response("too big\n", { status: 413 });
    let members = (await s.get("members")) || [];
    // A goodbye alone (one BYE frame: length 1, type 7): that device leaves, and its messages go.
    if (body.length === 5 && body[0] === 1 && body[1] === 0 && body[2] === 0 && body[3] === 0 && body[4] === 7) {
      members = members.filter((x) => x !== me);
      await s.put("members", members);
      await s.delete(["out:" + me, "read:" + me]);
      for (const x of members) await s.delete("read:" + x);
      if (!members.length) await s.deleteAll();
      return new Response(new Uint8Array([members.length]), { headers: { "Content-Type": "application/octet-stream" } });
    }
    if (!members.includes(me)) {
      if (members.length >= 2) return new Response("room full\n", { status: 409 });
      members.push(me);
      await s.put("members", members);
    }
    // What each device has said (out:<id>), and how much of the other's each has read (read:<id>).
    let mine = (await s.get("out:" + me)) || new Uint8Array(0);
    if (body.length) {
      if (mine.length + body.length > MAX_ROOM) return new Response("room full\n", { status: 413 });
      mine = join(mine, body);
      await s.put("out:" + me, mine);
    }
    const other = members.find((x) => x !== me);
    let news = new Uint8Array(0);
    if (other) {
      const theirs = (await s.get("out:" + other)) || new Uint8Array(0);
      const read = (await s.get("read:" + me)) || 0;
      news = theirs.slice(read);
      await s.put("read:" + me, theirs.length);
    }
    await s.setAlarm(Date.now() + FORGET_MS);
    return new Response(join(new Uint8Array([members.length]), news), {
      headers: { "Content-Type": "application/octet-stream", "Cache-Control": "no-store" },
    });
  }
  async alarm() {
    await this.state.storage.deleteAll();
  }
}

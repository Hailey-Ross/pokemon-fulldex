// Tests the Theme Gallery routes against `wrangler dev` (see README). Starts a pretend Discord on port 9911.
import http from "node:http";
import zlib from "node:zlib";

const base = process.argv[2] || "http://127.0.0.1:8787";
let fails = 0;
const ok = (c, label) => { console.log((c ? "PASS " : "FAIL ") + label); if (!c) fails++; };

// A pretend Discord: keeps every message it gets.
const msgs = [];
const hook = http.createServer((req, res) => {
  const c = [];
  req.on("data", (d) => c.push(d));
  req.on("end", () => { msgs.push(Buffer.concat(c).toString("latin1")); res.statusCode = 204; res.end(); });
}).listen(9911, "127.0.0.1");

const T = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = (b) => { let c = ~0; for (const x of b) c = T[(c ^ x) & 255] ^ (c >>> 8); return ~c >>> 0; };
function zip(files, deflateThemeTxt = false) {
  const parts = [], cd = [];
  let off = 0;
  for (const [name, data] of files) {
    const nb = Buffer.from(name), method = deflateThemeTxt && name === "theme.txt" ? 8 : 0;
    const body = method ? zlib.deflateRawSync(data) : data;
    const h = Buffer.alloc(30);
    h.writeUInt32LE(0x04034b50, 0); h.writeUInt16LE(20, 4); h.writeUInt16LE(0x0800, 6); h.writeUInt16LE(method, 8);
    h.writeUInt16LE(0x21, 12); h.writeUInt32LE(crc(data), 14); h.writeUInt32LE(body.length, 18); h.writeUInt32LE(data.length, 22); h.writeUInt16LE(nb.length, 26);
    const c = Buffer.alloc(46);
    c.writeUInt32LE(0x02014b50, 0); c.writeUInt16LE(20, 4); c.writeUInt16LE(20, 6); c.writeUInt16LE(0x0800, 8); c.writeUInt16LE(method, 10);
    c.writeUInt16LE(0x21, 14); c.writeUInt32LE(crc(data), 16); c.writeUInt32LE(body.length, 20); c.writeUInt32LE(data.length, 24); c.writeUInt16LE(nb.length, 28); c.writeUInt32LE(off, 42);
    parts.push(h, nb, body);
    cd.push(c, nb);
    off += 30 + nb.length + body.length;
  }
  const cdb = Buffer.concat(cd), e = Buffer.alloc(22);
  e.writeUInt32LE(0x06054b50, 0); e.writeUInt16LE(files.length, 8); e.writeUInt16LE(files.length, 10); e.writeUInt32LE(cdb.length, 12); e.writeUInt32LE(off, 16);
  return Buffer.concat([...parts, cdb, e]);
}
const jpg = Buffer.concat([Buffer.from([0xff, 0xd8, 0xff, 0xe0]), Buffer.alloc(300, 7)]);
const png = Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47]), Buffer.alloc(300, 9)]);
const txt = (name) => Buffer.from(`fulldex_theme\t1\nname\t${name}\naccent\t#ff8040\n`);
const good = (name, deflate) => zip([["theme.txt", txt(name)], ["preview.jpg", jpg], ["pictures/accent-sky.png", png]], deflate);

const up = (body, ip) => fetch(`${base}/v2/themes?author=Tester`, { method: "POST", body, headers: ip ? { "CF-Connecting-IP": ip } : {} });
const get = (path, init) => fetch(path.startsWith("http") ? path : base + path, init);
const names = async () => (await (await get("/v2/themes")).json()).themes.map((t) => t.name);
const link = (code, action) => {
  for (const m of msgs) { const r = m.match(new RegExp(`${base.replace(/[.]/g, "\\.")}/v2/mod/${code}/${action}\\?t=[0-9a-f]+`)); if (r) return r[0]; }
  return null;
};

// Uploads that must be refused (these also use up the upload allowance of 7 an hour).
ok((await up(Buffer.from("hello, not a zip"))).status === 400, "not a zip is refused");
ok((await up(zip([["preview.jpg", jpg]]))).status === 400, "zip without theme.txt is refused");
ok((await up(zip([["theme.txt", txt("x")], ["preview.jpg", jpg], ["fonts/a.ttf", Buffer.alloc(50)]]))).status === 400, "a font in the zip is refused");
ok((await up(Buffer.alloc(21 * 1024 * 1024, 1)).catch(() => ({ status: 413 }))).status === 413, "over 20 MB is refused");
ok((await up(zip([["theme.txt", Buffer.from("hello\n")], ["preview.jpg", jpg]]))).status === 400, "not a Fulldex theme is refused");
const ra = await up(good("Sunset Glass", false));
const A = await ra.json();
ok(ra.status === 201 && /^[A-Z0-9]{4}-[A-Z0-9]{4}$/.test(A.code) && A.status === "pending" && A.owner, "good theme A uploaded, pending, has a code and an owner token");
const rb = await up(good("Night Mode", true));
const B = await rb.json();
ok(rb.status === 201, "good theme B (deflated theme.txt) uploaded");
ok((await up(good("One Too Many", false))).status === 429, "the 8th upload in an hour is turned away");

const a = A.code.replace("-", ""), b = B.code.replace("-", "");
await new Promise((r) => setTimeout(r, 300));
ok(msgs.length === 2 && msgs[0].includes("Sunset Glass") && msgs[0].includes("preview.jpg") && msgs[0].includes(A.code), "Discord got both uploads, with preview and code");
ok(link(a, "approve") && link(a, "reject") && link(b, "approve") && link(b, "reject"), "Discord messages carry Approve and Reject links");

// Pending: not public.
ok((await (await get(`/v2/themes/${a}/status`)).json()).status === "pending", "A is pending");
ok((await get(`/v2/themes/${a}`)).status === 404, "pending A cannot be downloaded");
ok((await names()).length === 0, "the gallery list is empty");

// A link alone does nothing; the button (POST) does.
const ga = await get(link(a, "approve"));
ok(ga.status === 200 && (await ga.text()).includes("Sunset Glass"), "approve link shows a confirmation page");
ok((await (await get(`/v2/themes/${a}/status`)).json()).status === "pending", "opening the link did not approve");
ok((await get(link(a, "approve").replace(/t=.{4}/, "t=0000"), { method: "POST" })).status === 403, "a wrong token is refused");
ok((await get(link(a, "approve"), { method: "POST" })).status === 200, "POST approves A");
ok((await (await get(`/v2/themes/${a}/status`)).json()).status === "approved", "A is approved");
const dl = await get(`/v2/themes/${a.toLowerCase().slice(0, 4)}-${a.toLowerCase().slice(4)}`);
ok(dl.status === 200 && Buffer.compare(Buffer.from(await dl.arrayBuffer()), good("Sunset Glass", false)) === 0, "A downloads byte for byte (code with dash and lower case)");
const pv = await get(`/v2/themes/${a}/preview`);
ok(pv.status === 200 && pv.headers.get("content-type") === "image/jpeg" && Buffer.compare(Buffer.from(await pv.arrayBuffer()), jpg) === 0, "A's preview is the preview.jpg");
const l1 = await (await get("/v2/themes")).json();
ok(l1.themes.length === 1 && l1.themes[0].name === "Sunset Glass" && l1.themes[0].author === "Tester" && l1.themes[0].code === A.code, "list shows A with name, author and code");

// Queue and reject.
const q = link(a, "approve") && msgs[0].match(new RegExp(`${base.replace(/[.]/g, "\\.")}/v2/mod/queue\\?t=[0-9a-f]+`))[0];
const qt = await (await get(q)).text();
ok(qt.includes("Night Mode") && !qt.includes("Sunset Glass"), "the queue lists B only");
ok((await get(link(b, "reject"), { method: "POST" })).status === 200, "POST rejects B");
ok((await (await get(`/v2/themes/${b}/status`)).json()).status === "rejected" && (await get(`/v2/themes/${b}`)).status === 404, "B is rejected and gone");
ok((await get(link(b, "approve"), { method: "POST" })).status === 409, "a rejected theme cannot be approved");
const zl = await get(link(a, "zip"));
ok(zl.status === 200 && Buffer.from(await zl.arrayBuffer()).length === good("Sunset Glass", false).length, "moderator zip link works");

// Reports: 3 different people hide it.
const rep = (i, why) => get(`/v2/themes/${a}/report`, { method: "POST", body: why, headers: { "CF-Connecting-IP": "10.0.0." + i } });
ok((await rep(1, "rude")).status === 200 && (await names()).length === 1, "1 report keeps it listed");
await rep(1, "again");
ok((await names()).length === 1, "the same reporter twice still counts once");
await rep(2, "spam");
await rep(3, "bad");
ok((await names()).length === 0 && (await get(`/v2/themes/${a}`)).status === 404, "3 reporters hide it");
await new Promise((r) => setTimeout(r, 300));
const reports = msgs.filter((m) => m.includes("Theme reported"));
ok(reports.length === 4 && link(a, "remove") && reports[reports.length - 1].includes("Hidden from the gallery"), "Discord got the reports with a Remove link");
ok((await (await get(`/v2/themes/${a}/status`)).json()).status === "pending", "a hidden theme reads as pending");
ok((await get(link(a, "approve"), { method: "POST" })).status === 200 && (await names()).length === 1, "Keep (approve) puts it back");

// The uploader takes it down.
ok((await get(`/v2/themes/${a}?o=nope`, { method: "DELETE" })).status === 403, "wrong owner token is refused");
ok((await get(`/v2/themes/${a}?o=${A.owner}`, { method: "DELETE" })).status === 200 && (await names()).length === 0, "the owner can take it down");
ok((await (await get(`/v2/themes/${a}/status`)).json()).status === "removed", "it reads as removed");

// The trade rooms still work.
const tr = await get("/v1/TESTROOM/aaaaaaaa1", { method: "POST", body: new Uint8Array(0) });
ok(tr.status === 200 && new Uint8Array(await tr.arrayBuffer())[0] === 1, "trade room still answers");
ok((await get("/nothing")).status === 404, "unknown path is 404");

console.log(fails ? `${fails} FAILED` : "ALL PASSED");
hook.close();
process.exit(fails ? 1 : 0);

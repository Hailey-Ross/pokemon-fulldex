// Uploads every .zip in a folder to the local relay and approves them (except names with "waiting").
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
const base = "http://127.0.0.1:8787", dir = process.argv[2];
const msgs = [];
const hook = http.createServer((req, res) => {
  const c = [];
  req.on("data", (d) => c.push(d));
  req.on("end", () => { msgs.push(Buffer.concat(c).toString("latin1")); res.statusCode = 204; res.end(); });
}).listen(9911, "127.0.0.1");
let i = 0;
for (const f of fs.readdirSync(dir).filter((f) => f.toLowerCase().endsWith(".zip"))) {
  const r = await fetch(`${base}/v2/themes?author=${encodeURIComponent("Tester " + ++i)}`, { method: "POST", body: fs.readFileSync(path.join(dir, f)) });
  if (r.status !== 201) { console.log("UPLOADFAIL\t" + f + "\t" + r.status + "\t" + (await r.text()).trim()); continue; }
  const j = await r.json(), code = j.code.replace("-", "");
  await new Promise((x) => setTimeout(x, 250));
  const pending = /waiting/i.test(f);
  if (!pending) {
    const m = msgs.map((x) => x.match(new RegExp(`/v2/mod/${code}/approve\\?t=[0-9a-f]+`))).find(Boolean);
    await fetch(base + m[0], { method: "POST" });
  }
  console.log(`${pending ? "PENDING" : "APPROVED"}\t${f}\t${j.code}`);
}
hook.close();
process.exit(0);

// Boots the exported web build in headless chromium and requires the
// GDScript smoke to print its final verdict to the JS console.
import { execFileSync, spawn } from "node:child_process";

import { existsSync } from "node:fs";
const chromium = process.env.CHROMIUM_BIN ||
  ["/usr/bin/google-chrome", "/usr/bin/google-chrome-stable",
   "/usr/bin/chromium-browser", "/usr/bin/chromium"].find(existsSync) ||
  "chromium";
const url = "http://127.0.0.1:8971/index.html";
const timeoutMs = 120000;

// chromium headless with console piped via --enable-logging.
const proc = spawn(chromium, [
  "--headless=new", "--no-sandbox", "--disable-gpu-sandbox",
  "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
  "--enable-logging=stderr", "--v=0",
  url,
]);

let output = "";
const done = new Promise((resolve) => {
  const check = (chunk) => {
    output += chunk;
    if (output.includes("WEB SMOKE OK") || output.includes("WEB SMOKE FAIL")) {
      resolve();
    }
  };
  proc.stderr.on("data", check);
  proc.stdout.on("data", check);
  proc.on("error", (e) => { output += String(e); resolve(); });
  proc.on("exit", resolve);
  setTimeout(resolve, timeoutMs + 15000);
});
await done;
proc.kill();

const ok = output.includes("WEB SMOKE OK");
const lines = output.split("\n").filter(l => l.includes("WEB SMOKE") || l.includes("rivegd"));
console.log(lines.join("\n") || "(no smoke output captured)");
process.exit(ok ? 0 : 1);

// Dynamic-link ABI gate: every function the side module imports must be
// resolvable at load — from the engine wasm's exports, the engine JS
// library (present in index.js), or weak self-resolution (the side module
// defines it too). Catches Emscripten version/flag drift before a browser
// ever runs (e.g. emscripten_longjmp, SIMD-typed trampoline signatures).
import { readFileSync } from "node:fs";

const side = await WebAssembly.compile(
  readFileSync("addons/rive/bin/web/librivegd.web.template_release.wasm32.nothreads.wasm"));
const main = await WebAssembly.compile(readFileSync("out/web/index.wasm"));
const js = readFileSync("out/web/index.js", "utf8");

const mainExports = new Set(WebAssembly.Module.exports(main).map(e => e.name));
const sideExports = new Set(WebAssembly.Module.exports(side).map(e => e.name));

// rive's optional JS hooks: WebGL pixel-local-storage extension entry
// points, their async image decoder, and browser sniffing. The dynamic
// loader stubs these and the guarded code paths never call them in a
// Godot host (verified: the browser smoke passes with all of them
// unresolved). Anything OUTSIDE this list is real ABI drift.
const KNOWN_BENIGN = new Set([
  "enable_WEBGL_shader_pixel_local_storage_coherent",
  "framebufferTexturePixelLocalStorageWEBGL",
  "framebufferPixelLocalClearValuefvWEBGL",
  "beginPixelLocalStorageWEBGL",
  "endPixelLocalStorageWEBGL",
  "getFramebufferPixelLocalStorageParameterivWEBGL",
  "enable_WEBGL_provoking_vertex",
  "provokingVertexWEBGL",
  "wasm_start_image_decode",
  "isWindowsBrowser",
]);

const unresolved = WebAssembly.Module.imports(side)
  .filter(i => i.kind === "function")
  .map(i => i.name)
  .filter(n =>
    !mainExports.has(n) &&
    !sideExports.has(n) && // weak dylink self-resolution
    !n.startsWith("invoke_") &&
    !KNOWN_BENIGN.has(n) &&
    !js.includes(`_${n}:`) && !js.includes(`${n}:`) && !js.includes(`"${n}"`));

if (unresolved.length > 0) {
  console.error(`ABI check FAILED: ${unresolved.length} unresolvable imports:`);
  console.error(unresolved.slice(0, 20).join("\n"));
  process.exit(1);
}
console.log("ABI check OK: all side-module imports resolvable");

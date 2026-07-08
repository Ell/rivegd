# Rive ⇄ Godot Integration — Implementation Strategy

> **Status (July 2026):** the strategy below was executed and held up.
> Shipped: Vulkan zero-copy path, CommandQueue lifetime model (M0+M1;
> M2/M3 intentionally dropped after auditing the listener surface — see
> `commandqueue-migration.md`), one-flush-per-frame pump with adaptive
> chunked submission, full API surface, and the web (WebGL2/wasm dlink)
> port. The GL bridge's desktop-Compatibility limitation (Godot creates
> GL 3.3 core; rive needs 4.2) was confirmed; its real targets are
> Android GLES3 and WebGL2 — the latter now working.

**Target:** Godot 4.7 stable (latest; godot-cpp master tracks `4.7.stable.official`) · **Foundation:** `rive-app/rive-runtime` (official C++ SDK, MIT) · **Packaging:** GDExtension

This strategy was derived from primary sources only: the `rive-runtime` source tree (inspected locally), Godot 4.7's `extension_api.json` / godot-cpp, Godot engine docs/proposals, and Rive's official docs — deliberately *not* from prior Rive-Godot integration attempts. Rive's Unity/Unreal integrations were consulted only as SDK-usage precedent. Claims below marked **[verified]** survived 3-vote adversarial verification against live sources (2026-07-06); claims marked **[source]** were confirmed directly in the checked-out repos.

---

## 1. The core decision: embed the Rive Renderer, GPU-native

Three rendering strategies were evaluated:

| Strategy | Verdict |
|---|---|
| A. CPU rasterize → upload texture | **Reject.** rive-runtime ships no supported CPU rasterizer — Skia is legacy and was removed from Rive's own runtimes (Android v10.0.0). Loses Vector Feathering, loses perf, loses editor fidelity. **[verified]** |
| B. Translate Rive draw commands → Godot canvas APIs | **Reject.** Rive's renderer is a geometric reduction of AA vector paths into GPU triangle patches with its own raster-ordering, clipping, and blend semantics; Godot's canvas cannot reproduce this. Feature-exclusive rendering (feathering) only exists in the Rive Renderer. **[verified]** |
| C. Drive Rive's own GPU renderer on Godot's graphics device, composite output as a texture | **Adopt.** This is how Rive's official Unreal integration works (Rive renderer driven through RHI, coordinated with the render thread) **[verified]**, and it is the only path with full feature parity and Rive-Editor visual fidelity. |

The renderer's integration surface is `rive::gpu::RenderContext` — one API-agnostic frontend with per-API `RenderContextImpl` backends: **Vulkan, Metal, D3D11, D3D12, OpenGL/GLES3/WebGL2, WebGPU** (WebGPU present in source, undocumented). **[verified + source]** This maps 1:1 onto Godot 4.7's driver matrix:

| Godot rendering method | Driver | Rive backend |
|---|---|---|
| Forward+ / Mobile (Windows, Linux, Android) | Vulkan | `RenderContextVulkanImpl` |
| Forward+ / Mobile (macOS, iOS) | Metal | `RenderContextMetalImpl` |
| Forward+ (Windows, opt-in) | D3D12 | `RenderContextD3DImpl` (12) |
| Compatibility (Android GLES3; NOT desktop) | GLES3 | `RenderContextGLImpl` (desktop Compatibility is GL 3.3 core — below rive's 4.2 desktop floor) |
| Web export (Compatibility only) | WebGL2 | `RenderContextGLImpl` (Emscripten) |

**Important refuted claim:** Godot's `RenderingDevice` is *not* an RHI-style host that hands extensions raw command-stream control. The integration does not "port Rive onto RenderingDevice"; instead it bootstraps Rive's own backend on the **same native device** Godot created, renders to an offscreen target, and hands the result back as a texture.

### 1.1 The bridge, concretely (Vulkan path — verified in source on both sides)

Everything needed is already plumbed:

- **Context bootstrap:** `RenderContextVulkanImpl::MakeContext(VkInstance, VkPhysicalDevice, VkDevice, VulkanFeatures, PFN_vkGetInstanceProcAddr)` **[source: `renderer/include/rive/renderer/vulkan/render_context_vulkan_impl.hpp`]**. Godot exposes exactly these handles via `RenderingDevice.get_driver_resource()`: `DRIVER_RESOURCE_TOPMOST_OBJECT` (VkInstance), `_PHYSICAL_DEVICE`, `_LOGICAL_DEVICE`, `_COMMAND_QUEUE`, `_QUEUE_FAMILY` **[source: extension_api.json 4.7]**. `vkGetInstanceProcAddr` is obtained by loading the Vulkan loader ourselves.
- **Render target:** create the offscreen texture with `RenderingDevice.texture_create()` (usage: sampling + color attachment + storage as required), pull its `VkImage`/`VkImageView` via `get_driver_resource(DRIVER_RESOURCE_VULKAN_IMAGE / _IMAGE_VIEW)`, and wrap with `RenderTargetVulkanImpl::setTargetImageView(imageView, image, lastAccess)` **[source]**. Rive's `RenderTargetVulkan` carries explicit image-access/barrier bookkeeping (`accessTargetImage`, `updateLastAccess`), so layout transitions between Rive's writes and Godot's sampling are a first-class API, not a hack.
- **Scene composition:** expose that RD texture through `Texture2DRD` (shipped Godot 4.2, designed explicitly so custom-GPU-written textures work "in any material, node, etc.") **[verified: godot PR #79288 / proposal #6964]**. Zero copies, usable in CanvasItems, Controls, 3D materials alike.

The GL path is symmetric: `TextureRenderTargetGL` wraps a raw GL texture id **[source]**, and Godot's `RenderingServer.texture_get_native_handle()` provides one under the Compatibility renderer.

### 1.2 Frame synchronization (the hard part — plan for it)

A verified **refutation**: custom GPU work on the main RenderingDevice has **no guarantee** of executing before the same frame's scene draw. Synchronization is our responsibility. The plan:

- Run Rive's `flush()` on **Godot's render thread** via `RenderingServer.call_on_render_thread()`, before the canvas pass consumes the texture. Inside that callback we are on the thread that owns the queue, so allocating our own command pool (Godot's queue family) and submitting to Godot's `VkQueue` is same-thread-serialized with Godot's own submissions; same-queue submission order then guarantees Rive's writes land before the scene draw that samples the target.
- Rive's `RenderTargetVulkan` access API transitions the image to `SHADER_READ_ONLY_OPTIMAL` at the end of a flush, matching what Godot's sampler expects; `updateLastAccess` keeps Rive's barrier state coherent across frames.
- Note Godot 4.4+ added thread guards on main-device texture functions — all RD texture manipulation must happen on the render thread, which the `call_on_render_thread` design already satisfies.

This area carries the most engineering risk and is deliberately the *first* thing the roadmap proves out.

## 2. Threading & lifetime model

rive-runtime now ships an official multithreaded façade: **`CommandQueue` / `CommandServer`** **[source: `include/rive/command_queue.hpp`]**. The game thread holds opaque handles (`FileHandle`, `ArtboardHandle`, `StateMachineHandle`, `ViewModelInstanceHandle`) and enqueues commands (`loadFile`, `instantiateArtboardNamed`, `advanceStateMachine`, `pointerMove/Down/Up`, `draw(DrawKey, callback)`, typed view-model get/set); a server thread owns every actual Rive object — including the `Factory`/`RenderContext` — and executes them.

This maps perfectly onto Godot:

- **CommandServer runs on Godot's render thread** (pumped from a `call_on_render_thread` tick). All Rive object lifetimes, GPU resources, and draws live there.
- **Godot-facing wrapper objects** (`RiveFile`, `RiveArtboard`, `RiveStateMachine`, `RiveViewModelInstance` as `RefCounted`) hold only handles. Their destructors enqueue release commands — this cleanly decouples Rive lifetimes from GDScript GC timing and makes use-after-free structurally impossible.
- Async results (loaded file metadata, view-model property reads, fired events) come back via the queue's listener callbacks → marshalled to the main thread → emitted as Godot **signals**.
- **Phase-1 simplification:** start single-threaded (advance in `_process`, flush via `call_on_render_thread`) with the same public API, then swap the internals to CommandQueue without breaking users.

**Lifetime constraint (verified):** `RenderContext` *is* the `rive::Factory` — the object passed to `File::import` owns the GPU resources. So `.riv` decoding is coupled to the live render context, which dictates the asset pipeline below.

## 3. Asset pipeline

- **`.riv` files are runtime-opaque byte blobs.** Register a `ResourceFormatLoader` (plus `EditorImportPlugin` in "keep raw bytes" mode) producing a `RiveFileResource` that stores the bytes and lightweight parsed metadata only. `File::import` against the real render-context Factory happens lazily at runtime. Do **not** decode at import time — the verified Factory/RenderContext coupling makes import-time GPU-object creation wrong by construction.
- **Editor preview & thumbnails:** the editor process has a live RenderingDevice too, so the same render path powers an `EditorResourcePreviewGenerator` thumbnail and an inspector preview panel (artboard/state-machine/animation pickers, live playback).
- **Hot reload:** `RiveFileResource` watches for reimport (`changed` signal); nodes drop handles and re-instantiate. Cheap because instantiation is already lazy.
- **Out-of-band assets** (hosted/referenced images, fonts): implement Rive's `FileAssetLoader` to resolve them through Godot resources, so referenced assets participate in Godot's import/export normally.
- **Metadata for the inspector:** on load, enumerate artboards, state machines, inputs, events, and view models (the `CommandQueue` has `requestViewModelNames`, `requestViewModelPropertyDefinitions`, etc. **[source]**) and cache in the resource for editor dropdowns without a GPU context.

## 4. Runtime API surface

Node types (all render the same `Texture2DRD` under the hood):

- **`RiveSprite2D`** (`Node2D`) — world-space 2D.
- **`RiveControl`** (`Control`) — UI: respects layout sizing (artboard size / expand modes), forwards `_gui_input` pointer events into artboard space (Rive listeners then just work), participates in focus/theme.
- **`RiveTexture`** — a `Texture2DRD`-style resource for power users: use Rive output in any material, `TextureRect`, 3D quad, particle, etc. 3D support falls out for free; no dedicated 3D node initially.

Scripting surface (GDScript and C# both get it automatically via ClassDB):

- **State machines:** `play(state_machine)`, typed input access (`set_bool/set_number/fire_trigger`), auto-generated **inspector properties** via `_get_property_list()` from cached metadata so designers tweak inputs without code.
- **Events:** Rive events (incl. audio events) surface as a `rive_event(name, properties: Dictionary)` signal; general playback signals (`playback_complete`, `state_changed`).
- **Data binding:** `RiveViewModelInstance` wraps `ViewModelInstanceRuntime` — typed, path-addressed properties (number/string/boolean/color/enum/trigger/list/image **[source]**) exposed as `get_property("path")`/`set_property()`, plus `property_changed` signals subscribable per-path. This is Rive's strategic direction (view models supersede ad-hoc input poking) and must be first-class, not an afterthought.
- **Pointer input:** `pointerMove/Down/Up` forwarded with coordinates transformed by the artboard's fit/alignment transform.

## 5. Text, layout, audio

- **Text:** build with `WITH_RIVE_TEXT`. Rive vendors HarfBuzz + SheenBidi with **symbol renames** (`rive_hb_*`, generated rename headers in `dependencies/`) specifically to avoid collisions with hosts that bundle their own HarfBuzz — Godot does. **[source]** Static-link with hidden visibility; the rename layer is a second safety net.
- **Layout:** `WITH_RIVE_LAYOUT` (vendored Yoga, also symbol-renamed) — needed for Rive layouts; enable from the start.
- **Audio:** build with `with_rive_audio=external` → `EXTERNAL_RIVE_AUDIO_ENGINE` + `MA_NO_DEVICE_IO` **[source: premake5_v2.lua]** — miniaudio mixes but owns **no OS device**. Pull PCM frames from Rive's `AudioEngine` into a custom `AudioStream`/`AudioStreamPlayback`, so Rive audio routes through Godot's audio server, buses, and effects instead of fighting it for the device.

## 6. Build system

Two-stage build, kept strictly separated:

1. **Rive stage:** `build/build_rive.sh` (premake5) per platform — host desktop, `ios`, `android` (arm64), `wasm` cross-builds with per-config output dirs **[verified]**. Products: `librive`, `librive_pls_renderer` (renderer), `librive_decoders` (PNG/JPEG/WebP, symbol-renamed libjpeg), renamed HarfBuzz/SheenBidi/Yoga, miniaudio. Pin a rive-runtime commit as a git submodule; wrap invocation in a small script that normalizes flags (e.g. `--with_rive_text --with_rive_layout --with_rive_audio=external --with_vulkan`).
2. **Extension stage:** godot-cpp SCons builds the GDExtension and links the stage-1 static libs per platform/arch. Match ABI knobs deliberately: C++17+, libc++ on Apple, `/MT` vs `/MD` on Windows, NDK version on Android, and **identical Emscripten version + flags to Godot's web export templates**.
3. **CI:** GitHub Actions matrix producing all binaries; artifacts assembled into the addon layout below.

Platform quirks (verified): iOS statically links GDExtensions at export; web requires the special **dlink** export templates (side-module wasm) with known rough edges — and Godot's default web template is single-threaded, so the CommandServer thread must be compile-time optional (it already degrades to same-thread pumping by design).

### 6.1 Web builds — reality check

Web works in this design, and the *renderer* half is its most battle-tested part; the risk is concentrated in Godot's extension loading. Split honestly:

- **Rendering: low risk.** Godot web = Compatibility renderer = WebGL2, and Rive's GL backend is exactly what Rive's production web runtime ships on (Emscripten-built). Confirmed in source (`renderer/include/rive/renderer/gl/gles3.hpp`): the backend runtime-detects `WEBGL_/ANGLE_shader_pixel_local_storage` (incl. coherent variant) and falls back to a full **MSAA interlock mode** when absent. Chrome/Edge take the fast PLS path; Safari/Firefox take MSAA; identical visual output, all features (incl. feathering) on both.
- **GDExtension dlink: the actual risk.** Web GDExtensions need the experimental dlink export templates (Emscripten dynamic linking) with known open issues (godot#94537, #85210). Hard constraint: our wasm must be built with the **exact Emscripten version and flags of Godot's templates** — enforced by a CI lockstep check. **Escape hatch** if dlink proves unshippable: statically link the extension into custom web export templates (the iOS model applied to web) — worse install UX, zero dlink dependency.
- **Threading:** Godot web exports are single-threaded by default (4.3+, SharedArrayBuffer/COEP); the CommandServer degrades to same-thread pumping — non-event.
- **GL state sharing:** Godot (main module) owns the WebGL2 context; our side module's GL calls flow through the same Emscripten GL runtime — one context. Rive's `GLState` tracking vs Godot's state mutation requires disciplined save/restore around flush; cost benchmarked in Phase 4.
- **Size:** rive + HarfBuzz + Yoga + Luau adds a few MB of wasm; feature flags enable a slim web variant (e.g. no scripting), with a size budget tracked in CI.

Web build testing (browser harness, PLS/MSAA path coverage, lockstep + size gates) is specified in `development-and-testing.md` §8.

## 7. Distribution, versioning, licensing

- **License:** rive-runtime + renderer are plain MIT (verified against `LICENSE`; open-sourced 2024-03-19). Vendored deps are permissive. Shipping precompiled binaries via the Godot Asset Library and GitHub releases is clean. The Rive *editor* is the paid product; the runtime/format are open.
- **Binary compatibility (refuted assumption):** Godot does **not** guarantee forward binary compatibility for GDExtensions across 4.x minors. Set `compatibility_minimum = 4.7` in the `.gdextension`, run a per-minor compatibility CI job against each new Godot release, and cut per-minor builds when needed.
- **Addon layout:** `addons/rive/` — `rive.gdextension`, `bin/<platform-arch>/`, editor plugin scripts, icons, docs. Self-contained for Asset Library submission.

## 8. Phased roadmap

- **Phase 0 — skeleton + build proof.** GDExtension scaffold (godot-cpp 4.7), submodule rive-runtime, CI building & linking all desktop platforms. Headless smoke test: load a `.riv`, enumerate artboards, advance a state machine — no rendering. *Proves the build/ABI story before any GPU work.*
- **Phase 1 — render on desktop Vulkan.** Bootstrap `RenderContextVulkanImpl` from `get_driver_resource`, RD-created target wrapped via `setTargetImageView`, flush on render thread, `Texture2DRD` + `RiveSprite2D`, single-threaded advance. *This phase retires the #1 risk (queue submission/synchronization).*
- **Phase 2 — asset pipeline + API.** ResourceLoader/importer, editor preview & thumbnails, hot reload, dynamic inspector properties, state-machine inputs, pointer input, events→signals, `RiveControl`, data binding (view models). C# smoke coverage.
- **Phase 3 — platform breadth.** Metal (macOS/iOS), D3D12 (Windows opt-in), Android Vulkan; GL Compatibility path (desktop + Android GLES3) sharing Godot's GL context with disciplined state save/restore around flush.
- **Phase 4 — web + features.** wasm dlink build (single-threaded, WebGL2 backend), text + layout + external audio engine wired into Godot audio.
- **Phase 5 — production hardening.** CommandServer threading as default, profiling (many-artboard batching through one shared RenderContext per device — Rive's context is designed to batch all draws in a frame), docs/examples, Asset Library release, per-minor compat CI.

## 9. Top risks (ranked)

1. **Queue/submission synchronization with Godot's renderer** — no engine-provided ordering guarantee; mitigated by render-thread submission + Rive's explicit image-access API; Phase 1 exists to prove it.
2. **GL state sharing under Compatibility/web** — Rive's GL backend tracks GL state (`GLState`); sharing Godot's context requires careful save/restore; cost unquantified.
3. **Web dlink fragility** — GDExtension web support has open issues; Emscripten-version lockstep with Godot templates required.
4. **GDExtension per-minor breakage** — process risk, handled with CI + `compatibility_minimum`.
5. **rive-runtime velocity** — fast-moving SDK (new ORE/canvas layer, Luau scripting, WebGPU backend in-tree); pin a submodule commit and upgrade deliberately.

---

### Appendix: primary-source facts this strategy rests on

- `RenderContextVulkanImpl::MakeContext(...)` signature — `renderer/include/rive/renderer/vulkan/render_context_vulkan_impl.hpp`
- `RenderTargetVulkanImpl::setTargetImageView(...)`, access/barrier API — `renderer/include/rive/renderer/vulkan/render_target_vulkan.hpp`
- `TextureRenderTargetGL` / `FramebufferRenderTargetGL` — `renderer/include/rive/renderer/gl/render_target_gl.hpp`
- `CommandQueue` full API (load/instantiate/advance/pointer/draw/view-models) — `include/rive/command_queue.hpp`
- ViewModel typed property runtime — `include/rive/viewmodel/runtime/*.hpp`
- Feature flags & vendored-dep symbol renames — `premake5_v2.lua`, `dependencies/`
- External audio engine mode (`EXTERNAL_RIVE_AUDIO_ENGINE`, `MA_NO_DEVICE_IO`) — `include/rive/audio/audio_engine.hpp`, `premake5_v2.lua`
- wasm/emscripten build support — `build/rive_build_config.lua`
- Godot 4.7 API: `get_driver_resource` (incl. Vulkan image/view), `texture_create_from_extension`, `Texture2DRD`, `call_on_render_thread`, `texture_get_native_handle`, `CompositorEffect`, `EditorImportPlugin` — `godot-cpp/gdextension/extension_api.json`
- MIT license — `rive-runtime/LICENSE`

Verified web findings and refutations (full citations) are preserved in the deep-research output: 22 confirmed / 3 refuted claims, sources including rive.app docs & blog, godotengine docs, godot PR #79288, proposal #6964, rive-unreal/rive-unity repos.

# rivegd vs rive-unity (Rive's official Unity runtime)

Snapshot: July 2026, researched from rive.app/docs/game-runtimes/unity,
the rive-unity GitHub README, and Rive's Unity blog posts (Jan 2026 Asset
Store update). rive-unity is Rive's own first-party integration and the
closest analogue to what rivegd is for Godot — both wrap the same C++
runtime + Rive Renderer.

## Where we match (core parity)

| Feature | rive-unity | rivegd |
|---|---|---|
| Rive Renderer (GPU-native, feathering) | ✅ | ✅ (Vulkan) |
| State machines, inputs, listeners | ✅ | ✅ |
| Events → engine callbacks | ✅ | ✅ signals + `on_event` |
| Data binding: number/bool/string/color/enum/trigger/image/list/artboard/nested | ✅ | ✅ (incl. list item read/write) |
| Observability (property callbacks) | ✅ OnValueChanged | ✅ `property_changed` + `on_property` |
| Auto-bind modes | ✅ inspector modes | ✅ default-instance binding + inspector properties |
| Text (HarfBuzz, localization) | ✅ | ✅ |
| Responsive layouts (Fit=Layout resize) | ✅ | ✅ RiveControl resize |
| Audio through engine mixer | ✅ AudioProvider→AudioSource | ✅ RiveAudioStream→any bus |
| Texture-on-mesh (3D) + pointer via UV | ✅ MeshCollider + RaycastHit UV | ✅ `send_pointer_uv` (uv_at is game-side; Godot raycasts lack UVs) |
| Rive-as-texture | ✅ RenderTexture | ✅ RiveTexture (Texture2DRD) |
| Runtime file loading | ✅ | ✅ |
| Editor import + preview | ✅ | ✅ (+ dock thumbnails, trigger buttons) |
| Hot reload | ✅ | ✅ |
| C# API | ✅ (native) | ✅ (ClassDB, tested in CI) |

## Where we are AHEAD

| Feature | rive-unity status | rivegd status |
|---|---|---|
| Luau scripting | ✅ shipped (v0.4.1+, per the official feature-support matrix — the Jan 2026 blog's "rolling out" is stale) | ✅ shipped — parity; ours is uniquely *behaviorally verified* in CI (editor-signed fixture drives a VM round-trip) |
| **Keyboard input + focus** | roadmap ("improved key and gamepad detection") | ✅ shipped (key forwarding, focus_next/previous, focus test in smoke) |
| **Text input** (typing into Rive UI) | roadmap ("building text input support") | ✅ `send_text_input` shipped |
| **Gamepad → Rive** | roadmap | ✅ wire-v2 batching, SDL→W3C remap, CI-tested |
| Event **ordering guarantees** | undocumented | ✅ O1–O4 pinned by a dedicated suite |
| Headless / logic-only instances | undocumented | ✅ (servers, tests) |
| Lifetime safety model | C# GC + handles | ✅ CommandQueue handles (rive's own model, M1) |

## Where THEY are ahead

| Feature | rive-unity | rivegd status |
|---|---|---|
| **Shipped platforms** | D3D11, Metal (macOS/iOS), OpenGL (Win/Android), Vulkan (Win/Android/Linux), WebGL | Linux Vulkan today; GL bridge written (Android/WebGL2 targets); Metal/D3D12/web pending hardware/phase 4. **Biggest gap.** |
| Out-of-band assets (referenced images/fonts/audio) | ✅ shipped | ✅ shipped (sibling-file convention, queue global registry; smoke-verified) |
| Listener-aware hit testing (`Translucent`) | ✅ per-widget Hit Test Behavior | ✅ `hit_test_behavior` on RiveControl (sync `hitTest` guarded by the flush mutex; smoke-verified) |
| Multitouch | ✅ | ✅ per-finger pointer ids (touch index → rive pointerId; smoke-verified) |
| **Panel compositor** (many widgets → ONE render texture) | ✅ RivePanel | one texture per instance (GPU work batched, but N textures) |
| **Procedural rendering** (drive Rive Renderer from C#: paths/paints/blends) | ✅ | not exposed to GDScript |
| **DPI scaling modes** (reference/constant-pixel/constant-physical) | ✅ | none (Godot content-scale only) |
| Per-widget audio routing / mixer groups | ✅ per-widget AudioProvider | one shared engine (per-node bus routing on the list) |
| RenderTexture → Rive image property | ✅ experimental (Metal/D3D/Vulkan) | Image/Texture2D via PNG re-encode only |
| Scrolling + virtualization, N-slicing | ✅ advertised | untested (authored in Rive; runtime plumbing exists — needs fixtures/verification) |

Both sides list texture compression and consoles as future.

## Row-by-row vs the official matrix (rive.app/docs/feature-support)

Verified July 2026 against the Unity column. Runtime-core rows (mesh
deformation, follow path, joysticks, solos, interpolation/speed on states,
graph editor, randomization, N-slicing, RTL text) come with the embedded
C++ runtime and need no integration surface — we inherit the C++ column's
✅. Integration-surface rows verified individually:

- At parity or better: 17/20 rows (data binding incl. lists/images/
  artboards, text, layouts, feathering, audio, scripting, events,
  listeners, raster assets, caching-of-resources…).
- Out-of-Band Assets: ✅ closed (was the one real gap; sibling-file convention + queue global registry, smoke-verified).
- Caching Rive Files: ✅ closed — one refcounted queue import per
  resource; N instances of one .riv decode once (hot reload retires the
  old generation).
- Parity-at-❌ (neither has it): Fallback Fonts (task #24), Semantics —
  where CommandQueue's drainSemanticsDiff + Godot 4.7's AccessKit make an
  accessibility bridge a first-mover opportunity for us (task #25).
- Unverified-by-fixture (believed working, runtime-core): N-slicing, RTL
  text — covered by task #17's MCP-authored fixtures.

## Read on the standings

Core runtime coverage (state machines, full data-binding matrix, events,
text, audio, 3D-surface interactivity) is at parity. rivegd is genuinely
ahead on the interactivity frontier Rive itself is still rolling out —
scripting, keyboard/text/gamepad — and on testing rigor (ordering
guarantees, behavioral script verification). rive-unity is ahead on
breadth: shipped platform matrix (theirs is production on 6 backends, ours
on 1), out-of-band assets, and input-polish niceties (listener-aware hit
testing, multitouch, DPI modes).

Actionable for us, in value order:
1. Platforms (Metal/Android/web) — hardware/phase gated, but it is THE gap.
2. Out-of-band assets (G3.6) — buildable now on Linux.
3. Multitouch + listener-aware hit testing — input polish with real UX value.
4. Scrolling/virtualization verification fixtures (authored via Rive MCP).
5. Per-node audio bus routing; DPI scale modes — small ergonomics.

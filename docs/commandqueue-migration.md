# CommandQueue migration plan

Moving `RiveRenderServer`'s internals onto rive's official `CommandQueue` /
`CommandServer` threading model (GOALS G4.9). The public Godot API does not
change at any milestone.

## Why

- **Lifetime safety by construction**: main thread holds opaque handles
  (`FileHandle`, `ArtboardHandle`, `StateMachineHandle`,
  `ViewModelInstanceHandle`); the server owns every Rive object. Freeing a
  node can never use-after-free — rive maintains these semantics upstream.
- **Listener-based async** replaces our hand-rolled mutex mailboxes
  (`onFileLoaded`, VM property subscriptions, settled notifications).
- **Future headroom**: the server can later move off Godot's render thread
  (advance/logic on a worker; only GPU submission hops to the RT), and
  `pointer*Synchronized` gives synchronous hit-testing when we need input
  results (e.g. proper focus-edge bridging).

## Topology (unchanged thread-wise until M4)

- `CommandQueue` lives in `RiveRenderServer`, used from the main thread.
- `CommandServer` is created lazily on the render thread (it needs the
  Factory — the GPU bridge's context, or a `NoOpFactory` when headless) and
  **pumped by `processCommands()`** at the top of the per-frame
  `rt_flush_all` tick. No dedicated thread; the RT *is* the server thread.
- Custom work that the queue doesn't model (RD/GL texture creation, batched
  chunked flushing, thumbnails, sleeping bookkeeping) rides through
  `runOnce(CommandServerCallback)` closures and RT-side
  `CommandServer::get*` lookups — legal because we only touch them on the
  server thread.

## Milestones

- **M0 (done)**: queue + server instantiated and pumped each frame
  alongside the existing code. No behavior change; proved pump placement.
- **M1 (done, PR #1)**: lifecycle through handles; all mutations as runOnce
  closures; request_pump() for headless; self-contained thumbnails. Found:
  the server's Factory is fixed at construction (bridge must exist first or
  everything imports render-nothing NoOp paths); queue "" state-machine
  name means designated-default, not first-machine; render smoke needed a
  content assertion. Cost: ~22µs/instance/frame runOnce overhead
  (9.12→10.23ms on the 50-artboard bench).
- **M2**: data binding onto queue-native VM APIs + property subscriptions
  (listeners replace the property mailbox); events/settled via listeners
  (replace event/state mailboxes; settled listener replaces our
  `advanceAndApply` return-value sleeping).

  Mechanics (verified against the header): messages flow server→client and
  fire listener callbacks **only when the client calls
  `CommandQueue::processMessages()`** — on whichever thread calls it. Plan:
  pump messages once per frame on the **main thread**, from the same
  request-driven tick pattern as `request_pump()` (frame_pre_draw does not
  fire headless — reuse the dedup-flag approach, pumping in a deferred main
  thread callable). Listeners: one server-owned global
  `FileListener`/`StateMachineListener`/VM listener set
  (`setGlobal*Listener`), dispatching by handle→instance-id to the nodes'
  existing signal surface. Listener callbacks then EMIT directly (no
  mailboxes, no mutexes). The ordering suite (O1–O4) is the acceptance
  gate: rive must deliver events in report order and subscriptions in a
  deterministic order, or M2 keeps the mailboxes for whichever channel
  fails.
- **M3**: drop the bespoke rt_* closures that M1/M2 obsoleted; delete the
  mailbox mutex paths that listeners replaced.
- **M4 (optional, measured)**: move the server pump to a dedicated thread;
  `draw()` callbacks record GPU work there; submission hops to the RT.
  Only if the bench shows main/render-thread contention.

## Risks / notes

- `CommandQueue` requires 64-bit handle plumbing; all Godot-visible ids stay
  our own int64s mapped to handles inside the server wrapper.
- Our chunked-batch flush (K=4, measured) must survive: rendering stays in
  `rt_flush_all`, NOT per-instance `draw()` callbacks, until M4 revisits.
- Verification bar per milestone: full 12-phase api_smoke + render smoke +
  bench within budget + C# smoke, before merging to main.

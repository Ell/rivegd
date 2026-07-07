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

## Outcome (short version)

**M1 is the terminal state of this migration.** It delivered the real prize
— instance lifetime through opaque handles, so a freed node can never
use-after-free a Rive object (GOALS G4.9), matching rive's official model.
M2/M3 turned out unsupported/not-worthwhile once the listener surface was
audited (details below); M4 (dedicated thread) stays gated on measured
contention we don't have (50 artboards = 10.2 ms, well inside budget).
Recommendation: merge PR #1 (M0+M1) to main; treat the migration as
complete at M1.

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
- **M2 — RE-SCOPED after auditing the listener surface (not worth building
  as originally imagined).** The plan was "listeners replace all four
  mailboxes." The `CommandQueue` header does not support that:

  | Our async channel | Queue listener | Verdict |
  |---|---|---|
  | view-model property changes | `subscribeToViewModelProperty` + `onViewModelDataReceived` | listener exists |
  | state-machine settled (sleep) | `onStateMachineSettled` | listener exists |
  | **reported events** | *(none)* | no callback — read `reportedEventAt` in the server-thread draw callback, which is exactly what we do |
  | **state changes** | *(none)* | no callback — read `stateChangedByIndex`, same |

  Two of four channels have no listener; rive's own model is that you read
  events/state-changes from the instance inside your server-thread draw
  callback (our `rt_drain_reported_events` / state drain). So a "full M2"
  would produce a **mixed** model (two listener paths + two render-callback
  reads) that is *more* complex than today's uniform, mutex-guarded mailbox
  set — for a marginal gain over code the ordering suite already proves
  correct (O1–O4). **Decision: do not build M2.** The mailboxes stay.

  The two available listeners are individually optional micro-improvements,
  not migration blockers:
  - `onStateMachineSettled` vs our `advanceAndApply`→false sleep heuristic:
    ours is simpler and correct; no reason to switch.
  - property subscription vs our `flushChanges` poll: a real per-frame
    efficiency nit (we poll every watched property each frame), but it risks
    changing O3 delivery order. Deferred; revisit only if watched-property
    counts ever become a profiled cost.
- **M3 — moot.** Nothing to delete: M2 didn't replace the mailboxes.
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

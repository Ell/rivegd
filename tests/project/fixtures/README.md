# Test fixtures

Most `.riv` files here are copied from the `rive-runtime` submodule's MIT test
assets (used for metadata/behavior coverage) — including
`data_binding_images_test.riv` (artboard `main`, image property `main_im`
displayed by the `root_img` element; drives texturebind_smoke). Two are ours:

## rivegd_fixtures.riv — authored for rivegd via the Rive editor MCP

A single artboard `fixtures`, bound to view model `VisVM`:
- `box-scale` (number), `box-color` (color) — databound to a visible box
  (data-binding pixel coverage).
- `label` (string), `pulse` (trigger).
- A `ProbeNode` Luau **scripted drawable**: on init writes `label="script-init"`;
  on any `box-scale` change writes `label="scaled:<box-scale>"`. Because the
  file is exported from the Rive editor it is production-signed, so its
  script actually executes in our production-key build. See the api_smoke "Luau" phase.
- State machines `Chain` and `State Machine 1` (incidental; not asserted).

## cards.riv — dynamic-list test bed (authored via the Rive editor MCP)

Component artboards (components always export; regular MCP-created
artboards may not — mark fixtures as components):
- `card` (120x160): CardVM {label string, value number → bar sx, tint
  color → bg fill}, default instance bound.
- `cards` (512x512): ListVM {items: list} bound; a wrapping `card-grid`
  layout containing an ArtboardComponentList driven by `items` — appending
  CardVM instances at runtime instantiates `card` components into the
  layout. Drives cards_smoke (dynamic lists) and the many-card bench
  (RIVEGD_BENCH_FIXTURE/ARTBOARD/SIZE/COUNT).

cards.riv also carries (all component artboards):
- `responsive`: 48px header + 25% sidebar + fill content — FIT_LAYOUT
  reflow assertions (reflow_smoke). Authoring lesson: the MCP layout tool
  may ignore child width/height/units — verify computed sizes
  (query_property_values keys 810/811) and set style properties directly.
- `textbed`: latin + Arabic + CJK text authored with the default latin
  font — non-latin lines are missing-glyph by construction, rendered as
  dense tofu boxes. fallbackfont_smoke registers system Noto CJK + Kufi
  Arabic fallbacks and asserts both rows re-shape (density drops, content
  remains; also proves RTL shaping and multi-fallback iteration).
- The `cards` grid has a vertical ScrollConstraint (editor-authored;
  clamped physics). Runtime findings: rive scroll responds to plain
  pointer events but its physics integrates MOVE-event timestamps (we now
  pass real time); the card's Click listener with "Opaque Target" enabled
  consumes drags before the scroll viewport sees them (HitResult
  hitOpaque) — non-opaque listener targets let click AND scroll coexist.
  scroll smoke pending that authoring tweak.
- `card` carries a declarative Click listener (targets card-bg, sets
  CardVM.tint to red) — clicklistener_smoke drives it through Godot input
  and asserts the VM write via watch AND pixels. Authoring note: this
  editor build only offers POINTER listener conditions (no Key Down) —
  declarative key listeners are not authorable yet; keyboard-behavioral
  verification remains upstream-blocked (rive's own keyboard fixture uses
  a script for exactly this reason).

Re-authoring: open in the Rive editor with the MCP server, or rebuild from
the tool calls recorded in the project history. Signed export is a manual
editor step (the MCP cannot export).

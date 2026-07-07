# Test fixtures

Most `.riv` files here are copied from the `rive-runtime` submodule's MIT test
assets (used for metadata/behavior coverage). One is ours:

## rivegd_fixtures.riv — authored for rivegd via the Rive editor MCP

A single artboard `fixtures`, bound to view model `VisVM`:
- `box-scale` (number), `box-color` (color) — databound to a visible box
  (data-binding pixel coverage).
- `label` (string), `pulse` (trigger).
- A `ProbeNode` Luau **scripted drawable**: on init writes `label="script-init"`;
  on any `box-scale` change writes `label="scaled:<box-scale>"`. Because the
  file is exported from the Rive editor it is **production-signed**, so its
  script actually executes in our production-key build — this is our only
  behavioral Luau coverage (G5.4). See the api_smoke "Luau" phase.
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

Re-authoring: open in the Rive editor with the MCP server, or rebuild from
the tool calls recorded in the project history. Signed export is a manual
editor step (the MCP cannot export).

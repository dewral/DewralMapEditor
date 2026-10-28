# Procedural brush v2

The main topbar tool is a local example-based procedural generator. It does not call an LLM or use ground-brush names to generate items. The previous RAW quilting and advanced WFC engines remain readable for old styles/plans.

## Normal use

1. Select finished map fragments, including both sides of their boundaries and corners; click **Zapamiętaj**. Larger examples provide more directions. The library can combine several checked examples into a single style.
2. Choose a style in the topbar, then paint a corridor or area. The map stays unchanged while the native preview renders the result. Release the mouse to commit one Undo operation.
3. **Zwężaj** closes part of an existing room and rebuilds its boundary. Right drag still removes pending mask cells rather than secretly changing committed map data.
4. **Struktura** places complete prefabs or an assembled structure. The adjacent span control repeats its bridge segment; whole parts may include other floors.
5. Ctrl+Z restores the whole stroke, including the border halo and every affected floor. Escape exits the brush.

Automatic analysis is the default. If the inferred interior is wrong, select its ground on the map and use **Użyj zaznaczonego podłoża jako wnętrza** in brush settings. This saves a new style revision. Missing directional examples produce coordinates and a red diagnostic cell instead of fabricated edges.

## Generation

The brush footprint controls geometry. Source analysis extracts ordered RAW stacks, surface groups, directed border templates, compound border pieces and complete connected detail motifs. Client appearance, passability and observed spatial mixing propose surface families; two segregated materials are not merged merely because they share a minimap color. Explicit role corrections override inferred grouping.

Directed boundary patterns use eight-neighbor masks. Missing compounds can be assembled only from observed directed RAW pieces. Diagonal variants may substitute only when their cardinal connectivity agrees. Walkable guides try up to eight compatible variants and reject unresolved obstacles. No item IDs are rotated or invented.

Interior generation and decoration use a coordinate hash of the seed, position and role. This keeps unchanged interiors stable as the stroke grows. Detail motifs retain relative positions, item order and counts; they must fit completely. Groups cut by source selection, exceeding a four-cell radius/64 cells, or containing floor transitions are not scattered as loose items; use complete prefabs for them.

The current learned border halo is one tile, within the native maximum of four. It changes learned edge items only and preserves the outside ground and other details. Native validation rejects a halo that changes other content. Protected contents include houses, zones, creatures/spawns, item extras/identities, containers and floor-transition objects. Ordinary tile flags are retained through the transaction.

## Structures

The **Struktury** tab assembles named recipes from complete BrushStore prefabs. Each part has X/Y/floor offsets. Parts can be fixed, repeat as bridge segments, or shift with the bridge endpoint. The span is 1–32. Overlapping or clipped parts fail as a whole. A recipe can declare connections: endpoints must be passable, same-floor endpoints must connect, and a vertical connection needs an actual floor-transition item.

Saving and attaching a recipe creates a new style revision with frozen prefab data. This supports authored islands, bridges, lower-floor components and their combinations. It does not infer a scalable building or a well from an arbitrary unlabelled screenshot. Construct a recipe from the appropriate source parts first.

## Plans and compatibility

`procedural-v2` models retain sanitized RAW samples, source metadata, role corrections, analysis, frozen prefabs and recipes. `styleVersion` is 2; the legacy model version remains 1 for old engines. Library entry version 2 coexists with the original version-1 library container.

A saved plan contains its footprint, guides, pins/recipe spans, seed, parameters and a complete style snapshot/revision. Editing a style creates a new resource, leaving existing plans stable. Export/import packages contain the library resources; import requires a matching client profile, remaps resource IDs and writes the merged library atomically. The package limit is 64 MiB.

Generation runs in the existing background worker. Changed masks, maps or profiles cancel obsolete results. Preview validation checks the current native map revision, loaded item validity, bounds, protected contents and output limits before any writes. There are no hidden AI/network requests.

## Verification

`map_generation_test` covers geometry, RAW order/subtypes, whole motifs, deterministic seeds, protected cells, path constraints, shrinking, multilevel recipes and connections, frozen plans, and package round trips/profile separation. Legacy RAW and WFC checks remain included.

`map_creature_refresh_test --raw-style-check <client-folder> <library.json> <output-folder> --procedural --source-name=<example-name>` renders an organic corridor/room using actual DAT/SPR/OTB data without a BrushStore, checks native preview/commit/Undo/Redo and saves `raw-demo.otbm`, `raw-source.png` and `raw-generated.png`. Add `--midhem` for the custom client format.

QML tests cover immediate painting, release-to-commit, cancellation, errors, closing brush, recipe footprints/spans, frozen plans, diagnostic cells, SVG controls and narrow-window layout. `--smoke-test --smoke-map-brush` checks the packaged app.

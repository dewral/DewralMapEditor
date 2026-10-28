# Native Generation Library

The main topbar brush now uses **procedural v2**. See [current behavior and use](procedural-brush.md). The RAW quilting, terrainPaint and WFC descriptions below document the retained legacy engines and diagnostics.

The **Pędzel z wzorca** button is in the topbar above the map tabs, in every theme. It opens an inline toolbar instead of covering the map. The existing Tools menu activates the same toolbar.

## Direct painting on the current map

1. Click **Zaznacz wzorzec**, select a finished connected area of at least 3×3 tiles, then click **Zapamiętaj**. Capture and learning use the loaded map and client definitions.
2. Choose a saved style from the **Wzorzec** picker and select **Ścieżka**, **Obszar** or **Prefab**. Set **Rozmiar** beside those modes. **Ustawienia** contains irregularity and replacement; **Biblioteka** opens the optional resource manager.
3. Move over the map to see the exact native brush footprint as a thin white contour. A pending stroke has a subtle perimeter, with no blue tile grid or green path squares. Generated terrain remains visible inside that perimeter. The ordinary item brush cursor is suppressed while this brush is active.
4. Releasing the mouse commits the RAW stacks as one Undo step. Ctrl+Z undoes it. Escape or the active **Pędzel z wzorca** button exits the tool. Escape first closes an open picker/settings popup.

The toolbar wraps controls on narrow windows. Its status row shows progress, source-selection instructions and generation errors. Source controls remain disabled during generation/commit. The main brush uses complete RAW tile stacks and their spatial arrangement from retained examples. Existing saved styles are rebuilt from those examples automatically when used by the main brush.

The RAW path does not look up ground brush families, scatter independent decorations or run automatic border reconstruction. It copies ordered item IDs, subtype counts and ground flags from overlapping source windows, preferring a continuation of the previous window. Windows start at 9x9 cells and fall back to 7x7, 5x5 or 3x3 when the selected examples are smaller. Unknown source cells are not treated as empty tiles. Item identity attributes are stripped. Existing protected cells and map metadata remain subject to the usual replacement rules.

This is local fragment synthesis rather than semantic understanding of a cave or building. Clipping at the painted footprint and stitching between source windows can cut larger arrangements. Use complete prefabs for structures that must stay whole. A larger connected example with both interiors and edges gives more usable continuations; brush definitions do not repair missing source transitions in RAW mode.

OTBM example import passes loaded item/subtype/fluid definitions to its background reader for legacy inline subtype decoding.

## Optional library preview workflow

The **Brush** tab is the default when a style is available. Select a learned style, then choose:

- **Corridor**: left drag paints a connected organic corridor. Its center line is kept passable. Right drag erases only the pending footprint.
- **Room**: click or drag to grow an irregular room. **Connect new rooms** adds a corridor to the nearest pending area within 128 tiles. The connector belongs to the same mask and transaction.
- **Prefab**: choose a prefab allowed by the style and click to add the complete object, including its other floors. The projected footprint extends the pending area; right-click removes a pending stamp. Normal fit and occupancy checks still apply to the entire prefab.

**Brush width**, **Irregularity** and seed control deterministic shapes. Editable terrain ground is constrained to passable items in procedural plans. Decoration density/spacing and complete prefab placement still come from the learned style.

**Update preview while painting** schedules previews every 180 ms even while the mouse keeps moving; new movement does not indefinitely postpone the first request. Learning/generation runs in the existing background worker. Editing cancels an obsolete request; only the latest map revision can become the preview. The selected brush stays armed after preview and Apply. Apply commits the pending area in one undo step and clears that pending area so the next stroke can start cleanly. No map items are changed by drawing or erasing a footprint.

**Area editing and saved plans** reveals manual masks/guides and blueprint saving. A saved plan contains the same brush footprint, paths, prefab pins, seed and brush parameters; it is optional and requires no separate setup. Existing version 1 plans remain readable.

Complex islands, bridges and authored structures use complete prefabs selected for the style. The brush does not infer an arbitrary building or invent a new multilevel bridge from an unmarked sample.

## Library workflow

1. Select finished map fragments and use **Examples → Capture selected map fragment**. Samples are copied into the library. Alternatively choose an OTBM and explicit X/Y/Z, width, height and floor count; only that fragment becomes a sample.
2. In **Prefabs**, save complete selected buildings (all relevant floors), or check existing prefabs. These remain ordinary BrushStore prefabs; no migration changes their original data.
3. Check examples in **Examples**, then use **Styles → Learn style from checked examples**. Checked prefabs become the style's allowed structures. Exact full-stack matches in examples teach their occurrence rate. Learning and generation run locally in C++ on a background worker.
4. In **Brush**, choose the style. **Paint mask** adds cells with left drag and erases with right drag; repeated strokes extend the same pending area. The normal map brush size controls the width. **Paint path** draws connected, passable guides inside that area. Stop painting or press Escape to use other map tools. Selection can replace or extend the mask.
5. Preview, change the seed for a new variant, and **Apply**. Existing occupied cells are protected by default. Explicit replacement changes item stacks inside the mask while retaining existing map metadata. Apply is one Undo/Redo operation, including generated monster/spawn records. Advanced library generation retains strict context constraints. The main RAW brush writes only the painted footprint and explicitly pinned prefab floors.
6. Save the blueprint to retain its style reference, mask, paths, prefab pins, seed and parameters. Reopen it and edit any of these. **Regenerate selected part of preview** solves only the selected cells and preserves the rest of the pending preview as context.

## Model and constraints

The main brush stores a `raw-patch-v1` model containing sanitized source tiles. Tile order, stack order and counts remain available to synthesis, including structural items. Matching scores use complete stack signatures, with ground differences weighted more strongly. Protected cells, whole pinned prefabs, loaded item validity, cancellation, output limits and connected walkable path guides are checked. Generated patches carry `rawLayout: true` and do not request `autoBorders`.

The following ground-neighborhood model remains available to the older advanced workflow and terrain-paint diagnostics; it is not the main RAW brush:

The version 1 native model learns weighted, overlapping 3×3 **ground-ID neighborhoods**, conditional decoration stacks, frequencies and observed nearest-neighbor spacing, monster observations, and whole prefab occurrences. Unknown or unselected source cells never count as void. A sample must contain complete 3×3 neighborhoods. Recognized prefab cells are excluded from terrain-neighborhood learning and decoration extraction; known wall, teleport and floor-transition items are not sampled as loose decorations.

The advanced model uses exact ground IDs. Direct terrain painting instead uses the ground brush family at the source center, retaining weighted item variants and decoration frequency while deriving borders from the current footprint. Ground constraints in advanced mode use exact IDs, including variants. Incompatible transitions produce coordinate diagnostics. Eight deterministically seeded WFC attempts are allowed; the engine does not fabricate a contradictory fallback. Terrain masks use one active floor; prefab stacks can span multiple floors. Path cells must be passable and connected. Mandatory border pieces are retained when spacing or path guides suppress ordinary decorations. Prefabs must fit the complete projected mask, map bounds, floor range 0–15, path guides and occupancy policy. Conflicting automatic placements are skipped whole; a conflicting pinned placement prevents applying the result.

Monsters and spawns are optional and off by default. NPCs remain manual. Captured/generated item data contains server ID, count and ground classification; action IDs, unique IDs, teleport destinations, house identities and nested container contents are never cloned.

The same model version, context, mask, settings and seed reproduce the same result. Models support up to 512 distinct ground neighborhoods; a request supports up to 100000 mask/output cells and 500000 output items. A domain-memory limit and propagation work budget return actionable errors for complex requests. Split very diverse examples into coherent styles or use smaller masks. Generation can be cancelled without touching the map.

## Persistence and compatibility

Resources use stable UUIDs and a versioned JSON document written atomically with QSaveFile. Each loaded client fingerprint has a separate directory under the application's AppDataLocation/generation-library. Samples and learned models survive deletion of their original maps. Referenced styles/examples cannot be deleted until their dependent resources are removed. Unsupported library schemas are reported and left intact.

Changes to the target map, mask, client materials or brushes invalidate a pending result. Applying an expired preview is rejected. Preview stacks and creature names use the same virtual tiles in the map renderer and in-game scene.

**Legacy** lists existing Tile Map Generator recipes and links to the existing dialog. Those recipes and AI blueprints retain their original engine and schema. Native learned styles do not require TileMapGenerator, an API key, or network access.

## Verification

`map_generation_test` covers deterministic and connected organic brush shapes, learned forest/cave generation, deterministic seeds, protected/replaced content, impossible context/path constraints, whole multilevel prefab bounds, exact prefab recognition, cancellation, 90000-cell generation, asynchronous learning/generation, profile separation, persistence, and selected OTBM import without copied identities.

`map_creature_refresh_test` includes native generation preview, selection refinement, a single undo group, monster/spawn undo and redo, stale-result rejection, mask bounds and OTBM/spawn round trips. `tests/qml/tst_generation_library.qml` covers blueprint restoration, live preview scheduling, superseded results, prefab stamps, continuation after Apply, preview revision capture, cancellation after map changes, painting shutdown and dialog rendering.

## Reproducible 7.72 example

Run `map_creature_refresh_test.exe --brush-demo <Tibia772-folder> <output-folder>` with a local 7.72 client and `Server772/data/items/items.otb`. The test creates a copied training patch, learns its grass/decorations and whole rock-clearing prefab, draws native mouse strokes, generates their preview, and verifies atomic Apply/Undo/Redo. It writes `training-example.png`, `procedural-brush-preview.png`, `procedural-brush-demo.otbm` and the brush plan JSON. Sprite assets are read from the local client.

The sample is at X20–38/Y20–38/Z7; the generated area is near X65–100/Y65–85/Z7. It demonstrates the actual native output using real 7.72 assets. The surrounding void is intentional: the demonstration example contains grass, flowers and a rock prefab, without a surrounding terrain style.

Run the same demo with `--terrain-paint` to test a connected terrain stroke on dirt, geometric borders, preserved action identities in the border halo and a single Undo/Redo operation. The example and image are written to the requested demo output directory.

## Custom client examples

Ground items are never classified as loose structural objects by fallback brush IDs or standard items.xml floor-change data. Non-ground structural checks use the loaded client appearance and OTB group. Direct terrain painting reanalyses retained source examples against current material metadata; previously captured styles therefore recover decorations lost by old classification.

The learner also records unambiguous directional edge observations from complete source neighborhoods. Items observed in terrain interiors are excluded from inferred edges. On custom terrain without matching brush definitions, the preview uses these learned directional pieces, including a border-only halo. Missing transitions or directions produce an explicit error requesting a larger source region containing both sides; they do not silently commit an unfinished coastline.

`map_creature_refresh_test --midhem-brush-check <Midhem-folder> <Midhem-library.json> <output-folder>` verifies the real custom DAT/SPR/OTB, regeneration from a retained sample with incorrect old structural labels, recovered snow decorations and rejection of an unobserved transition without map writes. It writes `midhem-preview.png`.

## RAW regression check

`map_generation_test` checks exact spatial layout, ordered stacks and counts, structural RAW items, independence from brush classifications, protected contents, cancellation, path constraints and whole prefab bounds.

`map_creature_refresh_test --raw-style-check <1098-client-folder> <1098-library.json> <output-folder>` learns the latest retained example, generates 600 cells without a BrushStore, renders `raw-source.png` and `raw-generated.png`, validates native preview and a single Undo/Redo transaction, and saves `raw-demo.otbm`. It uses the actual local DAT/SPR/OTB files.

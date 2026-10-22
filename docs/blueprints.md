# Blueprints

Blueprints save reusable pieces of a map in the active client's Doodad categories.
They survive editor restarts and can be shared as individual `.dmeblueprint` files.
Existing fixed-layout Prefabs appear as Blueprints automatically; random doodads
keep their weighted placement behavior.

## Save a map piece

1. Select the tiles to reuse. Enable multi-floor selection when needed.
2. Right-click and choose **Save as Blueprint…**.
3. Choose **Map piece** for every item stack, or **Ground with borders** for base
   ground and terrain borders recognized by the active brush profile.
4. Review the layout preview, dimensions and floor count, enter a name and Doodad
   category, and click **Save**. Choose **New category…** to create a category.

Capture always includes complete stacks in the selected tiles, even when normal
selection is set to select individual items. Ground capture excludes walls and
decorations. Include the surrounding border tiles in the selection if you want
those edges in the Blueprint. Gaps remain gaps: they never clear destination tiles.

Blueprints retain item counts, fluids, text, descriptions, visual properties and
container contents. They exclude creatures, spawns, houses, zones and tile flags.
Action IDs, unique IDs, teleport destinations, house-door IDs, depot links and
arbitrary gameplay attributes are removed, including inside containers. Doors
placed into an existing house receive locally assigned door IDs through normal
editor placement.

## Place and rotate

Select a Blueprint in its **Doodad Palette** category. A preview follows the
cursor. Each left-click places one stamp, with one Undo/Redo step. Brush size and
drag painting do not multiply Blueprint placements. The stamp stays selected for
repeated clicks; **Escape** or **Cancel** exits placement.

Use **Rotate** or the configured rotation hotkey (default **Z**) for quarter-turns.
Terrain borders, walls and items with known rotated variants rotate with the
layout. Newly captured Blueprints anchor their top-left bounds and smallest
selected floor number to the cursor and active floor. Old Prefabs retain their
existing anchors. Lower-floor previews follow **Show lower floors**.

The map's Blueprint controls offer:

- **Merge** (default): replace ground where the Blueprint includes ground, then
  add borders and other items using normal stack ordering.
- **Replace**: replace item stacks in occupied Map-piece cells. For Ground with
  borders, replace only ground and recognized borders, retaining decorations.

Both modes preserve destination creatures, spawns, houses, zones and tile flags.
Placement preserves saved border shapes without automatic rebordering. A stamp
that cannot fit within coordinates 0–65535 and floors 0–15, references missing
items, or cannot allocate destination house-door IDs is rejected before editing.

## Manage and share

Right-click a Blueprint in the palette to **Rename**, **Move to category**,
**Export**, or **Delete** it. Rename and Move open the same properties dialog;
both fields can be edited together. The palette and map context menu provide
**Import Blueprint…**. Import shows the source client, layout, name and category
before saving. Existing names are never overwritten; choose another name.

Files contain JSON data and item IDs, without Tibia assets. Imports require the
same client version and all referenced item IDs in the active client, including
container contents. IDs are not converted. Custom clients must agree on what
those IDs represent. Client assets must be supplied separately by the recipient.

The client profile's `brushes.json` stores Blueprint data alongside the existing
Prefab ID layout. Legacy entries are interpreted without rewriting them on load.
The portable JSON format is version 1. Files are limited to 64 MiB and 65,536
occupied cells; unsupported format versions are rejected. Older editor releases
can read the compatibility item-ID layout but cannot retain Blueprint properties
when editing it. Use a Blueprint-aware release for shared files and rich layouts.
Blueprints with retained properties, multiple floors or large layouts cannot be
saved through the single-floor Doodad composer. To edit their contents, place
them on the map, edit the tiles, and save a new Blueprint.

## Validation

The `blueprint_store` CTest covers compatibility, persistence, sanitization,
import/export, malformed files and failed writes. `map_blueprints` covers capture,
placement, rotation, Undo/Redo, mouse/keyboard input and reload/share workflows
with synthetic client assets. `tests/qml/tst_blueprints.qml` checks the dialogs
and placement controls across supported themes; Doodad composer tests protect
against lossy layout saves.

# Tile Map Generator

**Ruins & settlements** adds streets, plazas and cutaway buildings with upper
storeys and cellars. Choose blocking walls and a compatible stair pair, then assign
outdoor, street, interior and cellar grounds (each defaults to the walking floor).
Street Z is the surface: upper levels decrease Z and cellars increase it. Every
floor and stair landing must fit Z0–15. DME previews the street level and applies
the complete footprint as one undoable edit. This requires a generator with
settlement/manifest 2 support; older installations show an update message.

Optional AI planning uses one explicit paid OpenAI request to arrange districts
and landmarks from a description and the selected material metadata. Review the
summary, then Generate builds a local map. Generate and New seed reuse a saved
blueprint without another API call. Dimension, floor, guide or material changes
invalidate it. Client sprites remain local.

Fill `OPENAI_API_KEY` in ignored `.env.local` in DME or TileMapGenerator; optional
`OPENAI_MODEL` defaults to `gpt-6.1-sol`. Alternatively use the masked key field.
Keys stay in the session unless Windows Credential Manager saving is selected.
Forget removes session/vault credentials without changing user env files. Keys
are excluded from generator profiles, recipes, map files and process arguments.
AI failures and cancellation preserve the current map result.

The Tools menu shows **Tile Map Generator…** when
`C:\github2\TileMapGenerator` contains `dist\TileMapGen\TileMapGen.exe`,
`TileMapGen.exe`, or a prepared `.venv\Scripts\python.exe` with the generator's
source. The integration runs the local `generate` command with JSON inputs.
It does not need the studio's web server running.
When a prepared source checkout is present, DME uses its Python environment so
engine fixes take effect even if an older studio executable is still open.

Open a map with the intended client profile loaded, then choose the menu entry.
Select **Generate in DME**, a style (Hunting grounds, Cave or Linear dungeon),
width, height, floor count and seed. Reusing a seed and the same inputs gives a
repeatable result; **New seed** creates a new variation. Choose a walking ground
brush and optional blocking background, secondary ground, water, lava, walls,
decoration prefab and up to five distinct monsters with relative weights.
For example, weights 75 and 25 give a 3:1 spawn ratio. Advanced settings control rooms, corridors,
caves, terrain, decoration and spawn density, spacing and respawn time.
Material IDs and movement flags come from the active client. Creature names are
exported once; when a monster and NPC share a name, generation uses the monster.
Incompatible or stale material references are reported by the generator.

Ground pickers use the active editor's ground definitions and their configured
preview items, resolved through the loaded client's OTB mapping. Each brush shows
an example sprite in the field and dropdown. Walking floor and secondary ground must be passable;
background, water and lava must block walking. Leave background as Void when
no blocking ground is wanted. Each picker lists brushes compatible with its role;
all variants must be valid ground without floor changes. DME validates ground
selections before starting the generator.

Enter a profile name and click **Save** to store the complete recipe locally,
including materials, all monster weights, stair IDs, placement and advanced
settings. Select a saved profile to restore it, use **Save** to update it, or
**Save as new** to keep a separate variation. Profiles survive restarting DME
and are stored in its user data directory. Missing materials retain their saved
names when loaded with another client and show as unavailable. Choose compatible
brushes to enable Generate again.

For multiple floors, provide a matching down stair and directional up stair
server ID. The selected origin floor plus floor count must fit floors 0–15.

Choose **Current map (preview, then Apply)** and enter the origin X, Y and Z.
Click **Generate**. DME exports the client's material definitions automatically,
runs TileMapGen in the background, then validates and previews the result on the
map. **Apply** places it as one edit; Undo and Redo include the generated spawns.
The complete rectangle on every generated floor must be empty, including tile
flags, houses and spawn records. Placement supports up to 100,000 cells and
500,000 items, within the editor's undo memory budget. Changing the inputs,
cancelling, closing the dialog or switching maps discards the preview.

Choose **New map document** for larger generation. Success opens a new unsaved
map without filling its undo history. Save normally to write OTBM and spawn XML.
The X/Y placement origin applies only to placement in the current map.

The other workflows let you export a `.tmgpack` including sprite previews for
the external studio, or import a completed `.tmg` file. Imported maps must match
the active client and material fingerprint. Synthetic studio demo maps cannot
be imported as production maps. Generated files and exported client assets are
kept local.

### Production foundations in Studio

TileMapGenerator 0.4 uses Recipe/Project v2 for curated kits, semantic regions,
pinned landmarks and stage regeneration. Its resolved `.tmg` maps and `.tmgpack`
assets still use format v1, so DME can import the new hunting grounds, caves and
winding dungeons with optional wings using the existing import workflow.
**Generate in DME** and saved DME generator profiles continue to use legacy v1
recipes. Use Studio for the new composition pipeline:

1. Load the intended client and brushes in DME, then choose **Export materials for
   studio**. Export a fresh `.tmgpack` to include sprite patterns, elevation and
   item rotation mappings needed by Studio previews and curated modules.
2. Open the pack in Studio and use a compatible v2 kit/project. Kits bind to the
   exact client/material fingerprint; rebind a kit when the catalog changes.
3. Generate and refine the composition in Studio, then export its resolved `.tmg`.
4. Choose **Import .tmg** in DME with the same client and brushes. Save the map to
   OTBM and spawn XML and continue tile editing normally.

Studio authoring state stays in its project. Importing `.tmg` transfers the
resolved tiles, stacks and spawns, not editable semantic regions or recipe
history. Subsequent DME edits are not imported back into the procedural project.
Static previews use native-resolution sprites and position/stack patterns;
animations remain an editor feature. The optional `render` metadata and
`sprite_variants` table do not change the strict v1 pack manifest or fingerprint,
and pattern zero remains in `assets` for older readers.

If the generator fails, its final error is shown in the status message. **Show
error details** expands the complete traceback, and **Copy error** copies it for
troubleshooting.

Native tests use a synthetic fixture produced by TileMapGenerator's
`scripts/make_dewral_fixture.py`. Set `TILEMAPGEN_FIXTURE_DIR` and
`TILEMAPGEN_DIRECTORY` when configuring CMake, build `tmg_import_test`,
`tilemapgen_process_test`, `tilemapgen_settings_test`, `tilemapgen_materials_test`,
`tilemapgen_assets_test` and `DewralMapEditor`, then run CTest's `tilemapgen_*`
tests. The QML workflow tests are in `tests/qml/tst_tile_map_generator.qml`.

For local real-client acceptance, build `tilemapgen_asset_tool` (also available
from the standalone `tests/tilemapgen-assets` CMake project). It supports:

```text
tilemapgen_asset_tool --export CLIENT BRUSHES VERSION PROFILE OUTPUT_DIR
tilemapgen_asset_tool --render CLIENT VERSION PACK_JSON MAP_TMG OUTPUT_PNG [X Y WIDTH HEIGHT]
```

`--export` captures a native catalog and static sprite patterns for comparison;
use the editor workflow for the final `.tmgpack`. `--render` validates a resolved
map, renders its native tile stacks, saves OTBM/spawns, reopens them and checks
item and spawn persistence. `PACK_JSON` must be the matching exported catalog.
Keep those local client captures outside the repository. The border test's
`--fixture BRUSHDIR OUTPUT GROUND...` includes optional borders and three-material
intersections; `--wall-fixture BRUSHDIR OUTPUT NAME` captures all 16 cardinal wall
adjacencies for cross-language checks.

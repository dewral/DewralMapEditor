# Tile Map Generator

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

If the generator fails, its final error is shown in the status message. **Show
error details** expands the complete traceback, and **Copy error** copies it for
troubleshooting.

Native tests use a synthetic fixture produced by TileMapGenerator's
`scripts/make_dewral_fixture.py`. Set `TILEMAPGEN_FIXTURE_DIR` and
`TILEMAPGEN_DIRECTORY` when configuring CMake, build `tmg_import_test`,
`tilemapgen_process_test`, `tilemapgen_settings_test`, `tilemapgen_materials_test` and `DewralMapEditor`, then run CTest's `tilemapgen_*`
tests. The QML workflow tests are in `tests/qml/tst_tile_map_generator.qml`.

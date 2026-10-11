# Lua generation API

Open **Tools → Lua Scripts & otMapGen…**. Lua 5.1.5 is embedded in DME;
no Lua installation or running TFS server is required.

## otMapGenPublic

1. Download and extract [otMapGenPublic](https://github.com/abecek/otMapGenPublic).
2. Load an editable map with matching client assets. The supplied schemas use
   Tibia 8.6 server item IDs. DME rejects IDs absent from the current client.
3. Choose **otMapGenPublic**, then choose its root folder containing `config.lua`,
   `lib/` and `data/genScripts/`.
4. Select a generation script (start with `test30.lua`), a seed, and destination
   X/Y/floor. Click **Generate**, then **Apply** after checking the summary.

The adapter loads upstream libraries directly, runs the selected script in CLI
table mode, and converts `CLI_FINAL_MAP_TABLE` to a DME patch. It does not call
the upstream OTBM writer, bootstrap shell commands, or TFS drawing functions.
Ground and item order/counts are retained; the mock `uid` fields are temporary
generator handles and are deliberately not written as OTBM unique IDs.

The script's `MAP_CONFIGURATION.mainPos` is translated to the destination origin.
Coordinates outside its main region retain their offsets. Multi-floor output
retains relative floors; DME refuses patches extending outside floors 0–15.
Upstream generator files are external and are not redistributed with DME.

## Custom scripts

Choose **Custom Lua script** and select a `.lua` file. Scripts build a detached
tile buffer; this initial API does not expose live map reads, brushes, creatures,
or the editor UI. `dme.getTile` accesses the generated buffer only.

```lua
for y = 0, 20 do
    for x = 0, 20 do
        local pos = {x = x, y = y, z = 7}
        dme.setGround(pos, 4526) -- Server ID from the loaded client.
    end
end
dme.addItem({x = 10, y = 10, z = 7}, 2152, 10)
dme.log("Generated a 21 × 21 area")
```

| API | Behavior |
| --- | --- |
| `dme.apiVersion` | `1` |
| `dme.setGround(position, serverId)` | Set/replace the generated ground. |
| `dme.addItem(position, serverId, count = 1)` | Append an item to the generated stack. |
| `dme.clearTile(position)` | Create an empty generated tile or clear its items. |
| `dme.getTile(position)` | Return/create a generated tile `{x,y,z,ground,items}`. |
| `dme.log(message)` / `print(...)` | Append to the execution log. |

Use ordinary Lua functions, `dofile`, `require`, and `math.random`. Relative
script/module/file paths resolve against the script's folder (the generator root
in otMapGen mode). The chosen seed initializes `math.random`; scripts can reseed
it themselves. Custom scripts use minimum X/Y and maximum Z as their source
origin for translation to the destination.

## Applying, cancellation and limits

Generation runs on a worker thread and never modifies the open map. Errors or
cancellation discard the entire result. **Apply** validates every position and
item ID before modifying the map, replaces items only on generated tiles, and
preserves tile metadata including zones, houses, creatures and spawns. It is one
undo group; use **Undo/Redo** to remove/restore the change. The panel refuses to
apply to a different active document from the one used when generation started.

Runs have a 120-second Lua execution budget, a 256 MiB Lua allocation budget,
and limits of 100,000 generated tiles, 500,000 items and 1,024 items per tile.
Cancellation and timeout are checked by a VM instruction hook. The log is
capped at approximately 32 KiB. File IO remains available: run trusted scripts;
this is not a filesystem sandbox. Subprocess execution, process exit and native
module loaders are disabled. Blocking file IO is not interrupted by the hook.
Item counts/subtypes use OTBM's byte-sized Count attribute (0–255).

The generator is upstream alpha software. DME's adapter does not fix its room
layout/pathfinding bugs, and does not implement its TFS-only delayed drawing or
server interactions. Existing map dimensions are unchanged by applying a patch.

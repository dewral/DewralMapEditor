# Numbered zones (optional RME-ZONES extension)

Tools > Numbered zones opens the zone brush and search panel. Enable **Save numbered
zones in this map** only when the target server implements RME-ZONES. The same setting
is available in Map > Properties. New maps and standard maps default to disabled.
Loading an OTBM carrying the extension automatically enables it to preserve its data.

Choose an ID from 1 to 65535 and click Paint zone. Paint adds that ID to ground tiles;
several IDs can coexist. Ctrl + paint removes only the chosen ID. ID 0 clears all IDs.
The existing brush size/shape, grouped strokes and Undo/Redo apply. Whole-tile copy,
cut, move, rotation and paste preserve zone lists; copying a single item does not.
View > Show numbered zones controls the overlay independently of PZ/PvP flags.
Visible zones use world-anchored colored fills (stripes for overlaps) and a bright
outer outline without shared edges inside the same zone. The active ID has a thicker
outline. Labels use the same ID color and remain visible independently of PZ/PvP
visibility, including the classic renderer. Numbered overlays draw above PZ/house
fills so their boundaries remain legible. Connected labels show IDs.
Hovering a tile also lists its IDs in the position status. Find zones searches the
whole map or selection; ID 0 finds all IDs. Click a result to jump to it. Results are
limited to the first 1000 tiles, with the full match count displayed.

## Compatibility

The implementation follows the wire layout in
[fridaii/RME-ZONES](https://github.com/fridaii/RME-ZONES/blob/66c4963bd8a5bf6bd347d4dfcae0c015b52f8b05/source/iomap_otbm.cpp):
TileFlags bit `0x40`, followed immediately by little-endian `uint16_t` zone IDs and a
zero `uint16_t` terminator. IDs are unique and sorted. Ordinary tile flags, house tiles,
items and the OTBM version are preserved. Truncated lists fail loading safely.

With the setting disabled, normal Save / Save As writes **neither the extension bit
nor the zone list**. This produces ordinary TFS-compatible tile metadata. Existing
zones remain in the open document so the setting can be re-enabled, but are absent
from that output file; reopening it will not recover zones. Recovery snapshots retain
them regardless of output mode. Imported/copied zone data follows the destination
map's output setting; enable the extension there if it should be included on save.
An empty enabled map has no extension records to detect on reopening, so enable it
again before creating the first zones. Workspaces do not add private attributes to
standard OTBM files to remember this switch.

Ordinary tiles allocate no zone list. A tile carries one nullable 8-byte metadata
pointer (64-byte tile bound); lists are allocated only for tiles with IDs and are deep
copied for history and background recovery. Rendering batches visible chunks instead
of creating per-tile UI widgets. Disabled maps skip numbered-zone rendering entirely.

Tests use independently generated RME wire fixtures, standard-output inspection,
overlapping/max IDs, malformed lists, undo/redo, bulk editing, cleanup, background
adoption, detached recovery and recovery while output is disabled. View tests cover
painting, Ctrl removal, search, labels and overlay visibility. UI tests cover opt-in,
brush activation, selection scope and four themes.

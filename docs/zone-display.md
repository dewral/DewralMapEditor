# Zone display

Fluent Dark uses the colors and layout from `design/dme-zones-concept-v1.png`.
Preferences → Zone display controls flag visibility and opacity; changes are saved with
the map-view preferences. Tile and item opacity can be adjusted independently.
House, spawn, creature and grid visibility use the existing view settings.

Protection Zone is blue (`#399ee8`), Non-PvP green (`#48b883`), No Logout amber
(`#dfa65a`) and PvP red (`#d46b79`). Overlapping flags occupy alternating diagonal
bands, rather than mixing into a third color. Outlines omit shared tile edges.
The active zone brush uses a stronger outline. Other themes retain their existing
zone rendering.

Connected visible regions have compact labels; mixed regions show names such as
`PZ + NP`. House labels use the map's house name when available. Labels are hidden
below an 8-pixel tile size and capped at 64 to keep overview maps readable.
Labels stay anchored to the center of the complete connected region, including
tiles outside the viewport. Panning and zooming only transform that fixed world
position; labels are clipped instead of sticking to viewport edges. Region
anchors are cached until the map metadata or zone display settings change.
When painting extends a region, its existing anchor is retained while that tile
still belongs to the region. Removing the anchor tile or splitting off a region
causes only the affected label to be recentered.
Region discovery is deferred while painting or dragging a selection. Once the
operation ends, labels update once; camera changes keep projecting the existing
anchors. Unchanged label models retain their QML instances instead of rebuilding
on every pointer update.
Zone fills and boundaries refresh for each committed edit batch while painting;
only label discovery is deferred until the stroke ends. Camera movement is not
needed to display newly painted or erased flags.

Spawns use a square with a subtle 12% violet fill, a thin lavender outline,
brighter corner accents and a small square origin marker. Selected spawns have
stronger outlines and square corner handles. Their label sits on the top edge.
The square represents the real spawn coverage in the map format. This display does not
change spawn behavior, tile flags, or saved map data.

Validation covers mixed-flag coverage, region boundaries, connected labels,
visibility, zero opacity, value bounds, UI interactions and preference persistence.

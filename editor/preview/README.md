# In-game preview module

The offline explorer is intentionally isolated from the map editor input code.

- `IngamePreviewController` owns the logical player position, movement queue,
  collision requests, floor, direction, speed and frame interpolation.
- `IngamePlayerOverlay.qml` presents the local player without modifying map
  data or creating undo entries.
- `IngamePreviewPanel.qml` is only the window, camera binding and input adapter.
- `MapView::isPreviewWalkable()` is the read-only bridge to map and item data.

Movement never edits the OTBM. This keeps preview state independent from map
serialization, undo/redo and normal editor navigation.

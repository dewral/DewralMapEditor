# Palette docking and OTBM validation

## Fluent Dark palette

The Palette, Tools and Brush size sections move together. Dragging the header
starts after an 8-pixel movement threshold, so clicks do not undock the panel.
The panel can dock on the left or right, or float inside the editor window.
The nearest panel edge activates a blue docking preview within 48 pixels of an
edge. This avoids requiring the pointer itself to touch the edge.

Docked panels resize with the adjacent divider; floating panels resize at the
bottom-right corner. Locking prevents movement and resizing without disabling
editing controls. The close button hides the panel; View → Show palette and
Ctrl+B restore it. Hiding a docked panel expands the map workspace.

Qt settings persist side, width, floating coordinates and height, visibility,
and lock state. Floating geometry is constrained to the current editor window.
This is an in-window panel, not a separate native desktop window.

## OTBM safeguards

- Reject unexpected bytes after the root node instead of accepting malformed
  files with trailing data.
- Reject text exceeding the OTBM 16-bit length limit of 65,535 encoded bytes.
  Return a clear error rather than truncating the length and writing corrupt data.
- Regression tests verify that reserved marker bytes survive save/load and that
  a failed oversized-text save preserves the existing map file.

## Validation

Qt 6.10.2 / MinGW x64 on Windows:

- Seven palette docking checks passed, including both sides, floating geometry,
  locking, cancellation, click handling and edge-based docking previews.
- Five OTBM/map suites passed: compact storage, editing storage, background
  loading, map creature/house/zone integration, and document recovery.
- The application built successfully and its offscreen startup smoke test passed.

The OTBM suites use generated fixtures rather than user maps. The startup smoke
test still reports the existing duplicate-method warning in AdvancedBrushEditor.

Build and run the OTBM suites from a configured build directory:

```powershell
cmake --build build/local-release --target otbm_compact_storage_test otbm_editing_storage_test otbm_background_load_test map_creature_refresh_test document_recovery_test --config Release --parallel
ctest --test-dir build/local-release -C Release -R '^(otbm_compact_storage|otbm_editing_storage|otbm_background_load|map_creature_refresh|document_recovery)$' --output-on-failure
```

Build test targets explicitly because they are excluded from the default build.
Ensure the matching Qt runtime is on PATH when using an existing Qt SDK.

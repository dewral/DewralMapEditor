# Classic UI in the Tibia preview

The preview sidebar follows the local Qt/QML 7.72 reconstruction's `GamePanels.qml`,
`NativeUi::chrome`, `ClassicMinimap.qml`, `NativeRangeSlider.qml`, and `NativeScrollBar.qml`.
The existing preview caption and action toolbar keep their previous skin.

- `TibiaPreviewFrame.qml` repeats native strips and corners instead of stretching the finished window.
- `TibiaPreviewLabel.qml` uses the client's bitmap UI, button, and input fonts through the existing UiTheme image provider.
- `TibiaPreviewSidebar.qml` uses the original miniwindow button atlas, native fields/checks/sliders/status bars and classic minimap controls.
- `TibiaPreviewScrollBar.qml` preserves the client's scroll arrows, thumb sizing and hold-to-repeat behavior.
- Minimap floor browsing and compass panning do not move the preview player. Centre resets only the minimap.
- HP/MP bars display 100% in this editor preview; they are not live server statistics.

Unmodified OTCv8-derived assets retain the supplied MIT license. Native sprites/fonts
are lossless exports from the user's supplied original client artwork and retain
that separate provenance. See `editor/qml/themes/tibia/classic/NOTICE.txt`, `LICENSE.txt`
and `ORIGIN.json` for source digests, crop coordinates and unchanged asset hashes.
No full DAT, SPR, OTB or PIC source file is included.

Validation is recorded in the local build report. QML tests use mock map/controller
objects with the real Qt image provider and real client textures/font atlases.

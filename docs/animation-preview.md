# Effects and missile preview

Open **Tools > Effects & Missiles...** after loading a client profile. Select
Effect or Missile, browse the thumbnail list, or enter a client ID. The IDs are
client asset IDs, not server spell constants.

The preview uses the loaded client's ground tiles. Choose a ground client ID,
toggle the tile grid, or zoom from 100% to 400%. One tile is 32 pixels at 100%.
Large sprites retain their full dimensions, layers, bottom-right tile anchor and
DAT displacement. Scroll the preview when zoomed in. The yellow square marks
the origin; the green square marks a missile's destination.

Effects repeat for inspection. Pause, restart, or advance one frame at a time.
The inspector uses the midpoint of each DAT animation-duration range; legacy
assets without duration metadata use 100 ms per frame. This is an inspection
loop, independent of finite-loop counts and randomized starts in the client.

Missiles travel between tiles using the directional sprites from the asset's
compass pattern. Choose any of eight directions, a distance from one to six
tiles, and an illustrative flight time. Asset definitions do not specify server
or client flight timing. A short gap separates repeated flights.

The browser changes no map data, selections, undo history, or saved assets.
Playback stops while the dialog is closed or its window is inactive. Switching
client profiles refreshes both catalogs. Both classic DAT/SPR and modern
appearance catalogs retain missile definitions for this browser.

Validation targets: `animation_preview_test`, `canary_format_test`,
`client_load_service_test`, and `tibia_ui_qml_test` (CTest
`animation_preview_qml`). The latter uses mock asset readers and covers UI
behavior in both themes; it does not establish native GPU rendering correctness.

# In-game preview

`IngamePreviewView` is the QML/QRhi view for the offline explorer. It uses the
same GPU backend as the editor, with a separate ordered scene and sprite atlas.
`IngamePreviewScene` adapts the renderer from the local Tibia772 reconstruction:

- `src/worldscene.cpp`: diagonal tile traversal, ground/bottom/common/actor/top
  passes, 24px elevation cap, native displacement and walking actor placement.
- `src/worldlighting.cpp`: visible floors, projected cover tests, 17x13 light
  lattice for a 15x11 viewport, colored max blending and radial falloff.
- `src/clientassets.cpp`: outfit masks and exact recovered 133-color palette.
- `src/walkanimation.h`: smooth straight motion, 8px foot-phase bands, server
  beat rounding and diagonal input cooldown for 7.72.
- `qml/CreatureInformation.qml` and `src/nativeui.cpp`: fixed screen-pixel
  name/health placement and the original CP1252 bitmap font.

OTBM common items are stored bottom-to-top, whereas the client's network stack
is top-to-bottom; the adapter preserves the resulting painter order. The local
player is drawn between world layers, receives world lighting and can be covered
by foreground items. Names are placed after lighting and remain screen-sized.
Editor selection tint, zone fills and creature visibility switches do not alter
this scene. Existing viewport presets, docking, outfits, weather and screenshots
remain available.

`IngamePreviewController` owns position, movement, collision queries and floor
transitions. Rendering and walking do not edit the OTBM or create undo entries.
The offline world has no server combat events, projectiles, spell effects, HP
updates or timed squares to replay; mapped creatures have a full health bar.
Brightness/color controls supply the offline world's ambient light. As in the
reference client, underground ambient is zero and local lights illuminate it.

## Validation

`map_creature_refresh_test` includes preview layer/elevation/light/roof regression
checks. QML tests cover docking, controls, screenshots and saved preferences.
For an independent image comparison using original 7.72 assets, generate the
reference with reconstruction's `world_scene_tests`, then run:

```
map_creature_refresh_test --preview-reference <Tibia772-assets-directory> <world-scene.png> <output-directory>
```

This compares every CPU reference pixel to the client screenshot, then renders
with QRhi and compares every GPU pixel. Diagnostic PNGs are saved in the output
directory. The reference fixtures and assets are supplied locally, not embedded
in the editor or required to build it. Font provenance and the OTCv8 license are
in `assets/`; the copied palette is the reconstruction's recovered 7.72 data.

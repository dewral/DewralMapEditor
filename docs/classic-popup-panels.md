# Unified Classic panels

The existing Terrain Palette remains the visual baseline. This change is limited
to Tibia-UI chrome and the shared QML container layer. Existing graphics, Verdana
metrics, active-tool highlights and the main layout are retained.

## Components and adoption

- `TibiaPanelHeader`: shared 17px native caption, optional icon, close, lock and
  collapse buttons, inactive text and existing drag/caption signals.
- `TibiaWindowFrame`: original 4/17/4/4 popup texture and blank-area input shield;
  delegates its header instead of implementing another one.
- `TibiaPopupWindow`: embedded/floating variant, controlled collapse, padding,
  intrinsic content height and optional scrolling with existing `DmeScrollBar`.
- `TibiaSurface` strip mode: existing dark texture and two-pixel bevel; transparent
  center when used over the map. Existing button/field/panel modes remain available.
- `TibiaSeparator`: shared two-pixel inset for toolbar and status groups, routed
  through the existing theme-aware `DmeSeparator`.

Palette, Tools, Brush size and Zone display use the common container. Menu,
toolbar, document tabs and status bar use embedded strip chrome. Status fields
have separators instead of individual boxes. The viewport changes only its
transparent frame. Shared `DmeDialog` uses the same caption and popup texture for
Tibia-UI; close rejects the dialog, and NoAutoClose dialogs have no caption close
button. Other themes keep their existing dialog presentation and controls.

The Brush size section gains a collapsible header. It keeps the old button sizes;
its extra caption space comes from the palette grid, not from the map. Palette
search/category signals, tool commands, docking controllers, tooltips, keyboard
shortcuts and existing settings ownership are preserved. No permanent sidebar,
map renderer changes, new images or C++ changes are introduced by this refactor.
Preview-specific client controls and detailed tool content controls retain their
existing skin; this refactor unifies their surrounding editor containers.

## Validation

- Release build using the existing Qt 6.10.2 / MinGW Ninja tree: successful.
- CTest: `tibia_ui_qml` and `classic_panels_qml` passed.
- OpenGL QuickTest: Classic panels 9, Tibia theme 27, Zone display 6, Preview
  docking 21, brush topbar 9 passed (QtTest counts include setup/cleanup).
- Status bar 14 and palette docking 7 passed with software rendering. Status
  thumbnail tests have the existing mock image-provider warning; no failed tests.
- Native DME, real client 1098 and generated fixture map: first complete OpenGL
  frame at 7969 ms; no QML errors, binding loops or missing-resource warnings.
- Rendered and inspected the actual editor at 1668 x 895. Selected Terrain
  Palette through its caption and collapsed/restored Brush size. The map stayed
  in place; only the palette grid grew/shrank. Independently compared workspace
  anchors against the pre-refactor source backup: identical. MapWorkspace changes
  only the transparent frame kind.
- Native generation-library smoke test passed. Source backups and detailed local
  build/test logs are under ignored `build/classic-panels/`.

For repeatable native visual checks, diagnostic `--load-profile` runs optionally
accept `--profile-window-size 1668x895`. Normal startup and saved settings are
unaffected. Component rendering was also inspected at this size in the dedicated
QuickTest fixture; mocks do not replace the separate native application check.

Limitations: this is a container refactor, not a redesign of every tool form.
Floating/docking logic is covered by existing controller and theme tests; it was
not manually exercised in every dialog or every possible display scale.


## Main window follow-up

The main window now uses the same outer popup frame as Terrain Palette, with a
26px application caption. Panel and dialog captions retain their 17px metrics.
The taller header repeats the native caption interior and moves its bottom
separator; original border slices, font metrics and 12px icons are preserved.
Text, icon and window controls are vertically centered. Menu, toolbar, tabs and
status are transparent over one shared body texture, separated by thin lines.

Reference review: the locally extracted Dudantas `TibiaDialog.qml`,
`TibiaStyle-detected.qml` and `TibiaTiledImage.qml` establish frame ownership,
17px captions, 4px edges and repeat tiling. The DME frame PNG is byte-identical
to the locally extracted Dudantas PNG (SHA256
`D116ABE0A6DE490403957330E642D1FD39A7C2DBCF4146544BC4B8543A1E72B2`).

Current validation: Release build passed; OpenGL theme suite 29 and Classic
panels suite 9 passed. A rendered 26px frame with a standard 17px panel was
inspected; caption geometry and close-button input passed. Native DME with the
1098 fixture, a 1668x895 diagnostic window and the real renderer reached its
first complete frame at 8458ms and exited 0. Native generation-library smoke
passed. Logs, current-source backups and the component render are under ignored
`build/classic-tall-caption/`. Other open editors and maps were left running.


## Menu and toolbar frames

The menu row and toolbar again use the existing `TibiaMenuBox` (`menubox.png`,
3px repeat slices). Their dimensions and controls are unchanged. The main title
remains 26px and the outer popup frame is retained; document-tab/status background
behavior is unchanged. The OpenGL theme suite passed all 29 checks after this
adjustment, including toolbar-background rendering and menu/input/resize behavior.


## Standard caption buttons

Main, Palette and Tools caption actions share a grey button face and standard
window glyphs. Main controls use minimize/maximize/restore/close; Palette uses
lock/unlock and close; Tools uses collapse/expand and close. The 26px main title,
17px panel titles, restored menu/toolbar MenuBoxes and all panel positions are
unchanged. Caption and drag hit areas reserve the larger buttons. No new graphic
assets or C++ changes are introduced by this follow-up.

OpenGL validation: theme 29, Classic panels 9 and palette docking 7 checks passed.
The rendered caption/button comparison is stored under ignored
`build/caption-buttons/`; source backups and build logs are in the same directory.


Validation follow-up: Preview docking 21 and weather controls 14 checks passed
on OpenGL. A pre-existing unsupported `validator` assignment on the color field
in the current WeatherControls prevented the whole editor from loading. That
field now validates the same complete #RRGGBB format in its existing commit
handler, using the DmeTextField API, with no weather setting/layout changes.


## Outer edge correction

The square Tibia-UI window already draws its full QML popup border. Its extra
Windows DWM outline is now suppressed with DWMWA_BORDER_COLOR / DWMWA_COLOR_NONE;
other themes restore the system default when rounded corners are enabled.
This applies both on initial HWND creation and on theme changes. Window resize,
Snap, maximize and unsaved-map handling are unchanged.

Reference: https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
The documented no-border value is 0xFFFFFFFE; default is 0xFFFFFFFF.

Release build and native dme_window / dme_window_scaled tests passed (100% and
150% scale), including startup border state, theme switching, native work-area
bounds and caption interaction. Native generation-library startup passed before
and after installation. QML edge-pixel probing also confirmed no texture overflow
outside the frame. This is a native outline fix; a new full-editor screenshot was
not taken for this follow-up. Logs and source backups: build/caption-edge/.


## Main title proportions

Only the application caption uses a 20px DME icon and 14px Verdana title;
standard panel captions keep their existing defaults. Main title height stays
26px. An optional divider height in the extended frame removes the redundant
caption line above the menu MenuBox, which already draws its own top edge.
The EditorScene layout was compared against the source backup: all existing
geometry and anchors match after excluding these caption styling assignments.

Release build, OpenGL theme (29) and Classic panel (9) tests passed. The before/
after component render was inspected. Native 1098 profile at 1668x895 reached a
complete frame at 8346ms and exited 0; generation-library startup passed before
and after EXE installation. Local render/logs/backups: build/title-proportions/.


Caption symbol alignment: the minimize mark is centered in its existing SVG
viewBox, on the same visual axis as maximize/restore and close. Button rectangles,
spacing, title metrics and all panel geometry remain unchanged. This follow-up
changes only the existing minimize.svg asset. OpenGL theme checks (29), Release
build and native startup passed; rendered comparison: build/caption-align/.

Palette selection: Tibia UI now shows a separate Palette dropdown above the
Tileset dropdown. Caption activation no longer opens the palette menu. The
existing kind selection and persisted settings are reused. Palette selection
tests pass (65), including mouse opening, Escape, kind/title/settings updates,
resize/docking and restored Tibia palette/tileset. Release build and native
generation-library smoke pass; native 1098 profile reaches a complete frame
at 7322ms and exits 0. Artifacts: build/palette-kind-selector/.

Unified sliders: shared TibiaSliderSkin covers lighting, zone/layer opacity,
preferences, procedural roughness, brush scale, weather, color picker and preview.
Native arrow buttons preserve stepSize and emit moved only when value changes;
limits, dragging, keyboard input, disabled state and theme restoration are tested.
Theme suite: 30 passed; weather: 14; color picker: 3; preferences: 3; brush
topbar: 9; preview docking: 21. Wall editor: 22 passed, 4 existing layout
failures (418px content in 415px viewport), reproduced with the pre-change
BrushEditorDialog source. Logs and source backups: build/unified-native-sliders/.
Release build, native startup and 1098 map profile passed (first complete frame
at 7850ms). EXE-only installation in DME-dist verified by SHA256 and installed
startup smoke; previous EXE retained.

Status bar design implemented: one 36px MenuBox, 20px inset separators,
24px floor/zoom/timer controls, compact Ready and selection groups, flexible
item summary/name and fixed right-hand groups. Native timer mode shares Verdana
metrics, omits the duplicate task icon and uses a classic button face. Other
themes retain the 44px layout. EditorScene follows the bar implicit height.
Status tests: 34 passed (11 widths in both themes, maximum coordinates, long
duration, common control alignment, item details and native/Fluent actions).
Work timer tests: 12 passed. Release build, native startup and 1098 profile
passed, and installed EXE smoke passed after backup and checksum verification.
Sources and renders: build/statusbar-layout/; design: build/statusbar-design/.

Icon-group toolbar implements the approved icon-only mockup: one 44px MenuBox,
three inset groups with 26px buttons and 18px icons, and right-aligned native
lighting controls without section labels. Tooltips and accessibility names remain.
Narrow windows move lighting to the next row; docked tools use available space
between preview and lighting or a separate scrollable row. Lighting uses one
position binding across themes to avoid opposing anchors overwriting its width.
Theme checks: 31 passed, including six widths, all four dock combinations, theme
switching, native slider input and geometry; palette docking checks: 7 passed.
Artifacts and source backups: build/icon-group-toolbar/.

Toolbar follow-up: the three inset action groups are replaced with one flat
36px panel; existing group gaps and separate lighting panel remain. Native
action buttons are enlarged from 26 to 32px, icons from 18 to 22px. The main
toolbar is 50px tall. Lighting fields are enlarged proportionally, and wrapping/
docking thresholds are derived from the actual action span and lighting width.
Theme suite: 31 passed, including single-panel geometry, theme restoration,
six window widths and all dock combinations. Artifacts: build/flat-icon-toolbar/.

Toolbar panel correction: actions now use the same NativeGroup/field frame as
lighting, preserving the shared panel and enlarged 32px buttons/22px icons.
All eleven action buttons have uniform 8px gaps, including former file/edit/
preview boundaries. End padding is symmetric. Theme suite: 31 passed, comparing
both panel sources and every adjacent gap at six widths and four dock settings.
Artifacts: build/matching-toolbar-panels/.

Tools in tibia-ui now use eight direct SVG assets, muted role colors and no
visible captions. Full descriptions remain in tooltips and accessibility names.
Palette buttons are 40px tall with 24px icons; compact buttons use 22px icons
and the docked toolbar allocation is reduced to 297px. Other themes retain labels.
Lucide attribution and the existing ISC license are bundled with the assets.
The grid column count no longer depends on its derived button width, removing
a theme-transition binding loop. Theme suite: 32 passed; brush/topbar suite: 9
passed; standalone native render: 3 passed, with no QML warnings.
Sources, screenshots and logs: build/tools-svg-icons/.

Classic Tools artwork: the approved original filled SVG set now replaces the
Lucide-style tool assets at the same resource paths. Metallic shields/swords,
rope, stone border, no-logout clock, wand and wooden door keep their existing
button actions, tooltips and selected states. Material shading and pixel-aligned
24px geometry match the inspected Dudantas references. Source backups and logs:
build/tools-classic-install/; design and SVG package: build/tools-classic-design/.
Theme checks: 32 passed.

Small UI improvements: remove the duplicate Palette label above the kind
picker, show live brush diameter/shape in the native Brush size caption and
shape tooltip, and enlarge map-tab close hit targets to 22x18px while preserving
the 12px glyph and 18px tab. Selection hit areas reserve the same close region;
close controls expose tooltips and an accessible action. QML checks: theme 33,
palette selection 65 and brush/topbar 9 passed. Source backups and logs are in
build/ui-small-improvements/.

Brush size reference layout: restore separate square/circle selectors followed
by a spacer and seven size presets in one row. Remove the extra shape/dimension
readout; the fixed Brush size title and size tooltips remain. Selecting a shape
is idempotent. Button sizing now budgets nine buttons plus the spacer, retaining
the responsive one-row behavior. Theme checks: 33 passed; palette checks: 65.
Artifacts: build/brush-layout-reference/.

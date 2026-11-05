# Tibia-UI

Optional client-style skin of the Fluent editor layout. Select **Tibia-UI**
from Preferences / Interface / Theme; its ID is `tibia-ui`.
The palette width is stored separately as `tibiaPaletteWidth`.

## Client graphics

Selected UI slices are copied from the user-provided folder:
`C:/Users/Dewral/Desktop/project-zero-main/things/images`.
Upstream reference: https://github.com/omarcopires/project-zero/tree/main/things/images
No client runtime or client controllers are required by DME.

The application shell, Palette and Tools use the original
client popup frame with 4/17/4/4 slices and a centered Verdana caption, without
a blue title bar. Brush size shares the same compact, collapsible caption. Docking preserves
the sidebar appearance; floating windows shield the map from clicks.

Buttons use the client's grey up/down images; slots use its inventory image;
scrollbars use its arrows, handle and track. Drop-downs and the brightness track
also use client PNG slices. Images repeat at native resolution without smoothing.
The toolbar icons are silver; the selection and brightness fill retain blue accents.

Tools expose the same commands and order as Fluent Dark: Lasso, Border, PZ,
NP, NL, PvP, Auto and Doors. Tibia-UI changes their appearance only, including
when the tools are docked in the compact toolbar.

`TibiaSurface`, `TibiaClassicFrame`, `TibiaWindowFrame` and `TibiaSlot` centralize
surfaces, chrome, window input and inventory appearance. `TibiaSkin` instantiates
its texture nodes only for this theme.

Smoke-test without changing the saved selection:
`DME_UI_STYLE_OVERRIDE=tibia-ui` and `DME.exe --smoke-test`.

Client polish: controls use native-rendered Verdana 11, flat inset field fills,
a single original container-slot border, neutral pressed document tabs, and a
single responsive brush-size row. The 26px application caption repeats the original caption texture between the
fixed top edge and bottom divider. Small panel captions remain 17px. Windows corners remain
square for this theme and follow the normal preference in other themes.

The screenshot layout places the category picker in the left-aligned Palette
caption, integrates Brush size as a collapsible section, separates the status fields,
and adds functional map scrollbars. Click the Palette caption to change category;
the first footer button toggles square/circle brushes.

In-game Preview has a square grey caption and an optional 254px right sidebar.
The minimap follows the preview player (clicks move only that player), and the
collapsible sections manage player options, preview weather, creatures, items,
animation, lighting and borders. Narrow docked previews keep the existing popup
controls. Preview display options never change the editor visibility flags.

Native client verification: the extracted dudantas client TibiaScrollBar.qml uses
12px scrollbars, a 14px minimum handle, 6px endcaps on the scrolling axis, and
stretched handle centers. Only tracks tile. TibiaStyledSlider uses the same
12px horizontal handle. Widget headers use widget-borderimage.png; viewport
frames use TibiaFrame3Pixel's eight original corner/edge images. Checkboxes use
the original 12px state images. Reference extraction is kept in build/reference-dudantas.

MenuBox shell: the application uses menubox.png with 3px slices on every side,
matching OTClient data/styles/10-panels.otui. Sidebar popups instead retain the
client dialog-frame-borderimage.png, including its native caption. Never tile widget-borderimage.png
with narrower side slices: its embedded 4px edge would repeat across the body.
The source texture is from the user's local TibiaClient/data/images/ui/menubox.png.

Classic topbar: the grey client up/down button slices frame the title controls
and toolbar. The editor-specific filled SVG symbols are local to Tibia-UI;
maximized windows show Restore rather than the client's unrelated plus symbol.
Command groups share 38px buttons and 26px symbols. The brightness control uses
an inset client track with a cyan value strip and a 22px client handle. Narrow
windows move lighting controls to a separate row. Other themes retain their icons.

Menus retain the original 3pixel-frame-borderimage.png and native pressed
heading slices. Desktop rows are 20px high with 6px content insets, a shared
checkbox column and a separate shortcut column. Verdana 11 uses native font
rendering without the outline. Menu width follows the widest label and shortcut;
headings keep their content inside the same padded area used for clicking.
The application caption drag area excludes the Tibia-UI menu row.


Workspace popup polish: Palette and Tools use the exact Dudantas client
`dialog-frame-borderimage.png` (4/17/4/4 slices), not a MenuBox with a drawn
caption. Their caption is 17px, with 8px inner spacing and native close/lock
buttons. Sidebar content, caption hit areas and dock handles share these metrics.
Map tabs are 24px high and use native close buttons; the status strip is 32px.
These sizes apply only to Tibia-UI. Fluent Dark keeps its own geometry.


## Shared embedded windows

`TibiaPanelHeader` owns the 17px caption, icon, close/collapse/lock actions and
native drag signals. `TibiaWindowFrame` retains the original 4/17/4/4 texture
slices and blank-area input shield. `TibiaPopupWindow` adds a padded content
host, intrinsic sizing and optional scrolling through the existing `DmeScrollBar`.
Docked and floating panels use this same frame; their existing controller still
owns position, locking and persistence. `collapsed` is controlled by the owner:
handle `collapseRequested` to update it. For fill-anchored content, set
`contentHeight` explicitly; intrinsic Columns can use the default children bounds.

Palette, Tools, Brush size and Zone display share these components. `DmeDialog`
uses the same caption and frame only for Tibia-UI; its modal policy and remembered
position are unchanged. Other themes retain their existing controls.

`TibiaSurface.kind: "strip"` provides a two-pixel bevel using the existing dark
texture for embedded menu/toolbar/tab/status containers. With `frameOnly: true`
its center stays transparent over the map. `TibiaSeparator` provides the matching
two-pixel inset between tool groups and status fields. Toolbar/status/tab heights,
map anchors, renderer, palette width and compact topbar controls are unchanged.
The Brush size caption adds a small amount of height within the palette; the
item grid yields that space, not the map viewport. No new graphics are required.


Main window: one outer `TibiaWindowFrame` supplies the shared textured body.
The menu and toolbar each use the existing three-pixel `TibiaMenuBox` frame.
The document-tab row and status bar expose the shared body background, with thin
separators and their existing controls. The main caption
is 26px high with vertically centered native buttons and text. `headerHeight`
extends the original 17px caption by tiling its interior and moving the divider,
without changing the 4/17/4/4 source slices or scaling native icons. Ordinary
palette/dialog captions keep their 17px size. Existing native window actions,
Snap button integration and unsaved-map confirmation remain in their owners.


Caption actions use `TibiaCaptionButton`: existing grey up/down button slices
and the existing standard SVG window glyphs. Main-window hit targets are 24x26
with 22x20 faces; 17px panel captions retain compact 16x14 faces. Close, collapse,
lock/unlock and native maximize/restore keep their original state owners. The
palette lock now uses the actual lock/unlock icon rather than plus/minus. Caption
text and dock drag handles reserve the complete button width, preventing overlap.

All numeric sliders use TibiaSliderSkin in tibia-ui: the same native scrollbar
track, 28x12 thumb and 12px repeating arrow buttons. It decorates the existing
Slider, which keeps values, dragging, keyboard input and moved handlers.
Conditional bindings restore the original geometry and visuals on theme change.
The preview scrollbar uses the same vertical assets in tibia-ui.

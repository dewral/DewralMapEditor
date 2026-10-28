# Tibia-UI

Optional client-style skin of the Fluent editor layout. Select **Tibia-UI**
from Preferences / Interface / Theme; its ID is `tibia-ui`.
The palette width is stored separately as `tibiaPaletteWidth`.

## Client graphics

Selected UI slices are copied from the user-provided folder:
`C:/Users/Dewral/Desktop/project-zero-main/things/images`.
Upstream reference: https://github.com/omarcopires/project-zero/tree/main/things/images
No client runtime or client controllers are required by DME.

The application shell uses MenuBox textures. Palette and Tools use the original
client popup frame with 4/17/4/4 slices and a centered Verdana caption, without
a blue title bar. Brush size is a compact headless footer. Docking preserves
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
single responsive brush-size row. The enlarged application caption uses repeating
native-size frame slices without scaling the texture. Windows corners remain
square for this theme and follow the normal preference in other themes.

The screenshot layout places the category picker in the left-aligned Palette
caption, integrates Brush size as a horizontal footer, frames the status groups,
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
These sizes apply only to Tibia-UI. Other editor themes keep their geometry.

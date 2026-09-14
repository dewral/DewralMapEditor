# Dewral Map Editor

Dewral Map Editor (DME) is a desktop OpenTibia map editor for OTBM maps. It
uses a Qt 6/QML interface and a custom QRhi renderer designed for large,
multi-floor Tibia maps.

> **Development note:** This project was developed primarily with assistance
> from Claude Code Fable 5, Claude Opus 4.8, and ChatGPT 5.6 Sol.

> Client assets are not included. DME requires your own `Tibia.dat`,
> `Tibia.spr`, and `items.otb` files. These files are copyrighted by CipSoft
> and must not be committed to this repository or included in a release.

![Dewral Map Editor with an OTBM map open](docs/screenshots/editor-overview.png)

## Highlights

- Opens and saves OTBM maps for Tibia 7.60 through 10.98+ client profiles.
- Supports custom client profiles and remembers a separate asset directory for
  each profile.
- Uses an instanced QRhi renderer with chunk caching, smooth pan and zoom,
  multiple visible floors, lighting preview, and optional item animations.
- Provides terrain, doodad, item, creature, house, and custom palettes.
- Includes ground brushes, automatic borders, wall brushes, doodad variants,
  zone tools, spawns, creatures, houses, and towns.
- Supports selection, multi-floor selection, moving items between floors,
  cut/copy/paste, undo/redo, item properties, search, and replacement tools.
- Includes a configurable In-game Preview window. [ WIP ]
- Loads maps even when optional house and spawn sidecar files are absent.
- Lets you switch between the modern GitHub-inspired interface and the
  Tibia-inspired Classic UI from the theme settings.
- Produces a self-contained Windows release folder and ZIP archive.

## Screenshots

### Client profiles

Configure a built-in client version or point a custom profile at your own
client assets.

![DME startup window with a configured custom client profile](docs/screenshots/startup-client-profile.png)

### Brush Editor

Create and organize custom brushes directly in the editor.

![Creating and editing a brush in the DME Brush Editor](docs/screenshots/brush-editor.png)

### Classic UI

The interface can be changed at any time in the theme settings. Choose the
modern GitHub-inspired layout or the textured, Tibia-inspired Classic UI.

![Dewral Map Editor using the Tibia-inspired Classic UI](docs/screenshots/classic-interface.png)

## Download and run

1. Download `DewralMapEditor-windows-x64.zip` from the
   [GitHub Releases page](https://github.com/dewral/DewralMapEditor/releases).
2. Extract the complete archive. Do not move `DME.exe` away from the DLL and
   `qml` folders shipped next to it.
3. Run `DME.exe`.
4. Select a client version or create a custom profile.
5. Point the profile at a directory containing:

   ```text
   Tibia.dat
   Tibia.spr
   items.otb
   ```

6. Open an OTBM map.

House and spawn XML sidecar files are optional. When present, DME loads them
with the map. When absent, the map still opens normally.

### Release files

Each release provides two independent archives:

- `DewralMapEditor-windows-x64.zip` — ready-to-run Windows application with
  `DME.exe`, the required Qt runtime, built-in editor data, licenses, and
  documentation. Users do not need Qt Creator or a compiler.
- `DewralMapEditor-source.zip` — complete source package without executables,
  DLLs, build output, or client assets. It contains the CMake/vcpkg build files
  and the generated RME material profiles.

## Updates

DME checks for updates whenever the start window opens. That window shows the
current status and, when a newer build is available, a **Download & install** button.
Update channels follow the installed build:

- **Stable builds** download normal published GitHub releases.
- **Development builds** follow successful nightly builds of the `main` branch.

The nightly package is built and published automatically after each push. DME
downloads the portable ZIP, verifies its SHA-256 digest, and hands installation
to the separate `DMEUpdater.exe`. Modified maps must be saved before restarting;
user settings, custom client profiles, and recovery files are not stored in the
application folder and are not replaced.

Do not download only `DME.exe`. The executable requires the DLL and QML files
that are shipped beside it in the ready-to-run archive.

## Build on Windows without Qt Creator

The supported release build uses Visual Studio 2022, CMake, and a pinned vcpkg
manifest. Qt Creator and a separately installed Qt SDK are not required.

### Requirements

- Windows 10 or Windows 11, x64
- [Git for Windows](https://git-scm.com/download/win)
- [CMake 3.24 or newer](https://cmake.org/download/)
- Visual Studio 2022 Community or Build Tools 2022 with the
  **Desktop development with C++** workload
- An internet connection for the first dependency build

The first build downloads vcpkg and builds Qt 6.10.2. This can take a long time
and use significant disk space. Later builds reuse the local vcpkg binary
cache.

### One-command build

Clone the repository and run:

```powershell
.\build-release.bat
```

The script:

1. verifies Git, CMake, Visual Studio, and the MSVC C++ toolchain;
2. downloads the pinned vcpkg release into `.tools/vcpkg`;
3. installs the dependencies declared in `vcpkg.json`;
4. configures the `windows-vcpkg` CMake preset;
5. builds the Release configuration;
6. deploys only the required Qt runtime;
7. creates the ready-to-run and source folders and ZIP archives.

Build output:

```text
release/
|-- DewralMapEditor-windows-x64/
|   |-- DME.exe
|   |-- data/
|   |-- qml/
|   |-- LICENSE
|   |-- NOTICE
|   |-- README.md
|   `-- required Qt runtime files
|-- DewralMapEditor-windows-x64.zip
|-- DewralMapEditor-source/
`-- DewralMapEditor-source.zip
```

Local custom profiles and client binaries are removed from the release
package automatically.

### Build from PowerShell

The batch file is only a convenient launcher. The same build can be started
directly:

```powershell
.\scripts\build-release.ps1
```

After vcpkg has been bootstrapped, the underlying commands are:

```powershell
$env:VCPKG_ROOT = "$PWD\.tools\vcpkg"
cmake --preset windows-vcpkg
cmake --build --preset windows-release
```

### Build with an existing Qt installation

Developers who already have a compatible Qt 6 SDK may use it directly.
The renderer requires Qt 6.7 or later, including Shader Tools (`qsb`) and
GuiPrivate development headers. Qt 6.10.2 is the currently validated SDK:

```powershell
cmake -S . -B build/local-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.2/mingw_64

cmake --build build/local-release --target package_release --parallel
```

Use a compiler matching the selected Qt SDK. For example, a MinGW Qt package
must be built with the corresponding MinGW toolchain.

## Dependencies

The vcpkg manifest pins its registry baseline for repeatable dependency
resolution and installs:

- `qtbase`
- `qtdeclarative`
- `qtshadertools`
- `qtsvg`

The `qtbase` feature set is limited to the GUI, network, OpenGL, and PNG
components used by DME (including the optional QRhi OpenGL backend). The pinned
port set provides Qt 6.10.2. Build and deploy with the same Qt SDK because QRhi
does not guarantee compatibility across Qt minor versions.

See [QRhi rendering](docs/qrhi-rendering.md) for backend selection and tests.

The official Windows preset uses the release-only `x64-windows-release`
triplet to avoid compiling and storing an unused Debug copy of Qt. It targets
Visual Studio 2022. vcpkg manifest mode is activated through the CMake
toolchain specified in `CMakePresets.json`.

## Updating the built-in RME data

Built-in brushes, doodads, item groups, and RAW/terrain tilesets are generated
from the text material definitions in
[OTAcademy/RME](https://github.com/OTAcademy/RME). Client binaries are never
copied.

After cloning RME, regenerate every supported profile with:

```powershell
python scripts/import-rme-data.py --rme C:\path\to\RME
```

The importer writes `brushes.json`, `tilesets.json`, `items.xml`, and
`creatures.xml` for the built-in profiles. RME does not provide separate
material directories for Tibia 7.72, 7.80, or 7.92, so those profiles use the
conservative RME 7.60 material set. All other profiles use their matching RME
directory.

## Useful controls

| Action | Control |
|---|---|
| Toggle draw/select mode | `Space` |
| Change floor | `+` / `-` or `Ctrl` + mouse wheel |
| Zoom | Mouse wheel |
| Pan | Middle mouse button or arrow keys |
| Fast keyboard pan | `Shift` + arrow keys |
| Undo / redo | `Ctrl+Z` / `Ctrl+Shift+Z` |
| Copy / cut / paste | `Ctrl+C` / `Ctrl+X` / `Ctrl+V` |
| Rotate doodad variant | `R` |
| Go to position | `Ctrl+G` |

While dragging an item, changing floors keeps the item attached to the cursor
and drops it on the active floor.

## AI Map Assistant

The experimental AI Map Assistant can plan ground terrain inside a selection.
It sends the selected tile coordinates, current ground brush names, and the
available ground brushes to the OpenAI Responses API. The API key is read only
from the process environment and is never stored in a map or project file.

Start DME from PowerShell with:

```powershell
$env:OPENAI_API_KEY = "your-api-key"
.\dist\DME.exe
```

Optionally set `OPENAI_MODEL`; the default is `gpt-5.6-luna`. In DME, select up
to 1024 tiles on the current floor and choose **Edit > AI Map Assistant**. The
assistant creates a validated plan first and changes the map only after
**Apply** is pressed. The complete change is one undo step.

## Learned terrain generation

The terrain generator can learn a reusable style profile from an existing OTBM
map. Open **Tools > Terrain Generator**, choose **Learn OTBM**, and save the JSON
profile. The saved profile records terrain proportions, neighbouring-ground
transitions, continuity, and weighted doodad usage. World generation then tests
several deterministic candidates, selects the closest match, and previews both
terrain and learned decorations before applying them as one undo step.

The **Organic Cave** mode removes tiny isolated regions and connects meaningful
chambers with walkable tunnels. Candidate search is automatically limited for
very large selections to keep preview generation responsive.

In **Dungeon Generator > Organic cave**, **Min. cave region** controls the
minimum retained floor-region size in tiles. **Rock island cutoff** fills
enclosed rock regions smaller than the given size (0 disables cleanup), while
preserving rock connected to the selection boundary. **Corridor width** also
controls tunnels connecting cave chambers, including even widths.

## Ground cluster generator

Open **Tools > Ground Cluster Generator** to use a random cluster brush like the
layered RME workflow. No map selection is required. A single irregular cluster
follows the cursor while the generator window remains open. Its radius is drawn
from the configured minimum/maximum range. Left click places the visible cluster
and immediately creates a new size and shape; right click stops the preview.
Changing the seed, ground weights, radius, irregularity, or doodad density
updates the cursor preview. Borders adapt to the ground below the cursor and
every placement is one undoable operation. Optional doodads stay inside the
generated cluster. Ground values are normalized to 100% and allocated inside
each blob as connected patches, so a 50/50 mix produces approximately half of
each ground in the same cluster. DME places managed border items along internal
transitions and uses each brush's standard border on the outside edge. It does
not add ground tiles outside the generated shape.

The same window can generate a reusable prefab pack. Choose the ground mix,
enter a category and base name, and select the number of variants. DME rolls a
different organic shape for every variant and saves its ground tiles, internal
transitions, normal outer borders, and optional doodads as one prefab. Saved
variants appear in their Doodads palette category, where they can be attached to
the cursor, rotated in 90-degree steps with **Z**, and pasted repeatedly.

Organic caves carve the selected cave floor into a prepared mountain-ground
area without a wall brush. Uncarved ground remains in place and DME recomputes
ground borders when applying. The generator dialog shows brush thumbnails in
the selectors and their dropdowns. Add several cave decorations to the explicit
decoration list to use only those brushes; remove an entry with its × button.

## Reusable roads

For reusable roads, save selected segments using **Add Prefab**, then open
**Generate Path from Prefabs** and choose straight/corner/end segments. The new
**Preserve existing ground** option defaults on and excludes prefab ground
items from both the preview and placement. Turn it off to paste a complete
road including its ground. The placed road can be selected, copied, or saved
as another prefab. Placement is grouped into one Undo operation.

## Project structure

```text
DewralMapEditor/
|-- CMakeLists.txt             application and package targets
|-- CMakePresets.json          shareable Windows/vcpkg presets
|-- vcpkg.json                 pinned C++ dependency manifest
|-- build-release.bat          double-click release build
|-- scripts/
|   `-- build-release.ps1      validated command-line build
|-- cmake/                     Qt deployment and release packaging
|-- data/                      built-in brushes and palette definitions
|-- editor/
|   |-- core/                  application services and models
|   |-- map/                   editing, input, chunk cache, and renderer bridge
|   |-- qml/                   windows, components, controllers, and dialogs
|   `-- ui/                    bundled UI textures and icons
|-- libs/
|   `-- otformats/             DAT, SPR, OTB, OTFI, and OTBM readers
`-- docs/
    `-- screenshots/           README images supplied by the maintainer
```

## Troubleshooting

### Visual Studio C++ tools were not found

Open Visual Studio Installer, modify Visual Studio or Build Tools 2022, and
enable **Desktop development with C++**.

### CMake cannot find the vcpkg toolchain

Use `build-release.bat`, or set `VCPKG_ROOT` before configuring:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-vcpkg
```

### The first build appears to be stuck

Qt is being compiled by vcpkg. The first build is substantially slower than
normal DME rebuilds. Check CPU and disk activity before stopping it.

### Windows shows an old or missing application icon

Windows may cache executable icons. Refresh Explorer, rename the extracted
folder, or clear the Windows icon cache. The release executable contains a
native multi-size Windows icon.

## Data and credits

- DME is inspired by
  [Remere's Map Editor](https://github.com/hampusborgos/rme).
- Portions of the binary-format I/O implementation are derived from
  [Tibia ImGui Map Editor](https://github.com/Open-Tibia-Tools/tibia-imgui-map-editor),
  which is licensed under the GNU Affero General Public License v3.0.
- Lighting and in-game visualization ideas were also informed by Tibia ImGui
  Map Editor.
- Some brush, border, tileset, and creature definitions are derived from
  OpenTibia/RME-compatible data.
- Tibia is a trademark of CipSoft GmbH. Client data and artwork are not
  distributed with DME.

## License

Dewral Map Editor is free software licensed under the
[GNU Affero General Public License v3.0](LICENSE). See [NOTICE](NOTICE) for
third-party attribution.

Release binaries must be accompanied by access to the complete corresponding
source code for the same version. Tibia client files and artwork are not part
of this project and are not covered by this license.

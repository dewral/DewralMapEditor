# QRhi rendering

DME's map viewport uses `MapRhiView` (`QQuickRhiItem`). Qt Quick chooses
the graphics backend; the application no longer forces desktop OpenGL.
On Windows the default is Direct3D 11. For explicit comparison, start a new
process with `QSG_RHI_BACKEND=d3d11` or `QSG_RHI_BACKEND=opengl`.
QRhi also supports Vulkan and Metal when the Qt build and platform support
them. This change supplies the rendering foundation for a mobile port; it
does not add Android/iOS packaging or a touch interface.

## Components

- `maprhiview.cpp` synchronizes map state while the GUI thread is blocked.
  It preserves chunk versioning, instancing, floor ordering, lighting,
  overlay invalidation, the scene cache with a viewport margin, and separate
  pointer rendering. The existing frame and animation timers remain in use.
- `maprhibackend.cpp` owns QRhi textures, buffers, samplers, pipelines and
  resource bindings. Uploads happen before render passes. Draw uniforms use
  aligned dynamic offsets so different floors and overlays cannot overwrite
  one another's parameters within a frame.
- `mapview_render.cpp` prepares geometry and light grids without graphics API
  calls. Its `render*` methods are independent of OpenGL.
- `shaders/map.vert` and `map.frag` are baked by Qt Shader Tools into GLSL ES
  3.0, desktop GLSL 330, HLSL 5.0 and Metal shader variants.

World coordinates increase downward. Projection uses QRhi's clip-space
correction matrix; sampling the cached render target additionally accounts
for framebuffer orientation. The map output stays opaque when composed
by Qt Quick.

## Atlas and resource lifetime

Each viewport owns its GPU atlas. A preview in another window never receives
a native graphics handle from the main window. `MapAtlasService::uploadSince`
provides independent snapshots: a full image for a new atlas epoch/device,
then affected rows for append-only sprite additions. Reading an update does
not consume it for another renderer.

The service retains one authoritative CPU atlas for late previews and device
recreation. This uses more CPU memory than discarding the image after the
first OpenGL upload. GPU buffers also retain their staging data for resource
recreation. Atlas paging and a tighter mobile memory budget remain future
work; the atlas is still a single texture and must fit the device's maximum
texture size. Desktop map-size performance should be measured before release.

VSync is configured when the application creates its swapchains. Changing
the preference requires a restart; no WGL extension is used. Line overlays
use portable one-pixel line primitives instead of implementation-dependent
OpenGL wide lines.

## Build and validation

Use Qt 6.7 or later with GuiPrivate headers and Shader Tools. This branch is
validated with Qt 6.10.2 / MinGW on Windows. QRhi has limited compatibility
guarantees: build and deploy against the same Qt SDK.
The OpenGL fallback requires desktop OpenGL 3.3 or OpenGL ES 3.0 for the
shader variants and instanced vertex layout. Windows deployment includes
the Direct3D shader compiler used by the default D3D11 backend.

```powershell
cmake --build build/QRhi-Check --target DewralMapEditor map_rhi_backend_test atlas_background_build_test --parallel
$env:QT_FORCE_STDERR_LOGGING = '1'
ctest --test-dir build/QRhi-Check -R 'map_rhi_|atlas_background_build' --output-on-failure
```

The GPU tests run on real QRhi backends and read pixels back. They cover
instanced sprites, selection and lighting, overlay order, framebuffer
orientation, cached scene reuse and panning, atlas updates, sampler changes,
light texture resizing, uniform buffer growth and cache recreation. A
`QQuickRenderControl` test also verifies `QQuickRhiItem` composition.
The atlas test checks independent consumers, late previews and reset.

Windows tests cover Direct3D 11 and OpenGL through QRhi. Vulkan, Metal and
physical Android/iOS devices still require platform validation. Before a
release, compare representative real maps at multiple zoom levels, editing
strokes, animation, selection/lasso, lighting and a separate preview window.

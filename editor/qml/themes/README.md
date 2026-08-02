# DME UI themes

Application screens use neutral controls from `qml/style/Dme*.qml`.

- `themes/classic/controls` owns Classic/Tibia textures and metrics.
- `themes/github/controls` owns GitHub UI colors, radii and metrics.
- Shared screens must not reference `Classic*`, `Github*` or legacy `Tibia*`
  controls directly.
- Theme-specific shells such as `GithubEditorToolBar` and the classic
  `EditorToolBar` may use their own theme controls directly.
- A `Dme*` router must instantiate only the active implementation. Do not put
  both skins in the same component and switch them with `visible`.

Both themes can be smoke-tested without changing saved user settings:

```text
DME_UI_STYLE_OVERRIDE=classic
DME_UI_STYLE_OVERRIDE=github-dark
```

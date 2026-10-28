# Weather preview

Open **View > Weather settings...** and choose Rain, Storm, Snow, Mist, Autumn
leaves or Fireflies. Presets can be mixed with the individual intensity sliders.
Wind controls drift, Quality controls the particle/geometry budget, and Pause
freezes the animation. View > Weather preview toggles the entire effect without
forgetting the mix. Weather is off by default; settings are stored locally.

The editor and In-game Preview have independent, locally saved weather settings.
Use the **Weather** button in the Preview toolbar to enable effects, choose presets,
change intensity, wind and quality, or pause that view alone. Both start disabled.
The settings panel scrolls when the Preview window is short.

The effects use
a viewport-stabilized particle layer and world-anchored fog, follow the corresponding
camera and zoom, and do not intercept map input. Closing the settings window keeps the selected weather running.

Outdoors only hides weather below ground and beneath the diagonally projected
upper-floor tiles. Fog is clipped to exposed tiles rather than rendered as a
full-window translucent layer. Disable this switch to preview indoor effects.
Empty areas outside the map are excluded in either mode.

This is a local visual preview. It does not add map attributes, change map dirty
state, send network messages, or configure weather on a server/client. The earlier
client module in TibiaClient/modules/game_weather was used as a reference for
available effects and controls; its client-specific runtime is not required.

## Rendering and lifecycle

The native Qt Quick scene graph receives a batch of premultiplied, vertex-colored
triangles for fog, rain streaks, soft snow flakes, leaves and glowing fireflies.
There is no QML item per particle. Storm increases rain speed/length and can be
combined with fog. No sound or flashing overlay is generated.

Animation uses elapsed monotonic time; long frames are capped at 100 ms. Camera
updates never restart an already-running frame timer. Precipitation compensates
camera motion to keep its screen velocity independent of walking, while fog stays
anchored to the map. Particles have staggered lifetimes, smooth fading and reuse
pool entries on expiry. Changing a mix retains existing particles where possible. Quality bounds particle count (global maximum 1600),
fog sampling and update frequency. Geometry is capped at 200000 vertices. Overview
zooms below 25% hide effects. Inactive, hidden and minimized windows stop their
timers, as do disabled and paused effects. The weather frame rate respects the
configured rendering limit and never exceeds 60 FPS.

Tests cover mix limits, invalid values, monotonic timing, long-frame recovery,
camera compensation, lifetime recycling, all six effects, fog clipping and unchanged map state. A real
scene-graph render test compares outdoor and roof pixels and verifies disabling
restores the baseline. It also moves the camera every 5 ms and checks that weather
time continues advancing and pause still freezes it. QML tests cover presets, mixing, pause, wind, quality,
roof controls, camera selection and four UI themes.

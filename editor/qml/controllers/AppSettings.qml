import QtCore

Settings {
    property string clientFolder: ""
    property string clientPathsJson: "{}"
    property string customProfilesJson: "[]"
    property string mapProfilesJson: "{}"
    property string recentMapsJson: "[]"
    property string customPalettesJson: "{}"
    property bool showStartup: true
    property bool autosaveEnabled: true
    property int autosaveIntervalMinutes: 3
    property int glMaxFps: 60
    property bool glMaxFpsConfigured: false
    property bool vsyncEnabled: true
    property bool showClientBox: false
    property bool showTooltips: true
    property bool showWaypoints: true
    property bool showIngamePreviewWindow: false
    property bool ingamePreviewFollowCursor: true
    property bool ingamePreviewLighting: true
    property int ingamePreviewWidthTiles: 15
    property int ingamePreviewHeightTiles: 11
    property int paletteWidth: 390
    property bool paletteCollapsed: false
    property int iconSize: 66
    property bool githubLayoutV2Initialized: false
}

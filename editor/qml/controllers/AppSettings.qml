import QtCore

Settings {
    property string clientFolder: ""
    property string clientPathsJson: "{}"
    property string customProfilesJson: "[]"
    property string mapProfilesJson: "{}"
    property string recentMapsJson: "[]"
    property string customPalettesJson: "{}"
    property string recentBrushesJson: "{}"
    property string favoriteBrushesJson: "{}"
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
    property string paletteViewMode: "grid"
    property int undoLimit: 1000
    property bool githubLayoutV2Initialized: false
    property bool checkUpdatesAutomatically: true
    property string updateChannel: "stable"
}

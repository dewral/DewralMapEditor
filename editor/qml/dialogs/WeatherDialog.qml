pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import "../style"

DmeDialog {
    id: dialog
    objectName: "weatherDialog"
    required property var settings
    property string settingsPrefix: "weather"
    function preset(index) { controls.preset(index); }
    function settingKey(key) { return settingsPrefix + key.slice(7); }
    title: settingsPrefix === "weather" ? "Map weather" : "Preview weather"
    width: Math.min(510, Controls.Overlay.overlay ? Controls.Overlay.overlay.width - 24 : 510)
    height: Math.min(implicitHeight, Controls.Overlay.overlay ? Math.max(200, Controls.Overlay.overlay.height - 24) : implicitHeight)
    modal: false; dim: false
    contentItem: Controls.ScrollView {
        id: scroll
        implicitWidth: 478
        implicitHeight: column.implicitHeight
        contentWidth: availableWidth
        clip: true
        Column {
            id: column
            width: scroll.availableWidth; spacing: 12
            WeatherControls {
                id: controls
                width: parent.width
                settings: dialog.settings; settingsPrefix: dialog.settingsPrefix
                description: dialog.settingsPrefix === "weather" ? "Local weather test on the editor map. Region definitions stay unchanged." : "Weather for In-game Preview only. Editor weather stays unchanged."
            }
            DmeButton { text: "Close"; width: 100; anchors.right: parent.right; onClicked: dialog.close() }
        }
    }
}

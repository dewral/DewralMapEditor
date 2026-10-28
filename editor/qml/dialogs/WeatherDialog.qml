pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import "../style"

DmeDialog {
    id: dialog
    objectName: "weatherDialog"
    required property var settings
    property string settingsPrefix: "weather"
    function settingKey(key) { return settingsPrefix + key.slice(7); }
    readonly property color foreground: windowsClassic ? "#202020" : "#e0e0e0"
    readonly property color muted: windowsClassic ? "#595959" : "#a9afb5"
    readonly property color accent: "#4a9fc2"
    title: settingsPrefix === "weather" ? "Map weather" : "Preview weather"
    width: Math.min(510, Controls.Overlay.overlay ? Controls.Overlay.overlay.width - 24 : 510)
    height: Math.min(implicitHeight, Controls.Overlay.overlay ? Math.max(200, Controls.Overlay.overlay.height - 24) : implicitHeight)
    modal: false
    dim: false
    component WeatherSlider: Controls.Slider {
        id: slider
        padding: 0
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth; height: 3; radius: 2; color: "#45494d"
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: dialog.accent }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: 12; height: 12; radius: 6
            color: slider.pressed ? "#80bdd8" : dialog.accent
            border.color: "#83bdd5"; border.width: 1
        }
    }
    function preset(index) {
        const mixes = [[0,0,0,0,0,0],[55,0,0,12,0,0],[0,80,0,25,0,0],
                       [0,0,65,18,0,0],[0,0,0,75,0,0],[0,0,0,8,65,0],[0,0,0,8,0,75]];
        const keys = ["weatherRain","weatherStorm","weatherSnow","weatherFog","weatherLeaves","weatherFireflies"];
        for (let i=0;i<keys.length;++i) settings[settingKey(keys[i])]=mixes[index][i];
        presetPicker.currentIndex = index + 1;
        settings[settingsPrefix + "Enabled"] = index !== 0;
        settings[settingsPrefix + "Paused"] = false;
    }
    contentItem: Controls.ScrollView {
        id: weatherScroll
        implicitWidth: 478
        implicitHeight: weatherControls.implicitHeight
        contentWidth: availableWidth
        clip: true
        Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
        Column {
            id: weatherControls
            width: weatherScroll.availableWidth
            spacing: 12
            Text {
                width: parent.width
                text: dialog.settingsPrefix === "weather"
                      ? "Weather for the editor map. Preview has its own settings. Map files stay unchanged."
                      : "Weather for In-game Preview only. Editor weather stays unchanged."
                color: dialog.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
            Row {
                spacing: 16
                DmeCheckBox {
                    objectName: "weatherEnabled"
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Enable weather"
                    checked: dialog.settings[dialog.settingsPrefix + "Enabled"]
                    onClicked: dialog.settings[dialog.settingsPrefix + "Enabled"] = !checked
                }
                DmeButton {
                    objectName: "weatherPause"
                    width: 100; text: dialog.settings[dialog.settingsPrefix + "Paused"] ? "Resume" : "Pause"
                    enabled: dialog.settings[dialog.settingsPrefix + "Enabled"]
                    onClicked: dialog.settings[dialog.settingsPrefix + "Paused"] = !dialog.settings[dialog.settingsPrefix + "Paused"]
                }
            }
            DmeComboBox {
                id: presetPicker
                objectName: "weatherPreset"
                width: parent.width
                model: ["Custom mix / presets...", "Clear sky", "Rain", "Storm", "Snow", "Mist", "Autumn leaves", "Fireflies"]
                currentIndex: 0
                onActivated: index => { if (index > 0) dialog.preset(index - 1); }
            }
            Rectangle { width: parent.width; height: 1; color: "#414141" }
            Repeater {
                model: [{label:"Rain",key:"weatherRain"},{label:"Storm",key:"weatherStorm"},
                        {label:"Snow",key:"weatherSnow"},{label:"Fog",key:"weatherFog"},
                        {label:"Leaves",key:"weatherLeaves"},{label:"Fireflies",key:"weatherFireflies"}]
                delegate: Row {
                    id: effect
                    required property var modelData
                    width: weatherControls.width; spacing: 12
                    Text {
                        width: 75; anchors.verticalCenter: parent.verticalCenter
                        text: effect.modelData.label; color: dialog.foreground; font.pixelSize: 12
                    }
                    WeatherSlider {
                        objectName: effect.modelData.key
                        width: Math.max(60, weatherControls.width - 149); height: 24; from: 0; to: 100; stepSize: 1
                        palette.highlight: dialog.accent
                        value: dialog.settings[dialog.settingKey(effect.modelData.key)]
                        onMoved: {
                            dialog.settings[dialog.settingKey(effect.modelData.key)] = Math.round(value);
                            presetPicker.currentIndex = 0;
                        }
                    }
                    Text {
                        width: 50; anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(dialog.settings[dialog.settingKey(effect.modelData.key)]) + "%"
                        color: dialog.muted; font.pixelSize: 12
                    }
                }
            }
            Row {
                spacing: 12
                Text { width: 75; anchors.verticalCenter: parent.verticalCenter; text: "Wind"; color: dialog.foreground; font.pixelSize: 12 }
                WeatherSlider {
                    objectName: "weatherWind"
                    width: Math.max(60, weatherControls.width - 185); height: 24; from: -160; to: 160; stepSize: 5
                    palette.highlight: dialog.accent
                    value: dialog.settings[dialog.settingsPrefix + "Wind"]
                    onMoved: dialog.settings[dialog.settingsPrefix + "Wind"] = Math.round(value)
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: dialog.settings[dialog.settingsPrefix + "Wind"] + " px/s"; color: dialog.muted; font.pixelSize: 12 }
            }
            Row {
                spacing: 12
                Text { width: 75; anchors.verticalCenter: parent.verticalCenter; text: "Quality"; color: dialog.foreground; font.pixelSize: 12 }
                DmeComboBox {
                    objectName: "weatherQuality"
                    width: 200; model: ["Low", "Balanced", "High"]
                    currentIndex: dialog.settings[dialog.settingsPrefix + "Quality"]
                    onActivated: index => dialog.settings[dialog.settingsPrefix + "Quality"] = index
                }
            }
            DmeCheckBox {
                objectName: "weatherRoofs"
                text: "Outdoors only (respect roofs)"
                checked: dialog.settings[dialog.settingsPrefix + "RespectRoofs"]
                onClicked: dialog.settings[dialog.settingsPrefix + "RespectRoofs"] = !checked
            }
            Text {
                width: parent.width
                text: "Animation pauses in inactive windows. Effects are hidden below 25% zoom to keep overview maps responsive."
                color: dialog.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
            }
            DmeButton { text: "Close"; width: 100; anchors.right: parent.right; onClicked: dialog.close() }
        }
    }
}

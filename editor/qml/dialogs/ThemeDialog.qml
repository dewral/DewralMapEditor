import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: root

    title: "Appearance"
    width: 320

    readonly property color draft: Qt.rgba(rField.value / 255, gField.value / 255, bField.value / 255, 1)

    function loadFromTheme() {
        rField.value = Math.round(Backend.uiTheme.tint.r * 255);
        gField.value = Math.round(Backend.uiTheme.tint.g * 255);
        bField.value = Math.round(Backend.uiTheme.tint.b * 255);
    }
    onAboutToShow: loadFromTheme()

    contentItem: Column {
        spacing: 10

        Text {
            text: "Application style"
            color: Backend.uiTheme.style === "github-dark" ? "#B8B8B8" : "#999"
            font.pixelSize: 11
        }
        DmeComboBox {
            id: styleCombo
            width: parent.width - 24
            height: 23
            model: Backend.uiTheme.styles.map(function (s) {
                return s.name;
            })
            currentIndex: {
                for (var i = 0; i < Backend.uiTheme.styles.length; ++i)
                    if (Backend.uiTheme.styles[i].id === Backend.uiTheme.style)
                        return i;
                return 0;
            }
            onActivated: Backend.uiTheme.style = Backend.uiTheme.styles[currentIndex].id
        }

        Text {
            width: parent.width - 24
            text: Backend.uiTheme.style === "github-dark"
                  ? "Modern GitHub/Codex-inspired layout. Uses separate QML components while keeping the same editor actions."
                  : "Original Tibia-inspired layout and textured controls."
            color: Backend.uiTheme.style === "github-dark" ? "#8A8A8A" : "#8b949e"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        DmeSeparator {
            width: parent.width - 24
            visible: Backend.uiTheme.style === "classic"
        }

        Text {
            text: "Presets"
            color: "#999"
            font.pixelSize: 11
            visible: Backend.uiTheme.style === "classic"
        }

        Grid {
            columns: 4
            spacing: 4
            visible: Backend.uiTheme.style === "classic"
            Repeater {
                model: Backend.uiTheme.presets
                delegate: DmeButton {
                    required property var modelData
                    text: modelData.name
                    width: 68
                    onClicked: {
                        Backend.uiTheme.tint = modelData.color;
                        root.loadFromTheme();
                    }
                }
            }
        }

        DmeSeparator {
            width: parent.width - 24
            visible: Backend.uiTheme.style === "classic"
        }

        Text {
            text: "Custom color (RGB)"
            color: "#999"
            font.pixelSize: 11
            visible: Backend.uiTheme.style === "classic"
        }

        Row {
            spacing: 6
            visible: Backend.uiTheme.style === "classic"
            Text {
                text: "R"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: rField
                width: 62
                from: 0
                to: 255
            }
            Text {
                text: "G"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: gField
                width: 62
                from: 0
                to: 255
            }
            Text {
                text: "B"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: bField
                width: 62
                from: 0
                to: 255
            }
        }

        Row {
            spacing: 6
            visible: Backend.uiTheme.style === "classic"
            Text {
                text: "Preview"
                color: "#999"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                width: 40
                height: 18
                color: root.draft
                border {
                    width: 1
                    color: "#555"
                }
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.draft.toString()
                color: "#7f9f7f"
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "Apply"
                width: 90
                visible: Backend.uiTheme.style === "classic"
                onClicked: Backend.uiTheme.tint = root.draft
            }
            DmeButton {
                text: "Close"
                width: 90
                onClicked: root.close()
            }
        }
    }
}

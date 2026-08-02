import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"
import "../themes/classic/controls" as Classic

Item {
    id: toolBar
    required property var mapView
    required property var settings
    height: Backend.otbmReader.loaded ? 40 : 0
    visible: Backend.otbmReader.loaded

    DmePanel {
        anchors.fill: parent
    }

    component TbBtn: Item {
        id: btn
        property string label: ""
        property string tip: ""
        property color dot: "transparent"
        property string iconSource: ""
        property int iconSize: 26
        property bool active: false
        property color activeBg: "#2f6f4f"
        property color activeBorder: "#7fdc8f"
        signal clicked
        width: btnRow.implicitWidth + 24

        height: parent ? parent.height : 24
        opacity: enabled ? 1 : 0.45

        BorderImage {
            anchors.fill: parent
            source: Backend.uiTheme.tex + (btn.active ? "tab_checked.png" : (bma.containsMouse ? "tab_hover.png" : "tab_normal.png"))
            smooth: false
            border {
                left: 2
                right: 2
                top: 2
                bottom: 2
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: btn.active ? Qt.rgba(btn.activeBg.r, btn.activeBg.g, btn.activeBg.b, 0.45) : "transparent"
        }
        Row {
            id: btnRow
            anchors.centerIn: parent
            spacing: 5
            Rectangle {

                visible: btn.dot.a > 0
                width: 14
                height: 14
                radius: 2
                anchors.verticalCenter: parent.verticalCenter
                color: btn.dot
                border {
                    width: 1
                    color: "#20000000"
                }
            }
            Image {
                visible: btn.iconSource !== ""
                width: btn.iconSize
                height: btn.iconSize
                anchors.verticalCenter: parent.verticalCenter

                smooth: false
                cache: false
                fillMode: Image.PreserveAspectFit
                source: btn.iconSource
            }
            Text {
                visible: btn.label !== ""
                text: btn.label
                color: btn.active ? "#eaffea" : "#c0c0c0"
                font.pixelSize: 12
                font.bold: btn.active
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            id: bma
            anchors.fill: parent
            enabled: btn.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }
        ToolTip.visible: btn.tip !== "" && bma.containsMouse
        ToolTip.delay: 500
        ToolTip.text: btn.tip
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        spacing: -1

        Repeater {
            model: [
                {
                    label: "PZ",
                    flag: 1,
                    col: "#7fdc8f"
                },
                {
                    label: "No PvP",
                    flag: 4,
                    col: "#dc8fd0"
                },
                {
                    label: "No Logout",
                    flag: 8,
                    col: "#dcd07f"
                },
                {
                    label: "PvP",
                    flag: 16,
                    col: "#dca57f"
                }
            ]
            delegate: TbBtn {
                required property var modelData

                tip: modelData.label
                dot: modelData.col
                active: toolBar.mapView.activeZone === modelData.flag
                activeBg: "#3a5a4a"
                activeBorder: modelData.col
                onClicked: toolBar.mapView.activeZone = active ? 0 : modelData.flag
            }
        }

        TbBtn {
            label: "Erase"
            active: toolBar.mapView.eraseMode
            activeBg: "#5a3030"
            activeBorder: "#dc8f8f"
            onClicked: toolBar.mapView.eraseMode = !toolBar.mapView.eraseMode
        }

        Rectangle {
            width: 1
            height: 18
            color: "#555"
            anchors.verticalCenter: parent.verticalCenter
        }

        TbBtn {
            label: toolBar.mapView.selectionMode ? "Selection (Space)" : "Draw (Space)"
            active: true
            activeBg: toolBar.mapView.selectionMode ? "#2f3a4a" : "#22432f"
            activeBorder: toolBar.mapView.selectionMode ? "#6aa0dc" : "#7fdc8f"
            onClicked: toolBar.mapView.toggleSelectionMode()
        }
        TbBtn {
            label: "In-game preview"
            active: toolBar.settings.showIngamePreviewWindow
            onClicked: toolBar.settings.showIngamePreviewWindow =
                       !toolBar.settings.showIngamePreviewWindow
        }
        TbBtn {
            visible: toolBar.mapView.brushServerId > 0
            label: Backend.otbReader.nameForServerId(toolBar.mapView.brushServerId) + "  X"
            active: true
            onClicked: toolBar.mapView.brushServerId = 0
        }
        TbBtn {
            visible: toolBar.mapView.selectionCount > 0
            label: "Clear sel (" + toolBar.mapView.selectionCount + ")"
            onClicked: toolBar.mapView.clearSelection()
        }
        Text {
            visible: toolBar.mapView.pasting || toolBar.mapView.eraseMode
                     || toolBar.mapView.activeZone !== 0
            text: toolBar.mapView.pasting
                  ? "PASTE MODE - left click to confirm, Esc/right click to cancel"
                  : (toolBar.mapView.eraseMode ? (toolBar.mapView.activeZone !== 0 ? "ERASE: zone" : "ERASE: items") : "Ctrl+left click = erase")
            color: toolBar.mapView.pasting ? "#6aa0dc" : (toolBar.mapView.eraseMode ? "#dc8f8f" : "#888")
            font.pixelSize: 10
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        TbBtn {

            width: 38
            height: 38
            iconSize: 32
            anchors.verticalCenter: parent.verticalCenter
            active: toolBar.mapView.torchOn
            tip: "Lighting preview"

            iconSource: toolBar.mapView.torchOn ? "qrc:/ui/LightON.png" : "qrc:/ui/LightOFF.png"
            onClicked: toolBar.mapView.torchOn = !toolBar.mapView.torchOn
        }

        TbBtn {
            width: 38
            height: 38
            iconSize: 26
            anchors.verticalCenter: parent.verticalCenter
            active: toolBar.mapView.showAnimations
            tip: "Item animations"
            iconSource: "qrc:/ui/conditions.png"
            onClicked: toolBar.mapView.showAnimations = !toolBar.mapView.showAnimations
        }

        TbBtn {
            width: 38
            height: 38
            iconSize: 26
            anchors.verticalCenter: parent.verticalCenter
            active: toolBar.mapView.minimapOn
            tip: "Minimap"
            iconSource: "qrc:/ui/compass.png"
            onClicked: toolBar.mapView.minimapOn = !toolBar.mapView.minimapOn
        }

        Rectangle {
            width: 1
            height: 18
            color: "#555"
            anchors.verticalCenter: parent.verticalCenter
        }

        Classic.ClassicDarkButton {
            label: "Center"
            onClicked: toolBar.mapView.centerOnContent()
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: 18
            color: "#555"
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: "Floor"
            color: "#999"
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
        Classic.ClassicDarkButton {
            readOnly: true
            width: 26
            label: toolBar.mapView.floor
            anchors.verticalCenter: parent.verticalCenter
        }

        Classic.ClassicDarkButton {
            width: 26
            label: "-"
            onClicked: toolBar.mapView.floor = toolBar.mapView.floor + 1
            anchors.verticalCenter: parent.verticalCenter
        }
        Classic.ClassicDarkButton {
            width: 26
            label: "+"
            onClicked: toolBar.mapView.floor = toolBar.mapView.floor - 1
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}

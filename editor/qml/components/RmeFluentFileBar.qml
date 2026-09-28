import QtQuick
import QtQuick.Controls
import QtQuick.Controls.FluentWinUI3 as Fluent
import Tibia 1.0

Rectangle {
    id: root
    required property var mapView
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    color: "#2B2D2F"
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#414345" }
    implicitHeight: 36
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Repeater {
            model: [
                {name: "New map", op: "new"},
                {name: "Open map", op: "open"},
                {name: "Save", op: "save"},
                {name: "Save as", op: "saveAs"},
                {name: "Undo", op: "undo"},
                {name: "Redo", op: "redo"},
                {name: "Cut", op: "cut"},
                {name: "Copy", op: "copy"},
                {name: "Paste", op: "paste"}
            ]
            delegate: Fluent.ToolButton {
                id: toolButton
                required property var modelData
                width: 30; height: 30
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: "qrc:/qml/themes/rme/icons/" + toolButton.modelData.op + ".svg"
                        sourceSize: Qt.size(20, 20)
                        opacity: toolButton.enabled ? 1 : 0.35
                    }
                }
                background: Rectangle {
                    radius: 0
                    color: toolButton.down || toolButton.checked ? "#484B4E" : toolButton.hovered ? "#383B3E" : "transparent"
                    border.width: toolButton.checked || toolButton.hovered ? 1 : 0
                    border.color: "#777C80"
                }
                font.pixelSize: 16
                palette.buttonText: "#eeeeee"
                enabled: modelData.op === "new" || modelData.op === "open"
                    || (Backend.otbmReader.loaded && (
                        modelData.op === "undo" ? Backend.otbmReader.undoCount > 0
                        : modelData.op === "redo" ? Backend.otbmReader.redoCount > 0
                        : modelData.op === "paste" ? root.mapView.hasClipboard
                        : modelData.op === "cut" || modelData.op === "copy" ? root.mapView.selectionCount > 0 : true))
                Accessible.name: modelData.name
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: modelData.name
                onClicked: {
                    switch (modelData.op) {
                    case "new": root.newRequested(); break;
                    case "open": root.openRequested(); break;
                    case "save": root.saveRequested(); break;
                    case "saveAs": root.saveAsRequested(); break;
                    case "undo": root.mapView.undo(); break;
                    case "redo": root.mapView.redo(); break;
                    case "cut": root.mapView.cutSelection(); break;
                    case "copy": root.mapView.copySelection(); break;
                    case "paste": root.mapView.startPasting(); break;
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import "../style"

Row {
    id: root
    required property var mapCtrl
    property color textColor: "#C9D1D9"
    visible: mapCtrl.blueprintActive === true
    spacing: 8
    Text { text: "Blueprint"; color: root.textColor; anchors.verticalCenter: parent.verticalCenter }
    DmeComboBox {
        objectName: "blueprintPlacementMode"
        width: 110
        model: ["Merge", "Replace"]
        currentIndex: root.mapCtrl.blueprintPlacementMode === "replace" ? 1 : 0
        onActivated: {
            root.mapCtrl.blueprintPlacementMode = currentIndex === 1 ? "replace" : "merge";
            currentIndex = Qt.binding(() => root.mapCtrl.blueprintPlacementMode === "replace" ? 1 : 0);
        }
    }
    DmeButton { objectName: "blueprintRotate"; text: "Rotate"; onClicked: root.mapCtrl.rotateDoodadBrush() }
    DmeButton { objectName: "blueprintStop"; text: "Cancel"; onClicked: root.mapCtrl.cancelBlueprint() }
}

import QtQuick
import Tibia 1.0

DmeDialog {
    id: root
    property string message: ""
    width: 340
    contentItem: Column {
        spacing: 14
        Text {
            width: parent.width
            text: root.message
            color: Backend.uiTheme.style === "gray-dark" ? "#E0E0E0" : (Backend.uiTheme.style === "github-dark" ? "#C9D1D9" : "#c0c0c0")
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            DmeButton { text: "Yes"; width: 90; variant: "primary"; onClicked: { root.accepted(); root.close(); } }
            DmeButton { text: "Cancel"; width: 90; onClicked: root.close() }
        }
    }
}

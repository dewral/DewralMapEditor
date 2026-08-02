import QtQuick
import QtQuick.Controls
import Tibia 1.0

MenuItem {
    id: control
    implicitHeight: 24
    padding: 0
    spacing: 0
    contentItem: Item {
        implicitWidth: label.implicitWidth + (control.checkable ? 30 : 10) + (control.subMenu !== null ? 22 : 10)
        implicitHeight: 24
        Text {
            id: label
            anchors.left: parent.left
            anchors.leftMargin: control.checkable ? 30 : 10
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: Backend.uiTheme.style === "github-dark"
                   ? (!control.enabled ? "#6E7681" : (control.highlighted ? "#FFFFFF" : "#C9D1D9"))
                   : (!control.enabled ? "#777" : (control.highlighted ? "#eaffea" : "#dcdcdc"))
            font.pixelSize: 12
        }
    }
    indicator: Text {
        visible: control.checkable
        width: 22; anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
        text: control.checked ? "\u2713" : ""; horizontalAlignment: Text.AlignHCenter
        color: Backend.uiTheme.style === "github-dark" ? (control.highlighted ? "#FFFFFF" : "#3FB950") : "#80c080"
        font.pixelSize: 13; font.bold: true
    }
    arrow: Text {
        visible: control.subMenu !== null; text: ">"; font.pixelSize: 10
        color: Backend.uiTheme.style === "github-dark" ? (control.highlighted ? "#FFFFFF" : "#8B949E") : "#999"
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
    }
    background: Rectangle {
        radius: Backend.uiTheme.style === "github-dark" ? 4 : 0
        color: control.highlighted ? (Backend.uiTheme.style === "github-dark" ? "#1B2632" : "#807a7d82") : "transparent"
        border.width: control.highlighted ? 1 : 0
        border.color: Backend.uiTheme.style === "github-dark" ? "#3A4655" : "#9a9a9a"
    }
}

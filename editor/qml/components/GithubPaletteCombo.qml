import QtQuick
import QtQuick.Controls
import Tibia 1.0

ComboBox {
    id: root
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"

    height: 40
    leftPadding: 12
    rightPadding: 34
    font.pixelSize: 13

    contentItem: Text {
        leftPadding: root.leftPadding
        rightPadding: root.rightPadding
        text: root.displayText
        color: root.enabled ? (root.grayTheme ? "#E8E8E8" : "#E6EDF3") : (root.grayTheme ? "#777777" : "#768390")
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: root.width - width - 13
        anchors.verticalCenter: parent.verticalCenter
        text: "\u2304"
        color: root.enabled ? (root.grayTheme ? "#D8D8D8" : "#C9D1D9") : (root.grayTheme ? "#777777" : "#768390")
        font.pixelSize: 18
    }

    background: Rectangle {
        radius: 4
        color: root.down ? (root.grayTheme ? "#303030" : "#171E27") : (root.grayTheme ? "#242424" : "#0D1117")
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? (root.grayTheme ? "#C79A3B" : "#3A7D55") : (root.grayTheme ? "#484848" : "#242D38")
    }

    delegate: ItemDelegate {
        id: comboDelegate
        required property var modelData
        required property int index
        width: root.width
        height: 34
        leftPadding: 12
        text: modelData
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: comboDelegate.text
            color: root.grayTheme ? "#E8E8E8" : "#E6EDF3"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: comboDelegate.highlighted ? (root.grayTheme ? "#303030" : "#1B2632") : (root.grayTheme ? "#202020" : "#10151C")
        }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: 4
            color: root.grayTheme ? "#202020" : "#10151C"
            border.width: 1
            border.color: root.grayTheme ? "#484848" : "#2D3743"
        }
    }
}

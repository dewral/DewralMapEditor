import QtQuick
import QtQuick.Controls

ComboBox {
    id: root

    height: 40
    leftPadding: 12
    rightPadding: 34
    font.pixelSize: 13

    contentItem: Text {
        leftPadding: root.leftPadding
        rightPadding: root.rightPadding
        text: root.displayText
        color: root.enabled ? "#E6EDF3" : "#768390"
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: root.width - width - 13
        anchors.verticalCenter: parent.verticalCenter
        text: "\u2304"
        color: root.enabled ? "#C9D1D9" : "#768390"
        font.pixelSize: 18
    }

    background: Rectangle {
        radius: 4
        color: root.down ? "#171E27" : "#0D1117"
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? "#3A7D55" : "#242D38"
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
            color: "#E6EDF3"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: comboDelegate.highlighted ? "#1B2632" : "#10151C"
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
            color: "#10151C"
            border.width: 1
            border.color: "#2D3743"
        }
    }
}

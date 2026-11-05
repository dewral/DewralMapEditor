import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

MenuItem {
    id: control
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    hoverEnabled: true
    readonly property string shortcutText: control.action && control.action.shortcut
        ? String(control.action.shortcut) : ""
    implicitHeight: control.tibiaTheme ? 20 : 24
    padding: 0
    spacing: 0
    contentItem: Item {
        implicitWidth: control.tibiaTheme
            ? 26 + label.implicitWidth + (shortcutLabel.visible ? shortcutLabel.implicitWidth + 24 : 0) + (control.subMenu !== null ? 22 : 12)
            : label.implicitWidth + (control.checkable ? 30 : 10) + (control.subMenu !== null ? 22 : 10)
        implicitHeight: control.tibiaTheme ? 20 : 24
        Text {
            id: label
            objectName: "menuEntryLabel"
            anchors.left: parent.left
            anchors.leftMargin: control.tibiaTheme ? 26 : control.checkable ? 22 : 10
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.tibiaTheme ? (control.enabled ? (control.highlighted || control.hovered ? "#ffffff" : "#dfdfdf") : "#777777") : ((!control.enabled ? FluentColors.c("disabled") : FluentColors.c("text")))
            font.family: (FluentColors.family())
            font.bold: !control.tibiaTheme && FluentColors.bold()
            font.pixelSize: control.tibiaTheme ? 11 : (FluentColors.size(12))
            style: Text.Normal
            renderType: control.tibiaTheme ? Text.NativeRendering : Text.QtRendering
            font.hintingPreference: Font.PreferFullHinting
        }
        Text {
            id: shortcutLabel
            objectName: "menuEntryShortcut"
            visible: control.tibiaTheme && text.length > 0
            text: control.shortcutText
            anchors.right: parent.right; anchors.rightMargin: control.subMenu !== null ? 22 : 10
            anchors.verticalCenter: parent.verticalCenter
            color: control.enabled ? "#b8b8b8" : "#777777"
            font.family: "Verdana"; font.pixelSize: 11
            renderType: Text.NativeRendering
        }
    }
    indicator: Item {
        visible: control.checkable
        width: 22; height: 12
        anchors.left: parent.left; anchors.leftMargin: control.tibiaTheme ? 4 : 6; anchors.verticalCenter: parent.verticalCenter
        Image {
            objectName: "classicMenuCheckBox"
            visible: control.tibiaTheme
            anchors.centerIn: parent; width: 12; height: 12; smooth: false
            source: Qt.resolvedUrl("../themes/tibia/images/" + (control.checked ? (control.enabled ? "checkbox-checked.png" : "checkbox-checked-disabled.png") : "checkbox-unchecked.png"))
        }
        Text {
            visible: !control.tibiaTheme
            anchors.centerIn: parent
            text: control.checked ? "\u2713" : ""
            color: (FluentColors.c("accent"))
            font.pixelSize: 13; font.bold: true
        }
    }
    arrow: Text {
        visible: control.subMenu !== null; text: ">"; font.pixelSize: 10
        color: (FluentColors.c("muted"))
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
    }
    background: Rectangle {
        objectName: "menuEntryBackground"
        radius: Backend.uiTheme.style === "tibia-ui" ? 0 : 4
        color: control.tibiaTheme ? ((control.highlighted || control.hovered) && control.enabled ? "#18ffffff" : "transparent") : ((control.highlighted ? FluentColors.c("hover") : "transparent"))
        border.width: !control.tibiaTheme && control.highlighted ? 1 : 0
        border.color: (FluentColors.c("border"))
    }
}

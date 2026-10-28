import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

MenuItem {
    id: control
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    readonly property bool fluentTheme: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic" && !windowsClassic
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
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
            color: control.tibiaTheme ? (control.enabled ? (control.highlighted || control.hovered ? "#ffffff" : "#dfdfdf") : "#777777") : control.fluentTheme ? (!control.enabled ? FluentColors.c("disabled") : FluentColors.c("text")) : control.windowsClassic
                   ? (!control.enabled ? "#808080" : (control.highlighted ? "#ffffff" : "#202020"))
                   : control.modernTheme
                   ? (!control.enabled ? (control.grayTheme ? "#777777" : "#6E7681") : (control.highlighted ? "#FFFFFF" : (control.grayTheme ? "#E0E0E0" : "#C9D1D9")))
                   : (!control.enabled ? "#777" : (control.highlighted ? "#eaffea" : "#dcdcdc"))
            font.family: control.fluentTheme ? FluentColors.family() : Qt.application.font.family
            font.bold: !control.tibiaTheme && control.fluentTheme && FluentColors.bold()
            font.pixelSize: control.tibiaTheme ? 11 : control.fluentTheme ? FluentColors.size(12) : 12
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
            color: control.fluentTheme ? FluentColors.c("accent") : control.windowsClassic ? (control.highlighted ? "#ffffff" : "#202020") : control.modernTheme ? (control.highlighted ? "#FFFFFF" : control.grayTheme ? "#C79A3B" : "#3FB950") : "#80c080"
            font.pixelSize: 13; font.bold: true
        }
    }
    arrow: Text {
        visible: control.subMenu !== null; text: ">"; font.pixelSize: 10
        color: control.fluentTheme ? FluentColors.c("muted") : control.windowsClassic ? (control.highlighted ? "#ffffff" : "#202020") : (control.modernTheme ? (control.highlighted ? "#FFFFFF" : (control.grayTheme ? "#929292" : "#8B949E")) : "#999")
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
    }
    background: Rectangle {
        objectName: "menuEntryBackground"
        radius: Backend.uiTheme.style === "tibia-ui" ? 0 : control.fluentTheme ? 4 : control.modernTheme ? 4 : 0
        color: control.tibiaTheme ? ((control.highlighted || control.hovered) && control.enabled ? "#606060" : "transparent") : control.fluentTheme ? (control.highlighted ? FluentColors.c("hover") : "transparent") : control.highlighted ? (control.windowsClassic ? "#0a64ad" : (control.modernTheme ? (control.grayTheme ? "#303030" : "#1B2632") : "#807a7d82")) : "transparent"
        border.width: !control.tibiaTheme && control.highlighted ? 1 : 0
        border.color: control.fluentTheme ? FluentColors.c("border") : control.windowsClassic ? "#0a64ad" : (control.modernTheme ? (control.grayTheme ? "#555555" : "#3A4655") : "#9a9a9a")
    }
}

import QtQuick
import QtQuick.Window
import Tibia 1.0

// Lightweight GitHub-style tooltip. It is an Item instead of a Controls
// Popup so it also works in the editor's frameless Window (which is not an
// ApplicationWindow and therefore has no guaranteed Controls overlay).
Item {
    id: root
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"

    property Item targetItem
    property bool targetHovered: false
    property string message: ""

    readonly property Item hostItem: {
        if (!targetItem)
            return null;
        var targetWindow = targetItem.Window.window;
        if (targetWindow && targetWindow.contentItem)
            return targetWindow.contentItem;
        return targetItem.parent;
    }

    visible: targetHovered && message.length > 0 && hostItem !== null
    parent: hostItem
    z: 1000

    width: Math.max(40, tooltipText.implicitWidth + 20)
    height: Math.max(24, tooltipText.implicitHeight + 12)

    readonly property bool hasCursorPosition: targetItem
                                               && typeof targetItem.mouseX === "number"
                                               && typeof targetItem.mouseY === "number"
    readonly property point cursorPosition: {
        if (!targetItem || !root.parent)
            return Qt.point(0, 0);
        if (hasCursorPosition)
            return targetItem.mapToItem(root.parent, targetItem.mouseX, targetItem.mouseY);
        return targetItem.mapToItem(root.parent, targetItem.width / 2, targetItem.height / 2);
    }

    // Follow the pointer instead of anchoring the tooltip to the center of
    // the control. Clamp the result so it never leaves the application window.
    x: {
        if (!targetItem || !root.parent)
            return 0;
        var desired = cursorPosition.x + 14;
        return Math.round(Math.max(8, Math.min(root.parent.width - width - 8, desired)));
    }
    y: {
        if (!targetItem || !root.parent)
            return 0;
        var below = cursorPosition.y + 18;
        if (below + height <= root.parent.height - 8)
            return Math.round(below);
        return Math.round(Math.max(8, cursorPosition.y - height - 12));
    }

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: root.grayTheme ? "#242424" : "#161B22"
        border.width: 1
        border.color: root.grayTheme ? "#484848" : "#30363D"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            radius: 1
            color: root.grayTheme ? "#C79A3B" : "#2EA043"
        }
    }

    Text {
        id: tooltipText
        anchors.fill: parent
        leftPadding: 10
        rightPadding: 10
        topPadding: 6
        bottomPadding: 6
        text: root.message
        color: root.grayTheme ? "#F0F0F0" : "#E6EDF3"
        font.pixelSize: 12
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}

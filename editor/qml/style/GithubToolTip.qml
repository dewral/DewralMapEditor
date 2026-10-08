import QtQuick
import QtQuick.Window
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

// Lightweight GitHub-style tooltip. It is an Item instead of a Controls
// Popup so it also works in the editor's frameless Window (which is not an
// ApplicationWindow and therefore has no guaranteed Controls overlay).
Item {
    id: root
    readonly property bool fluentTheme: Backend.uiTheme.style === "fluent-dark"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"

    property Item targetItem
    property bool targetHovered: false
    property string message: ""
    property int delay: 350
    property bool ready: false
    function schedule() {
        ready = false;
        reveal.stop();
        if (targetHovered && message.length > 0) reveal.start();
    }
    onTargetHoveredChanged: schedule()
    onMessageChanged: schedule()
    onTargetItemChanged: schedule()
    Timer { id: reveal; interval: root.delay; onTriggered: root.ready = true }

    readonly property Item hostItem: {
        if (!targetItem)
            return null;
        var targetWindow = targetItem.Window.window;
        if (targetWindow && targetWindow.contentItem)
            return targetWindow.contentItem;
        return targetItem.parent;
    }

    visible: ready && targetHovered && message.length > 0 && hostItem !== null
    parent: hostItem
    z: 1000

    width: Math.min(hostItem ? Math.max(40, hostItem.width - 16) : 360, 360, Math.max(40, tooltipText.implicitWidth + 30))
    height: Math.max(24, tooltipText.contentHeight + 12)

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
        radius: 4
        color: "#e6191d1f"
        border.width: 1
        border.color: FluentColors.c("accent")
    }

    Rectangle {
        x: 9
        anchors.verticalCenter: parent.verticalCenter
        width: 6; height: 6; radius: 3
        color: FluentColors.c("accent")
    }

    Text {
        id: tooltipText
        anchors.fill: parent
        leftPadding: 21
        rightPadding: 10
        topPadding: 6
        bottomPadding: 6
        text: root.message
        color: root.fluentTheme ? FluentColors.c("text") : root.grayTheme ? "#F0F0F0" : "#E6EDF3"
        font.pixelSize: 12
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.family: FluentColors.fontFamily
        verticalAlignment: Text.AlignVCenter
    }
}

import "../themes/fluent/Colors.js" as Colors
import "../components"
import QtQuick
import Tibia 1.0

Item {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    property var scrollController: null
    property var flickable
    property bool dragging: false

    visible: flickable && flickable.contentHeight > flickable.height
    clip: true
    width: 12

    readonly property int pixelStep: tibiaTheme ? 20 : 32
    function scrollBy(delta) {
        if (scrollController) scrollController.scrollBy(delta, false);
        else if (flickable) flickable.contentY=Math.max(minY,Math.min(maxY,flickable.contentY+delta));
    }
    readonly property real minY: flickable ? flickable.originY : 0
    readonly property real maxY: flickable ? flickable.originY + flickable.contentHeight - flickable.height : 0
    readonly property real scrollPos: flickable && flickable.contentHeight > flickable.height
                                      ? Math.max(0, Math.min(1, (flickable.contentY - minY)
                                                               / (flickable.contentHeight - flickable.height)))
                                      : 0

    Item {
        id: track
        anchors.top: upArrow.bottom
        anchors.bottom: downArrow.top
        anchors.left: parent.left
        anchors.right: parent.right
        Loader { anchors.fill: parent; sourceComponent: !root.tibiaTheme ? fluentTrack : tibiaTrack }
        MouseArea {
            anchors.fill: parent
            onClicked: mouse => root.scrollBy((mouse.y+track.y<thumb.y ? -1 : 1)*(root.flickable ? root.flickable.height : 100))
        }
    }

    Item {
        id: upArrow
        anchors.top: parent.top
        width: root.width; height: root.width
        Loader { anchors.fill: parent; sourceComponent: !root.tibiaTheme ? fluentUp : tibiaUp }
        MouseArea {
            id: upArea; anchors.fill: parent; hoverEnabled: true
            onPressed: { root.scrollBy(-root.pixelStep); upRepeat.interval=350; upRepeat.start(); }
            onReleased: upRepeat.stop()
            onCanceled: upRepeat.stop()
            Timer { id: upRepeat; interval: 350; repeat: true; onTriggered: { if(upArea.containsMouse) root.scrollBy(-root.pixelStep); interval=60; } }
        }
    }

    Item {
        id: downArrow
        anchors.bottom: parent.bottom
        width: root.width; height: root.width
        Loader { anchors.fill: parent; sourceComponent: !root.tibiaTheme ? fluentDown : tibiaDown }
        MouseArea {
            id: downArea; anchors.fill: parent; hoverEnabled: true
            onPressed: { root.scrollBy(root.pixelStep); downRepeat.interval=350; downRepeat.start(); }
            onReleased: downRepeat.stop()
            onCanceled: downRepeat.stop()
            Timer { id: downRepeat; interval: 350; repeat: true; onTriggered: { if(downArea.containsMouse) root.scrollBy(root.pixelStep); interval=60; } }
        }
    }

    Item {
        id: thumb
        width: root.width
        height: root.flickable && root.flickable.contentHeight > 0
                ? Math.min(track.height, Math.max(root.tibiaTheme ? 14 : 24, track.height * (root.flickable.height / root.flickable.contentHeight)))
                : 20
        Loader { anchors.fill: parent; sourceComponent: !root.tibiaTheme ? fluentThumb : tibiaThumb }
        Binding {
            target: thumb; property: "y"; when: !root.dragging
            value: upArrow.height + root.scrollPos * (track.height - thumb.height)
        }
        MouseArea {
            id: thumbArea
            anchors.fill: parent
            hoverEnabled: true
            drag.target: thumb
            drag.axis: Drag.YAxis
            drag.minimumY: upArrow.height
            drag.maximumY: upArrow.height + track.height - thumb.height
            onPressed: { if (root.scrollController) root.scrollController.cancel(); root.dragging = true; if (root.flickable) root.flickable.cancelFlick(); }
            onReleased: { if (root.scrollController) root.scrollController.finish(); root.dragging = false; }
            onCanceled: { if (root.scrollController) root.scrollController.cancel(); root.dragging = false; }
            onPositionChanged: {
                if (!root.dragging || !pressed || !root.flickable || track.height === thumb.height)
                    return;
                var ratio = (thumb.y - upArrow.height) / (track.height - thumb.height);
                const target = root.minY + ratio * (root.flickable.contentHeight - root.flickable.height);
                if (root.scrollController)
                    root.scrollController.scrollTo(target, false, 75);
                else
                    root.flickable.contentY = target;
            }
        }
    }

    Component {
        id: tibiaTrack
        Item {
            Image { anchors.fill: parent; visible: root.tibiaTheme; source: Qt.resolvedUrl("../themes/tibia/images/scrollbar-texture-vertical.png"); fillMode: Image.Tile; smooth: false }
        }
    }
    Component {
        id: fluentTrack
        Item { Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; radius: 4; color: (Colors.c("scrollTrack")) } }
    }
    Component {
        id: tibiaUp
        Image { smooth: false; source: Qt.resolvedUrl("../themes/tibia/images/scrollbar-buttonup-"+(upArea.pressed ? "down" : "up")+".png") }
    }
    Component {
        id: fluentUp
        Text { text: "\u2303"; color: upArea.containsMouse ? ("#C9D1D9") : ("#6E7681"); font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    Component {
        id: tibiaDown
        Image { smooth: false; source: Qt.resolvedUrl("../themes/tibia/images/scrollbar-buttondown-"+(downArea.pressed ? "down" : "up")+".png") }
    }
    Component {
        id: fluentDown
        Text { text: "\u2304"; color: downArea.containsMouse ? ("#C9D1D9") : ("#6E7681"); font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    Component {
        id: tibiaThumb
        BorderImage { objectName: "clientScrollbarHandle"; source: Qt.resolvedUrl("../themes/tibia/images/scrollbar-handle-vertical.png"); smooth: false; border { left: 0; right: 0; top: 6; bottom: 6 } }
    }
    Component {
        id: fluentThumb
        Item { Rectangle { anchors.fill: parent; anchors.leftMargin: 3; anchors.rightMargin: 3; radius: 0; color: thumbArea.containsMouse || root.dragging ? ("#8B949E") : ((Colors.c("scrollThumb"))) } }
    }
    ColorHighlight { targetItem: root; colorKeys: ["scrollTrack", "scrollThumb"] }
}

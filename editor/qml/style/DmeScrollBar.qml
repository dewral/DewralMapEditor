import QtQuick
import Tibia 1.0

Item {
    id: root
    property var flickable
    property bool dragging: false
    readonly property bool githubTheme: Backend.uiTheme.style !== "classic"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"

    visible: flickable && flickable.contentHeight > flickable.height
    clip: true
    width: 12

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
        Loader { anchors.fill: parent; sourceComponent: root.githubTheme ? githubTrack : classicTrack }
    }

    Item {
        id: upArrow
        anchors.top: parent.top
        width: 12; height: 12
        Loader { anchors.fill: parent; sourceComponent: root.githubTheme ? githubUp : classicUp }
        MouseArea {
            id: upArea; anchors.fill: parent; hoverEnabled: true
            onClicked: if (root.flickable) root.flickable.contentY = Math.max(root.minY, root.flickable.contentY - 32)
        }
    }

    Item {
        id: downArrow
        anchors.bottom: parent.bottom
        width: 12; height: 12
        Loader { anchors.fill: parent; sourceComponent: root.githubTheme ? githubDown : classicDown }
        MouseArea {
            id: downArea; anchors.fill: parent; hoverEnabled: true
            onClicked: if (root.flickable) root.flickable.contentY = Math.min(root.maxY, root.flickable.contentY + 32)
        }
    }

    Item {
        id: thumb
        width: 12
        height: root.flickable && root.flickable.contentHeight > 0
                ? Math.min(track.height, Math.max(24, track.height * (root.flickable.height / root.flickable.contentHeight)))
                : 20
        Loader { anchors.fill: parent; sourceComponent: root.githubTheme ? githubThumb : classicThumb }
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
            onPressed: root.dragging = true
            onReleased: root.dragging = false
            onPositionChanged: {
                if (!root.flickable || track.height === thumb.height)
                    return;
                var ratio = (thumb.y - upArrow.height) / (track.height - thumb.height);
                root.flickable.contentY = root.minY + ratio * (root.flickable.contentHeight - root.flickable.height);
            }
        }
    }

    Component {
        id: classicTrack
        BorderImage { source: Backend.uiTheme.tex + "scrollbar_track.png"; smooth: false; border { left: 1; right: 1; top: 1; bottom: 1 } }
    }
    Component {
        id: githubTrack
        Item { Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; radius: 2; color: root.grayTheme ? "#242424" : "#161B22" } }
    }
    Component {
        id: classicUp
        Image { smooth: false; source: upArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_up_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_up.png") }
    }
    Component {
        id: githubUp
        Text { text: "\u2303"; color: upArea.containsMouse ? (root.grayTheme ? "#E0E0E0" : "#C9D1D9") : (root.grayTheme ? "#777777" : "#6E7681"); font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    Component {
        id: classicDown
        Image { smooth: false; source: downArea.pressed ? (Backend.uiTheme.tex + "scrollbar_arrow_down_hover.png") : (Backend.uiTheme.tex + "scrollbar_arrow_down.png") }
    }
    Component {
        id: githubDown
        Text { text: "\u2304"; color: downArea.containsMouse ? (root.grayTheme ? "#E0E0E0" : "#C9D1D9") : (root.grayTheme ? "#777777" : "#6E7681"); font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    Component {
        id: classicThumb
        BorderImage { source: Backend.uiTheme.tex + "scrollbar_thumb.png"; smooth: false; border { left: 6; right: 6; top: 6; bottom: 6 } }
    }
    Component {
        id: githubThumb
        Item { Rectangle { anchors.fill: parent; anchors.leftMargin: 3; anchors.rightMargin: 3; radius: 3; color: thumbArea.containsMouse || root.dragging ? (root.grayTheme ? "#A0A0A0" : "#8B949E") : (root.grayTheme ? "#666666" : "#57606A") } }
    }
}

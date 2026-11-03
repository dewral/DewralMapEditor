// Adapted from the user-provided Qt client NativeScrollBar.qml.
import QtQuick
import Tibia 1.0

Item {
    id: bar
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    implicitWidth: 12
    property var flickable
    property int stepSize: 14
    property int repeatDirection: 0
    property real pagePoint: -1
    readonly property real range: flickable ? Math.max(0, flickable.contentHeight - flickable.height) : 0
    readonly property real position: range > 0 ? Math.max(0, Math.min(1, flickable.contentY / range)) : 0
    onRangeChanged: scrollBy(0)
    function scrollBy(amount) {
        if (flickable) flickable.contentY = Math.round(Math.max(0, Math.min(range, flickable.contentY + amount)))
    }
    function pageAt(point) {
        if (!flickable || range<=0) return
        if (point < thumb.y) scrollBy(-flickable.height)
        else if (point >= thumb.y+thumb.height) scrollBy(flickable.height)
    }
    function beginRepeat(direction,point) {
        repeatDirection=direction;pagePoint=point
        if (direction) scrollBy(direction*stepSize);else pageAt(point)
        repeatTimer.interval=400;repeatTimer.restart()
    }
    function endRepeat() {repeatTimer.stop();repeatDirection=0;pagePoint=-1}
    Timer {
        id: repeatTimer;repeat: true;interval: 400
        onTriggered: {if(bar.repeatDirection)bar.scrollBy(bar.repeatDirection*bar.stepSize);else bar.pageAt(bar.pagePoint);interval=70}
    }
    onVisibleChanged: if (!visible) endRepeat()
    Image {
        id: track
        x: 0; y: 12; width: bar.width; height: Math.max(0, bar.height - 24)
        source: Qt.resolvedUrl(bar.tibiaUi ? "images/scrollbar-texture-vertical.png" : "classic/scrollbar.png"); sourceClipRect: bar.tibiaUi ? Qt.rect(0,0,0,0) : Qt.rect(24, 0, 12, 74); fillMode: Image.Tile; smooth: false
        MouseArea {
            anchors.fill: parent
            enabled: bar.range>0
            onPressed: function(mouse) {bar.beginRepeat(0,mouse.y+track.y)}
            onReleased: bar.endRepeat()
            onCanceled: bar.endRepeat()
        }
    }
    Image {
        width: bar.width; height: 12; source: Qt.resolvedUrl(bar.tibiaUi ? "images/scrollbar-buttonup-"+(up.pressed ? "down" : "up")+".png" : "classic/scrollbar.png"); sourceClipRect: bar.tibiaUi ? Qt.rect(0,0,0,0) : Qt.rect(up.pressed ? 12 : 0, 0, 12, 12); smooth: false
        MouseArea { id: up; anchors.fill: parent; enabled: bar.range>0; onPressed: bar.beginRepeat(-1,-1); onReleased: bar.endRepeat();onCanceled: bar.endRepeat() }
    }
    Image {
        y: bar.height - 12; width: bar.width; height: 12
        source: Qt.resolvedUrl(bar.tibiaUi ? "images/scrollbar-buttondown-"+(down.pressed ? "down" : "up")+".png" : "classic/scrollbar.png"); sourceClipRect: bar.tibiaUi ? Qt.rect(0,0,0,0) : Qt.rect(down.pressed ? 12 : 0, 12, 12, 12); smooth: false
        MouseArea { id: down; anchors.fill: parent; enabled: bar.range>0; onPressed: bar.beginRepeat(1,-1); onReleased: bar.endRepeat();onCanceled: bar.endRepeat() }
    }
    Item {
        id: thumb
        width: bar.width
        height: Math.floor(Math.min(track.height, Math.max(12, bar.flickable ? track.height * bar.flickable.height / Math.max(1, bar.flickable.contentHeight) : 12)))
        y: Math.floor(track.y + bar.position * (track.height - height))
        visible: bar.range > 0
        TibiaPreviewFrame { anchors.fill: parent; kind: "scrollbar"; visible: !bar.tibiaUi }
        BorderImage {
            anchors.fill: parent; visible: bar.tibiaUi
            source: Qt.resolvedUrl("images/scrollbar-handle-vertical.png")
            border { left: 0; right: 0; top: 6; bottom: 6 }
            smooth: false
        }
        MouseArea {
            anchors.fill: parent
            property real initialY
            property real initialContentY
            onPressed: function(mouse) {
                initialY = mapToItem(bar, mouse.x, mouse.y).y
                initialContentY = bar.flickable.contentY
            }
            onPositionChanged: function(mouse) {
                if (!pressed || track.height <= thumb.height) return
                const delta = mapToItem(bar, mouse.x, mouse.y).y - initialY
                bar.flickable.contentY = Math.round(Math.max(0, Math.min(bar.range, initialContentY + delta * bar.range / (track.height - thumb.height))))
            }
        }
    }
}

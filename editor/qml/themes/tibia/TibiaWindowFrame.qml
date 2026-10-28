import QtQuick
import QtQuick.Controls as Controls

// Native client popup frame: preserve the 17px caption and 4px edge slices.
BorderImage {
    id: frame
    objectName: "tibiaDialogFrame"
    property string caption: ""
    property bool movable: true
    property bool locked: false
    property bool showLockButton: false
    property bool showCloseButton: true
    property string captionIconSource: ""
    signal captionActivated()
    property color captionColor: "#c0c0c0"
    readonly property int captionHeight: 17
    readonly property int contentMargin: 8
    readonly property int contentTopMargin: 25
    signal closeRequested()
    signal lockRequested()
    signal dragStarted(real sceneX, real sceneY)
    signal dragMoved(real sceneX, real sceneY)
    signal dragFinished()
    signal dragCanceled()
    source: Qt.resolvedUrl("images/dialog-frame-borderimage.png")
    border { left: 4; top: 17; right: 4; bottom: 4 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
    smooth: false

    Image { x: 7; y: 2; width: 12; height: 12; source: frame.captionIconSource; visible: source.toString().length>0; smooth: false }
    // Match TibiaDialog's personalMouseShield: blank window areas also consume
    // clicks/wheel, so editing the map underneath a floating palette is impossible.
    MouseArea {
        objectName: "tibiaWindowMouseShield"
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted=true
    }
    Text {
        objectName: "tibiaWindowCaption"
        x: frame.captionIconSource ? 23 : 8; y: 0; width: parent.width-x-(frame.showLockButton ? 38 : frame.showCloseButton ? 22 : 8); height: frame.captionHeight
        text: frame.caption
        font.family: "Verdana"; font.pixelSize: 11; font.bold: true
        color: frame.captionColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    MouseArea {
        objectName: "tibiaWindowDragHandle"
        x: 4; y: 0; width: Math.max(0,parent.width-8-(frame.showCloseButton ? 14 : 0)-(frame.showLockButton ? 14 : 0)); height: frame.captionHeight
        property bool dragged: false
        property point pressPoint
        acceptedButtons: Qt.LeftButton
        cursorShape: frame.movable && !frame.locked ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
        onPressed: mouse => {
            dragged=false; pressPoint=Qt.point(mouse.x,mouse.y);
            if(!frame.movable || frame.locked) return;
            const p=mapToItem(null,mouse.x,mouse.y); frame.dragStarted(p.x,p.y);
        }
        onPositionChanged: mouse => {
            if(!pressed || !frame.movable || frame.locked) return;
            if (Math.abs(mouse.x-pressPoint.x)+Math.abs(mouse.y-pressPoint.y)>6) dragged=true;
            const p=mapToItem(null,mouse.x,mouse.y); frame.dragMoved(p.x,p.y);
        }
        onReleased: { frame.dragFinished(); if(!dragged) frame.captionActivated(); }
        onCanceled: frame.dragCanceled()
        onDoubleClicked: if(frame.showLockButton) frame.lockRequested()
    }
    Item {
        visible: frame.showLockButton
        width: 14; height: 14; x: frame.width-32; y: 1
        Image {
            anchors.centerIn: parent; width: 12; height: 12; smooth: false
            source: Qt.resolvedUrl("images/"+(frame.locked ? "max" : "min")+"-button-small-"+(lockMouse.pressed ? "down" : "up")+".png")
        }
        MouseArea { id: lockMouse; anchors.fill: parent; hoverEnabled: true; onClicked: frame.lockRequested() }
        Controls.ToolTip.visible: lockMouse.containsMouse
        Controls.ToolTip.text: frame.locked ? "Unlock palette position" : "Lock palette position"
    }
    Image {
        objectName: "tibiaWindowCloseButton"
        visible: frame.showCloseButton
        x: frame.width-17; y: 2; width: 12; height: 12; smooth: false
        source: Qt.resolvedUrl("images/"+(closeMouse.pressed ? "exit-button-small-down.png" : "exit-button-small-up.png"))
        MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; onClicked: frame.closeRequested() }
        Controls.ToolTip.visible: closeMouse.containsMouse
        Controls.ToolTip.text: "Close palette (Ctrl+B)"
    }
}

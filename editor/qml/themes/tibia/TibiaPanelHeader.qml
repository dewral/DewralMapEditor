import QtQuick
import QtQuick.Controls as Controls

// Shared native caption for embedded panels, floating panels and dialogs.
Item {
    id: frame
    objectName: "tibiaPanelHeader"
    property string caption: ""
    property bool centerCaption: false
    property bool movable: true
    property bool locked: false
    property bool showLockButton: false
    property bool showCloseButton: true
    property string captionIconSource: ""
    property int captionIconSize: 12
    property int captionFontPixelSize: 11
    signal captionActivated()
    property color captionColor: "#c0c0c0"
    property int captionHeight: 17
    readonly property int contentMargin: 8
    readonly property int contentTopMargin: captionHeight + contentMargin
    signal closeRequested()
    signal lockRequested()
    signal dragStarted(real sceneX, real sceneY)
    signal dragMoved(real sceneX, real sceneY)
    signal dragFinished()
    signal dragCanceled()
    implicitHeight: captionHeight
    property bool showCollapseButton: false
    property bool collapsed: false
    property bool windowActive: true
    property string closeToolTip: "Close"
    property string lockToolTip: locked ? "Unlock panel position" : "Lock panel position"
    // Reserve space for native window buttons supplied by the owner.
    property int reservedButtons: 0
    property int reservedButtonWidth: 18
    readonly property int buttonsWidth: reservedButtons*reservedButtonWidth + (Number(showCloseButton)+Number(showLockButton)+Number(showCollapseButton))*18
    readonly property int buttonCount: reservedButtons + Number(showCloseButton) + Number(showLockButton) + Number(showCollapseButton)
    signal collapseRequested()

    Image {
        objectName: "tibiaCaptionIcon"
        x: 7; y: Math.floor((frame.captionHeight-height)/2)
        width: frame.captionIconSize; height: width
        source: frame.captionIconSource; visible: source.toString().length>0; smooth: false
    }
    Text {
        objectName: "tibiaWindowCaption"
        x: frame.centerCaption ? 8+frame.buttonsWidth : frame.captionIconSource ? 11+frame.captionIconSize : 8; y: 0; width: Math.max(0,parent.width-x-(8 + frame.buttonsWidth)); height: frame.captionHeight
        text: frame.caption
        font.family: "Verdana"; font.pixelSize: frame.captionFontPixelSize; font.bold: true
        color: frame.windowActive ? frame.captionColor : "#909090"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    MouseArea {
        objectName: "tibiaWindowDragHandle"
        x: 4; y: 0; width: Math.max(0,parent.width-8-frame.buttonsWidth); height: frame.captionHeight
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
        objectName: "tibiaWindowLockButton"
        visible: frame.showLockButton
        width: 16; height: 14
        x: frame.width-20-(frame.showCloseButton ? 18 : 0)
        y: Math.floor((frame.captionHeight-height)/2)
        TibiaCaptionButton { anchors.fill: parent; controlType: frame.locked ? "lock" : "unlock"; pressed: lockMouse.pressed; hovered: lockMouse.containsMouse }
        MouseArea { id: lockMouse; anchors.fill: parent; hoverEnabled: true; onClicked: frame.lockRequested() }
        Controls.ToolTip.visible: lockMouse.containsMouse
        Controls.ToolTip.text: frame.lockToolTip
    }
    Item {
        objectName: "tibiaWindowCloseButton"
        visible: frame.showCloseButton
        x: frame.width-20; y: Math.floor((frame.captionHeight-height)/2)
        width: 16; height: 14
        TibiaCaptionButton { anchors.fill: parent; controlType: "close"; pressed: closeMouse.pressed; hovered: closeMouse.containsMouse }
        MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; onClicked: frame.closeRequested() }
        Controls.ToolTip.visible: closeMouse.containsMouse
        Controls.ToolTip.text: frame.closeToolTip
    }
    Item {
        objectName: "tibiaWindowCollapseButton"
        visible: frame.showCollapseButton
        width: 16; height: 14; y: Math.floor((frame.captionHeight-height)/2)
        x: frame.width-20-(Number(frame.showCloseButton)+Number(frame.showLockButton))*18
        TibiaCaptionButton { anchors.fill: parent; controlType: frame.collapsed ? "maximize" : "minimize"; pressed: collapseMouse.pressed; hovered: collapseMouse.containsMouse }
        MouseArea { id: collapseMouse; anchors.fill: parent; hoverEnabled: true; onClicked: frame.collapseRequested() }
        Controls.ToolTip.visible: collapseMouse.containsMouse
        Controls.ToolTip.text: frame.collapsed ? "Expand panel" : "Collapse panel"
    }
}

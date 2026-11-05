import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0

Dialog {
    id: root
    property bool movable: true
    property bool showCaptionCloseButton: (closePolicy & Popup.CloseOnEscape) !== 0
    property bool floatingPositionInitialized: false
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    modal: true
    focus: true
    dim: true
    anchors.centerIn: root.movable ? null : Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    padding: tibiaTheme ? 8 : 16
    background: DmeDialogBackground {}

    onOpened: {
        if (root.movable && !root.floatingPositionInitialized) {
            root.x = Math.max(0, (Overlay.overlay.width - root.width) / 2);
            root.y = Math.max(0, (Overlay.overlay.height - root.height) / 2);
            root.floatingPositionInitialized = true;
        }
    }

    header: Item {
        id: dialogHeader
        visible: root.title.length > 0
        implicitHeight: visible ? (root.tibiaTheme ? 17 : 38) : 0
        Rectangle {
            visible: !root.tibiaTheme
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: "#30363D"
        }
        TibiaSkin.TibiaSkin { anchors.fill: parent; visible: !root.tibiaTheme }
        TibiaSkin.TibiaPanelHeader {
            visible: root.tibiaTheme
            anchors.fill: parent
            caption: root.title
            movable: root.movable
            showCloseButton: root.showCaptionCloseButton
            onCloseRequested: root.reject()
            property point pressPosition
            property point dialogPosition
            onDragStarted: (x,y) => {
                pressPosition=Qt.point(x,y); dialogPosition=Qt.point(root.x,root.y);
            }
            onDragMoved: (x,y) => {
                root.x=Math.max(0,Math.min(Math.max(0,Overlay.overlay.width-root.width),dialogPosition.x+x-pressPosition.x));
                root.y=Math.max(0,Math.min(Math.max(0,Overlay.overlay.height-root.height),dialogPosition.y+y-pressPosition.y));
            }
        }
        Text {
            visible: !root.tibiaTheme
            anchors.verticalCenter: parent.verticalCenter
            x: root.tibiaTheme ? (parent.width-width)/2 : 16
            text: root.title
            color: root.tibiaTheme ? "#c0c0c0" : (((("#F0F6FC"))))
            font.bold: true
            font.pixelSize: root.tibiaTheme ? 11 : 14
            font.family: root.tibiaTheme ? "Verdana" : Qt.application.font.family
        }

        MouseArea {
            id: headerDrag
            anchors.fill: parent
            enabled: root.movable && !root.tibiaTheme
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point pressPosition: Qt.point(0, 0)
            property point dialogPosition: Qt.point(0, 0)

            onPressed: mouse => {
                pressPosition = mapToItem(Overlay.overlay, mouse.x, mouse.y);
                dialogPosition = Qt.point(root.x, root.y);
            }
            onPositionChanged: mouse => {
                if (!pressed)
                    return;
                const current = mapToItem(Overlay.overlay, mouse.x, mouse.y);
                const maxX = Math.max(0, Overlay.overlay.width - root.width);
                const maxY = Math.max(0, Overlay.overlay.height - root.height);
                root.x = Math.max(0, Math.min(maxX,
                         dialogPosition.x + current.x - pressPosition.x));
                root.y = Math.max(0, Math.min(maxY,
                         dialogPosition.y + current.y - pressPosition.y));
            }
        }
    }
    Overlay.modal: Rectangle { color: "#99000000" }
}

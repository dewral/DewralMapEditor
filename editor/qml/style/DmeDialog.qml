import QtQuick
import QtQuick.Controls
import Tibia 1.0

Dialog {
    id: root
    property bool movable: false
    property bool floatingPositionInitialized: false
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
    modal: true
    dim: modernTheme
    anchors.centerIn: root.movable ? null : Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    padding: modernTheme ? 16 : 12
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
        implicitHeight: visible ? (root.modernTheme ? 38 : 28) : 0
        Rectangle {
            visible: root.modernTheme
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: root.grayTheme ? "#3A3A3A" : "#30363D"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            x: root.modernTheme ? 16 : (parent.width - width) / 2
            text: root.title
            color: root.grayTheme ? "#F0F0F0" : (root.modernTheme ? "#F0F6FC" : "#c0c0c0")
            font.bold: true
            font.pixelSize: root.modernTheme ? 14 : 13
        }

        MouseArea {
            id: headerDrag
            anchors.fill: parent
            enabled: root.movable
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

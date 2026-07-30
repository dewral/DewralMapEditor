pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window

Item {
    id: root

    required property Window targetWindow
    property int handleSize: 6

    anchors.fill: parent
    enabled: targetWindow.visibility !== Window.Maximized
    z: 10000

    component ResizeArea: MouseArea {
        required property int edges
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        preventStealing: true
        onPressed: root.targetWindow.startSystemResize(edges)
    }

    ResizeArea {
        edges: Qt.LeftEdge
        cursorShape: Qt.SizeHorCursor
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            topMargin: root.handleSize
            bottomMargin: root.handleSize
        }
        width: root.handleSize
    }

    ResizeArea {
        edges: Qt.RightEdge
        cursorShape: Qt.SizeHorCursor
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            topMargin: root.handleSize
            bottomMargin: root.handleSize
        }
        width: root.handleSize
    }

    ResizeArea {
        edges: Qt.TopEdge
        cursorShape: Qt.SizeVerCursor
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: root.handleSize
            rightMargin: root.handleSize
        }
        height: root.handleSize
    }

    ResizeArea {
        edges: Qt.BottomEdge
        cursorShape: Qt.SizeVerCursor
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: root.handleSize
            rightMargin: root.handleSize
        }
        height: root.handleSize
    }

    ResizeArea {
        edges: Qt.LeftEdge | Qt.TopEdge
        cursorShape: Qt.SizeFDiagCursor
        anchors {
            left: parent.left
            top: parent.top
        }
        width: root.handleSize
        height: root.handleSize
    }

    ResizeArea {
        edges: Qt.RightEdge | Qt.TopEdge
        cursorShape: Qt.SizeBDiagCursor
        anchors {
            right: parent.right
            top: parent.top
        }
        width: root.handleSize
        height: root.handleSize
    }

    ResizeArea {
        edges: Qt.LeftEdge | Qt.BottomEdge
        cursorShape: Qt.SizeBDiagCursor
        anchors {
            left: parent.left
            bottom: parent.bottom
        }
        width: root.handleSize
        height: root.handleSize
    }

    ResizeArea {
        edges: Qt.RightEdge | Qt.BottomEdge
        cursorShape: Qt.SizeFDiagCursor
        anchors {
            right: parent.right
            bottom: parent.bottom
        }
        width: root.handleSize
        height: root.handleSize
    }
}

import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: statsDialog
    title: "Map Statistics"

    property var h: Backend.otbmReader.header()
    onAboutToShow: h = Backend.otbmReader.header()

    contentItem: Column {
        spacing: 10

        Grid {
            columns: 2
            rowSpacing: 4
            columnSpacing: 18
            Text {
                text: "Dimensions:"
                color: "#999"
                font.pixelSize: 12
            }
            Text {
                text: statsDialog.h.width + " x " + statsDialog.h.height
                color: "#c0c0c0"
                font.pixelSize: 12
            }
            Text {
                text: "Tiles:"
                color: "#999"
                font.pixelSize: 12
            }
            Text {
                text: "" + statsDialog.h.tileCount
                color: "#c0c0c0"
                font.pixelSize: 12
            }
            Text {
                text: "Items:"
                color: "#999"
                font.pixelSize: 12
            }
            Text {
                text: "" + statsDialog.h.itemCount
                color: "#c0c0c0"
                font.pixelSize: 12
            }
            Text {
                text: "Towns:"
                color: "#999"
                font.pixelSize: 12
            }
            Text {
                text: "" + statsDialog.h.townCount
                color: "#c0c0c0"
                font.pixelSize: 12
            }
            Text {
                text: "Waypoints:"
                color: "#999"
                font.pixelSize: 12
            }
            Text {
                text: "" + statsDialog.h.waypointCount
                color: "#c0c0c0"
                font.pixelSize: 12
            }
        }

        DmeButton {
            text: "Close"
            width: 90
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: statsDialog.close()
        }
    }
}

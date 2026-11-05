import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: dialog

    title: "About"
    width: 440

    readonly property color primaryText: (("#E6EDF3"))
    readonly property color secondaryText: (("#8B949E"))
    readonly property string githubUrl: "https://github.com/dewral/DewralMapEditor"

    contentItem: Column {
        spacing: 14

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Dewral Map Editor"
            color: dialog.primaryText
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Version " + Qt.application.version
            color: dialog.secondaryText
            font.pixelSize: 12
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "<a href=\"" + dialog.githubUrl + "\">" + dialog.githubUrl + "</a>"
            textFormat: Text.RichText
            color: dialog.primaryText
            linkColor: "#58A6FF"
            font.pixelSize: 12
            onLinkActivated: link => Qt.openUrlExternally(link)

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: (("#30363D"))
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Official sponsor"
            color: dialog.secondaryText
            font.pixelSize: 11
        }

        Image {
            id: sponsorLogo
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(260, sourceSize.width)
            height: Math.min(90, sourceSize.height)
            visible: status === Image.Ready
            source: "qrc:/ui/Midhem.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Midhem Online"
            color: dialog.primaryText
            font.pixelSize: 15
            font.bold: true
        }

        DmeButton {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 90
            text: "Close"
            onClicked: dialog.close()
        }
    }
}

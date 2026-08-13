import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    title: "DME Update"
    width: 470
    closePolicy: Backend.updateService.busy ? Popup.NoAutoClose : Popup.CloseOnEscape

    readonly property color primaryText: Backend.uiTheme.style === "gray-dark"
                                         ? "#F0F0F0" : (Backend.uiTheme.style !== "classic"
                                                           ? "#E6EDF3" : "#C0C0C0")
    readonly property color secondaryText: Backend.uiTheme.style === "gray-dark"
                                           ? "#A0A0A0" : (Backend.uiTheme.style !== "classic"
                                                             ? "#8B949E" : "#8A8A8A")

    function statusText() {
        switch (Backend.updateService.state) {
        case "checking": return "Checking GitHub for updates...";
        case "available": return "A new version is available.";
        case "upToDate": return "DME is up to date.";
        case "downloading": return "Downloading and verifying the update...";
        case "installing": return "Starting the updater...";
        case "error": return Backend.updateService.errorString;
        default: return "Ready to check for updates.";
        }
    }

    contentItem: Column {
        spacing: 12

        Text {
            width: parent.width
            text: dialog.statusText()
            color: Backend.updateService.state === "error" ? "#F85149" : dialog.primaryText
            font.pixelSize: 13
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Grid {
            columns: 2
            columnSpacing: 16
            rowSpacing: 5
            Text { text: "Installed"; color: dialog.secondaryText; font.pixelSize: 11 }
            Text {
                text: Backend.updateService.currentVersion
                      + (Backend.updateService.currentCommit !== "unknown"
                         ? " (" + Backend.updateService.currentCommit + ")" : "")
                color: dialog.primaryText
                font.pixelSize: 11
            }
            Text {
                visible: Backend.updateService.latestVersion.length > 0
                text: "Available"
                color: dialog.secondaryText
                font.pixelSize: 11
            }
            Text {
                visible: Backend.updateService.latestVersion.length > 0
                text: Backend.updateService.latestVersion
                      + (Backend.updateService.latestCommit.length > 0
                         ? " (" + Backend.updateService.latestCommit.substring(0, 12) + ")" : "")
                color: dialog.primaryText
                font.pixelSize: 11
            }
        }

        ProgressBar {
            width: parent.width
            visible: Backend.updateService.state === "downloading"
            from: 0
            to: 1
            value: Backend.updateService.downloadProgress
        }

        Rectangle {
            width: parent.width
            height: 150
            visible: Backend.updateService.releaseNotes.length > 0
                     && Backend.updateService.updateAvailable
            color: Backend.uiTheme.style === "gray-dark" ? "#202020"
                  : (Backend.uiTheme.style !== "classic" ? "#0D1117" : "#202020")
            border.width: 1
            border.color: Backend.uiTheme.style === "gray-dark" ? "#454545" : "#30363D"
            radius: Backend.uiTheme.style !== "classic" ? 5 : 0

            Flickable {
                anchors.fill: parent
                anchors.margins: 9
                contentWidth: width
                contentHeight: notes.implicitHeight
                clip: true

                Text {
                    id: notes
                    width: parent.width
                    text: Backend.updateService.releaseNotes
                    textFormat: Text.MarkdownText
                    color: dialog.secondaryText
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
        }

        Text {
            width: parent.width
            visible: Backend.updateService.updateAvailable
                     && Backend.docMgr.hasDirtyDocuments
            text: "Save or close all modified maps before installing the update."
            color: "#D29922"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 7
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Download and restart"
                width: 165
                variant: "primary"
                visible: Backend.updateService.updateAvailable
                enabled: !Backend.updateService.busy && !Backend.docMgr.hasDirtyDocuments
                onClicked: Backend.updateService.downloadAndInstall()
            }
            DmeButton {
                text: "Release page"
                width: 105
                visible: Backend.updateService.releasePageUrl.toString().length > 0
                enabled: !Backend.updateService.busy
                onClicked: Backend.updateService.openReleasePage()
            }
            DmeButton {
                text: Backend.updateService.busy ? "Cancel" : "Close"
                width: 90
                onClicked: {
                    if (Backend.updateService.busy)
                        Backend.updateService.cancel();
                    dialog.close();
                }
            }
        }
    }
}

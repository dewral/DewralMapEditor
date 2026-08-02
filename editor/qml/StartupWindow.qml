import Tibia 1.0
import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Controls
import "dialogs"
import "style"

Window {
    id: startupScreen

    required property var app

    required property var settings
    property bool loadingMap: false
    property string loadingMapPath: ""
    property string loadingProfileKey: ""

    function beginLoadMap(path, profileKey) {
        if (!path || loadingMap)
            return;
        loadingMapPath = path;
        loadingProfileKey = profileKey || "";
        Backend.otbmReader.reportLoadingProgress(0, "Preparing map...");
        loadingMap = true;
        loadDelay.restart();
    }

    function openMapDialog() {
        startMapDialog.open();
    }
    function openVersionFolderDialog() {
        versionFolderDialogStartup.open();
    }
    function beginRecoveryLoad(path) {
        loadingMapPath = path;
        loadingProfileKey = "";
        loadingMap = true;
    }

    transientParent: null
    visible: !app.started
    width: loadingMap ? 460 : 800
    height: loadingMap ? 190 : 520
    minimumWidth: loadingMap ? 460 : 800
    maximumWidth: loadingMap ? 460 : 800
    minimumHeight: loadingMap ? 190 : 520
    maximumHeight: loadingMap ? 190 : 520
    x: Screen.width / 2 - width / 2
    y: Screen.height / 2 - height / 2
    title: "Dewral Map Editor"

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    Timer {
        id: loadDelay
        interval: 80
        repeat: false
        onTriggered: {
            if (!startupScreen.app.loadEverything(startupScreen.loadingMapPath,
                                                  startupScreen.loadingProfileKey))
                startupScreen.loadingMap = false;
        }
    }

    Connections {
        target: Backend.otbmReader
        function onLoadingChanged() {
            if (!Backend.otbmReader.loading)
                startupScreen.loadingMap = false;
        }
    }

    DmeDialogBackground {
        id: card
        visible: !startupScreen.loadingMap && Backend.docMgr.recoveryCount === 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 760)
        height: Math.min(parent.height - 80, 440)

        Item {
            id: titleBar
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: 27

            Text {
                anchors.centerIn: parent
                text: startupScreen.title
                color: Backend.uiTheme.style === "github-dark" ? "#F0F6FC" : "#c0c0c0"
                font.bold: true
                font.pixelSize: 13
            }

            Row {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }

                GithubWindowButton {
                    height: parent.height
                    compact: true
                    controlType: "minimize"
                    onTriggered: startupScreen.showMinimized()
                }

                GithubWindowButton {
                    height: parent.height
                    compact: true
                    controlType: "close"
                    onTriggered: Qt.quit()
                }
            }

            MouseArea {
                anchors.fill: parent
                anchors.rightMargin: 76
                onPressed: startupScreen.startSystemMove()
            }
        }

        Row {
            anchors.fill: parent
            anchors.topMargin: titleBar.height + 8
            anchors.margins: 8
            spacing: 8

            DmePanel {
                width: 280
                height: parent.height
                Column {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14

                    Column {
                        spacing: 4
                        Text {
                            text: "Dewral Map Editor"
                            color: "#F0F6FC"
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Rectangle {
                            width: 180
                            height: 2
                            color: Backend.uiTheme.style === "github-dark" ? "#2EA043" : "#4a90e2"
                        }
                    }
                    Text {
                        text: "Tibia 7.72 - 10.98+ | OpenGL"
                        color: "#888"
                        font.pixelSize: 12
                    }

                    Item {
                        width: 1
                        height: 8
                    }

                    Row {
                        spacing: 6

                        DmeButton {
                            width: 113
                            height: 40
                            text: "Open map..."
                            variant: "primary"
                            onClicked: startMapDialog.open()
                        }

                        DmeButton {
                            width: 113
                            height: 40
                            text: "New map..."
                            onClicked: startupNewMapDialog.open()
                        }
                    }

                    Text {
                        text: "Client versions"
                        color: "#ddd"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Row {
                        spacing: 6
                        DmeComboBox {
                            id: verCombo
                            width: 150
                            height: 23

                            model: app.allProfileKeys().map(function (k) {
                                return app.profileLabel(k);
                            })
                            currentIndex: 0
                            readonly property string selKey: {
                                var keys = app.allProfileKeys();
                                return currentIndex >= 0 && currentIndex < keys.length ? keys[currentIndex] : "772";
                            }
                        }
                        DmeButton {
                            width: 76
                            height: 23
                            text: "Folder..."
                            onClicked: {
                                app.pendingKey = verCombo.selKey;
                                app.pendingMapPath = "";
                                versionFolderDialogStartup.open();
                            }
                        }
                    }

                    Row {
                        spacing: 6
                        DmeTextField {
                            id: newProfileField
                            width: 150
                            height: 23
                            placeholderText: "e.g. Midhem"
                        }
                        DmeButton {
                            width: 76
                            height: 23
                            text: "+ Custom"
                            onClicked: {
                                var base = app.profileVer(verCombo.selKey);
                                var name = newProfileField.text.trim();
                                if (app.addCustomProfile(name, base)) {
                                    newProfileField.text = "";

                                    verCombo.currentIndex = app.allProfileKeys().indexOf(name);
                                }
                            }
                        }
                    }

                    DmeButton {
                        visible: app.isCustomKey(verCombo.selKey)
                        width: 232
                        height: 21
                        text: "Remove profile " + verCombo.selKey
                        onClicked: {
                            app.removeCustomProfile(verCombo.selKey);
                            verCombo.currentIndex = 0;
                        }
                    }

                    Column {
                        spacing: 2
                        property string selFolder: app.clientPaths[verCombo.selKey] || ""
                        property var selFiles: app.clientFiles(selFolder)
                        Text {
                            width: 232
                            elide: Text.ElideMiddle
                            text: parent.selFolder !== "" ? parent.selFolder : "(folder not set for this version)"
                            color: "#999"
                            font.pixelSize: 10
                        }
                        Text {
                            font.pixelSize: 11
                            visible: parent.selFolder !== ""
                            property bool ok: parent.selFiles.dat && parent.selFiles.spr && parent.selFiles.otb
                            color: ok ? "#7fdc8f" : "#e08a6a"
                            text: ok ? "OK: dat / spr / otb found" : "Missing: " + (parent.selFiles.dat ? "" : "dat ") + (parent.selFiles.spr ? "" : "spr ") + (parent.selFiles.otb ? "" : "otb ") + "missing"
                        }

                        Text {
                            width: 232
                            wrapMode: Text.WordWrap
                            text: {
                                var keys = Object.keys(app.clientPaths).sort(function (a, b) {
                                    var na = Number(a), nb = Number(b);
                                    var ca = isNaN(na), cb = isNaN(nb);
                                    if (ca !== cb)
                                        return ca ? 1 : -1;
                                    return ca ? a.localeCompare(b) : na - nb;
                                });
                                if (keys.length === 0)
                                    return "No versions configured yet.";
                                return "Configured: " + keys.map(function (k) {
                                    return app.profileLabel(k);
                                }).join(", ");
                            }
                            color: "#7a9a7a"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            DmePanel {
                width: parent.width - 288
                height: parent.height

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 8

                    Text {
                        text: "Recent maps"
                        color: "#ddd"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        visible: app.recentMaps.length === 0
                        text: "No recent maps yet.\nUse Open map... to load one."
                        color: "#777"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: recentList
                        width: parent.width
                        height: parent.height - 30
                        clip: true
                        model: app.recentMaps
                        spacing: 4

                        delegate: Item {
                            width: recentList.width
                            height: 44

                            Rectangle {
                                anchors.fill: parent
                                radius: Backend.uiTheme.style === "github-dark" ? 5 : 0
                                color: Backend.uiTheme.style === "github-dark"
                                       ? (rma.pressed ? "#21262D" : (rma.containsMouse ? "#161E27" : "#0D1117"))
                                       : (rma.pressed ? "#14ffffff" : (rma.containsMouse ? "#0affffff" : "transparent"))
                                border.color: Backend.uiTheme.style === "github-dark" ? "#30363D" : "#555"
                                border.width: 1
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 1
                                Text {
                                    text: Backend.fileTools.fileName(modelData)
                                    color: "#eee"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: modelData
                                    color: "#888"
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                }
                            }
                            MouseArea {
                                id: rma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!Backend.fileTools.exists(modelData))
                                        return;
                                    startupScreen.beginLoadMap(modelData, verCombo.selKey);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    DmeDialogBackground {
        visible: !startupScreen.loadingMap && Backend.docMgr.recoveryCount > 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 620)
        height: Math.min(parent.height - 80, 330)
        frameSource: (Backend.uiTheme.tex + "popupwindow_tall.png")
        topBorder: 45

        Column {
            anchors {
                fill: parent
                margins: 28
                topMargin: 52
            }
            spacing: 14

            Text {
                width: parent.width
                text: "Recover unsaved maps"
                color: Backend.uiTheme.style === "github-dark" ? "#F0F6FC" : "#d6d6d6"
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                width: parent.width
                text: "DME found " + Backend.docMgr.recoveryCount
                      + (Backend.docMgr.recoveryCount === 1
                         ? " map from an interrupted session."
                         : " maps from an interrupted session.")
                color: Backend.uiTheme.style === "github-dark" ? "#9DA7B3" : "#b8b8b8"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Column {
                width: parent.width
                spacing: 5
                Repeater {
                    model: Backend.docMgr.recoveries.slice(0, 5)
                    Text {
                        required property var modelData
                        width: parent.width
                        text: "• " + modelData.title + "  —  " + modelData.savedAt
                        color: Backend.uiTheme.style === "github-dark" ? "#C9D1D9" : "#c0c0c0"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
            }
            Item { width: 1; height: 8 }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                DmeButton {
                    text: "Recover all"
                    width: 150
                    variant: "primary"
                    onClicked: startupScreen.app.recoverPreviousSession()
                }
                DmeButton {
                    text: "Discard recovery"
                    width: 150
                    variant: "danger"
                    onClicked: startupScreen.app.discardPreviousSession()
                }
            }
        }
    }

    DmeDialogBackground {
        id: loadingCard
        visible: startupScreen.loadingMap
        anchors.centerIn: parent
        width: 420
        height: 142

        Column {
            anchors {
                fill: parent
                margins: 24
            }
            spacing: 10

            Text {
                text: "Loading map"
                color: "#F0F6FC"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Item {
                width: parent.width
                height: 18

                Text {
                    anchors {
                        left: parent.left
                        right: progressPercent.left
                        rightMargin: 12
                        verticalCenter: parent.verticalCenter
                    }
                    text: Backend.fileTools.fileName(startupScreen.loadingMapPath)
                    color: "#C9D1D9"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }

                Text {
                    id: progressPercent
                    anchors {
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    text: Backend.otbmReader.loadingProgress + "%"
                    color: "#56D364"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            Rectangle {
                id: progressTrack
                width: parent.width
                height: 7
                radius: 4
                color: "#21262D"
                clip: true

                Rectangle {
                    id: progressChunk
                    width: progressTrack.width * Backend.otbmReader.loadingProgress / 100
                    height: parent.height
                    radius: 4
                    color: "#2EA043"

                    Behavior on width {
                        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                    }
                }
            }

            Text {
                text: Backend.otbmReader.loadingStage.length > 0
                      ? Backend.otbmReader.loadingStage
                      : "Preparing map..."
                color: "#7D8590"
                font.pixelSize: 11
            }
        }
    }

    FolderDialog {
        id: versionFolderDialogStartup
        title: "Select client folder for " + app.profileLabel(app.pendingKey) + " (Tibia.dat / Tibia.spr / items.otb)"
        onAccepted: app.onVersionFolderPicked(selectedFolder)
    }

    FileDialog {
        id: startMapDialog
        title: "Open .otbm map"
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        onAccepted: startupScreen.beginLoadMap(Backend.fileTools.toLocalFile(selectedFile),
                                               verCombo.selKey)
    }

    NewMapDialog {
        id: startupNewMapDialog
        app: startupScreen.app
        preferredProfileKey: verCombo.selKey
    }
}

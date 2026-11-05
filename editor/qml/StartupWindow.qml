import Tibia 1.0
import QtQuick
import "themes/tibia" as TibiaSkin
import QtQuick.Window
import QtQuick.Controls
import "style"
import "components"
import "themes/fluent/Colors.js" as Colors

Window {
    id: startupScreen

    readonly property bool nativeDashboard: Backend.uiTheme.style === "tibia-ui" && typeof app.profiles!=="undefined"
    required property var app

    required property var settings
    property bool editorReady: app.started
    property bool loadingMap: false
    property string loadingMapPath: ""
    property string loadingProfileKey: ""

    Component.onCompleted: Backend.updateService.checkForUpdates()

    function showError(message) {
        loadingMap = false;
        if (startupError.ensureLoaded()) {
            startupError.item.message = message;
            startupError.item.open();
        }
    }
    LazyToolHost {
        id: startupError
        toolSource: Qt.resolvedUrl("tools/StartupErrorDialog.qml")
    }

    function selectClientProfile(key) {
        settings.selectedClientKey = key;
        // Activation assigns currentIndex, so restore the binding to the saved key.
        verCombo.currentIndex = Qt.binding(function () {
            return Math.max(0, app.allProfileKeys().indexOf(settings.selectedClientKey));
        });
    }

    function beginLoadMap(path, profileKey) {
        if (!path || loadingMap)
            return;
        loadingMapPath = path;
        loadingProfileKey = profileKey || "";
        Backend.otbmReader.reportLoadingProgress(0, "Preparing map...");
        loadingMap = true;
        if (!app.loadEverything(loadingMapPath, loadingProfileKey))
            loadingMap = false;
    }

    readonly property string selectedProfileKey: dashboard.item ? dashboard.item.selectedKey : verCombo.selKey
    function openClientProfiles() {
        if(!profileHost.ensureLoaded())return;
        profileHost.item.transientParent=app.editorWindow || startupScreen;
        profileHost.item.openFor(selectedProfileKey || "772");
    }
    LazyToolHost {
        id:profileHost;objectName:"clientProfilesHost"
        toolSource:Qt.resolvedUrl("components/ClientProfilesWindow.qml")
        initialProperties:({profiles:typeof startupScreen.app.profiles!=="undefined" ? startupScreen.app.profiles : null})
    }
    function chooseClientProfile(version, reason, callback) {
        loadingMap=false;
        clientChoice.version=version;clientChoice.reason=reason;clientChoice.callback=callback;
        clientChoice.parent=(app.editorWindow || startupScreen).contentItem;
        clientChoice.open();
    }
    DmeDialog {
        id:clientChoice;title:"Choose client profile";width:480
        property int version:772
        property string reason:""
        property var callback:null
        readonly property var keys:typeof app.profiles!=="undefined" ? app.profiles.configuredProfileKeys().filter(k=>app.profiles.profileMatchesVersion(k,version)) : []
        onRejected:{const done=callback;callback=null;if(done)done("");}
        contentItem:Column {
            spacing:12
            Text {width:450;wrapMode:Text.WordWrap;color:"#d5d5ce";text:clientChoice.reason}
            DmeComboBox {id:choiceCombo;width:450;model:clientChoice.keys.map(k=>app.profileLabel(k));currentIndex:0}
            Row {spacing:8
                DmeButton {text:"Manage clients…";onClicked:startupScreen.openClientProfiles()}
                DmeButton {text:"Cancel";onClicked:clientChoice.reject()}
                DmeButton {text:"Use profile";enabled:clientChoice.keys.length>0;onClicked:{const done=clientChoice.callback;clientChoice.callback=null;const key=clientChoice.keys[choiceCombo.currentIndex];clientChoice.close();if(done)done(key);}}
            }
        }
    }
    Loader {
        id:dashboard;anchors.fill:parent
        active:startupScreen.nativeDashboard && !startupScreen.loadingMap && Backend.docMgr.recoveryCount===0
        sourceComponent:StartupDashboard {startup:startupScreen}
    }
    function openMapDialog() {
        if (openMapHost.ensureLoaded()) openMapHost.item.open();
    }
    function openNewMapDialog() {
        if (newMapHost.ensureLoaded()) newMapHost.item.open();
    }
    function openVersionFolderDialog() {
        if (folderHost.ensureLoaded()) folderHost.item.open();
    }
    function beginRecoveryLoad(path) {
        loadingMapPath = path;
        loadingProfileKey = "";
        loadingMap = true;
    }

    transientParent: null
    visible: !editorReady
    width: loadingMap ? 460 : nativeDashboard ? 1000 : 800
    height: loadingMap ? 190 : nativeDashboard ? 660 : 600
    minimumWidth: loadingMap ? 460 : nativeDashboard ? 900 : 800
    maximumWidth: loadingMap ? 460 : nativeDashboard ? 16777215 : 800
    minimumHeight: loadingMap ? 190 : nativeDashboard ? 600 : 600
    maximumHeight: loadingMap ? 190 : nativeDashboard ? 16777215 : 600
    x: Screen.width / 2 - width / 2
    y: Screen.height / 2 - height / 2
    title: "Dewral Map Editor"

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    Connections {
        target: Backend.otbmReader
        function onLoadingChanged() {
            if (!Backend.otbmReader.loading)
                startupScreen.loadingMap = false;
        }
    }

    DmeDialogBackground {
        id: card
        visible: !startupScreen.nativeDashboard && !startupScreen.loadingMap && Backend.docMgr.recoveryCount === 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 760)
        height: Math.min(parent.height - 80, 520)

        Rectangle {
            anchors.fill: parent
            visible: true
            color: Colors.c("background")
            TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "window" }
            border.color: Colors.c("border")
            radius: 6
        }

        Item {
            id: titleBar
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: 34

            Image {
                visible: true
                x: 12; y: (parent.height - height) / 2; width: 16; height: 16
                source: "qrc:/ui/app-icon.png"
            }
            Rectangle { visible: true; anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.c("separator") }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 38
                text: startupScreen.title
                color: (Colors.c("buttonText"))
                font.bold: false
                font.family: (Colors.family())
                font.pixelSize: 13
            }

            Row {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }

                DmeWindowButton {
                    height: parent.height
                    compact: true
                    controlType: "minimize"
                    onTriggered: startupScreen.showMinimized()
                }

                DmeWindowButton {
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
                    anchors.margins: 20
                    spacing: 9

                    Column {
                        spacing: 4
                        Text {
                            text: "Dewral Map Editor"
                            color: (Colors.c("heading"))
                            font.family: (Colors.family())
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }
                    Text {
                        text: "Tibia 7.72 – 10.98+"
                        color: (Colors.c("muted"))
                        font.family: (Colors.family())
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
                            onClicked: startupScreen.openMapDialog()
                        }

                        DmeButton {
                            width: 113
                            height: 40
                            text: "New map..."
                            onClicked: startupScreen.openNewMapDialog()
                        }
                    }

                    Text {
                        text: "Client versions"
                        color: (Colors.c("heading"))
                        font.family: (Colors.family())
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Row {
                        spacing: 6
                        DmeComboBox {
                            id: verCombo
                            objectName: "startupClientVersion"
                            width: 150
                            height: 28

                            model: app.allProfileKeys().map(function (k) {
                                return app.profileLabel(k);
                            })
                            currentIndex: Math.max(0, app.allProfileKeys().indexOf(settings.selectedClientKey))
                            onActivated: startupScreen.selectClientProfile(selKey)
                            readonly property string selKey: {
                                var keys = app.allProfileKeys();
                                return currentIndex >= 0 && currentIndex < keys.length ? keys[currentIndex] : "772";
                            }
                        }
                        DmeButton {
                            width: 76
                            height: 28
                            text: "Folder..."
                            onClicked: {
                                app.selectVersionFolder(verCombo.selKey);
                            }
                        }
                    }

                    Row {
                        spacing: 6
                        DmeTextField {
                            id: newProfileField
                            width: 150
                            height: 28
                            placeholderText: "e.g. Midhem"
                        }
                        DmeButton {
                            width: 76
                            height: 28
                            text: "+ Custom"
                            onClicked: {
                                var base = app.profileVer(verCombo.selKey);
                                var name = newProfileField.text.trim();
                                if (app.addCustomProfile(name, base)) {
                                    newProfileField.text = "";

                                    startupScreen.selectClientProfile(name);
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
                            startupScreen.selectClientProfile(app.allProfileKeys()[0]);
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
                            color: (Colors.c("muted"))
                            font.family: (Colors.family())
                            font.pixelSize: 10
                        }
                        Text {
                            font.family: (Colors.family())
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
                            color: (Colors.c("muted"))
                            font.family: (Colors.family())
                            font.pixelSize: 10
                        }
                    }

                    Column {
                        width: 232
                        spacing: 4

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: (Colors.c("separator"))
                        }

                        Text {
                            text: "Updates (v" + Backend.updateService.currentVersion + ")"
                            color: (Colors.c("heading"))
                            font.family: (Colors.family())
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Row {
                            width: parent.width
                            height: 23
                            spacing: 6

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: updateAction.visible ? parent.width - updateAction.width - parent.spacing
                                                            : parent.width
                                text: {
                                    switch (Backend.updateService.state) {
                                    case "checking": return "Checking for updates...";
                                    case "available": return "Version " + Backend.updateService.latestVersion + " available";
                                    case "downloading": return "Downloading... " + Math.round(Backend.updateService.downloadProgress * 100) + "%";
                                    case "installing": return "Starting installer...";
                                    case "error": return "Unable to check for updates";
                                    case "upToDate": return "No newer stable release";
                                    default: return "Checking for updates...";
                                    }
                                }
                                color: Backend.updateService.state === "error" ? "#D29922"
                                     : Backend.updateService.updateAvailable ? "#7FDC8F" : "#888"
                                font.family: (Colors.family())
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }

                            DmeButton {
                                id: updateAction
                                width: Backend.updateService.state === "available" ? 112 : 58
                                height: 23
                                visible: Backend.updateService.state === "available"
                                      || Backend.updateService.state === "downloading"
                                      || Backend.updateService.state === "error"
                                enabled: Backend.updateService.state !== "installing"
                                variant: Backend.updateService.state === "available" ? "primary" : "default"
                                text: Backend.updateService.state === "available" ? "Download & install"
                                    : Backend.updateService.state === "downloading" ? "Cancel" : "Retry"
                                onClicked: {
                                    if (Backend.updateService.state === "available")
                                        Backend.updateService.downloadAndInstall();
                                    else if (Backend.updateService.state === "downloading")
                                        Backend.updateService.cancel();
                                    else
                                        Backend.updateService.checkForUpdates();
                                }
                            }
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
                        color: (Colors.c("heading"))
                        font.family: (Colors.family())
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        visible: app.recentMaps.length === 0
                        text: "No recent maps yet.\nUse Open map... to load one."
                        color: (Colors.c("placeholder"))
                        font.family: (Colors.family())
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
                            height: 50

                            Rectangle {
                                anchors.fill: parent
                                radius: 4
                                color: ((rma.pressed ? Colors.c("selected") : rma.containsMouse ? Colors.c("hover") : Colors.c("base")))
                                border.color: ((rma.pressed ? Colors.c("selectedBorder") : Colors.c("border")))
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
                                    color: (Colors.c("text"))
                                    font.family: (Colors.family())
                                    font.pixelSize: 13
                                    font.bold: false
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: modelData
                                    color: (Colors.c("placeholder"))
                                    font.family: (Colors.family())
                                    font.pixelSize: 11
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

    Loader {
        objectName: "startupRecoveryHost"
        anchors.centerIn: parent
        width: Math.min(startupScreen.width - 40, 620)
        height: Math.min(startupScreen.height - 80, 330)
        active: !startupScreen.loadingMap && Backend.docMgr.recoveryCount > 0
        sourceComponent: DmeDialogBackground {
            visible: !startupScreen.loadingMap && Backend.docMgr.recoveryCount > 0

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
                    color: (Colors.c("heading"))
                    font.family: (Colors.family())
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    width: parent.width
                    text: "DME found " + Backend.docMgr.recoveryCount
                          + (Backend.docMgr.recoveryCount === 1
                             ? " map from an interrupted session."
                             : " maps from an interrupted session.")
                    color: (Colors.c("muted"))
                    font.family: (Colors.family())
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
                            color: (Colors.c("text"))
                            font.family: (Colors.family())
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
    }

    Loader {
        objectName: "startupLoadingHost"
        anchors.centerIn: parent
        width: 420
        height: 142
        active: startupScreen.loadingMap
        sourceComponent: DmeDialogBackground {
            id: loadingCard
            visible: startupScreen.loadingMap

            Column {
                anchors {
                    fill: parent
                    margins: 24
                }
                spacing: 10

                Text {
                    text: "Loading map"
                    color: (Colors.c("heading"))
                    font.family: (Colors.family())
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
                        color: (Colors.c("text"))
                        font.family: (Colors.family())
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
                        color: (Colors.c("text"))
                        font.family: (Colors.family())
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }

                Rectangle {
                    id: progressTrack
                    objectName: "startupProgressTrack"
                    readonly property bool classicBar: Backend.uiTheme.style === "tibia-ui"
                    width: parent.width
                    height: classicBar ? 14 : 7
                    radius: classicBar ? height / 2 : 4
                    color: classicBar ? "#202020" : (Colors.c("scrollTrack"))
                    border.width: classicBar ? 1 : 0
                    border.color: "#181b17"
                    gradient: classicBar ? emptyBarGradient : null
                    Gradient {
                        id: emptyBarGradient
                        GradientStop { position: 0; color: "#1a1c18" }
                        GradientStop { position: 0.5; color: "#292d26" }
                        GradientStop { position: 1; color: "#1d211b" }
                    }

                    Rectangle {
                        id: progressChunk
                        objectName: "startupProgressFill"
                        width: Math.round(progressTrack.width * Math.max(0, Math.min(100, Backend.otbmReader.loadingProgress)) / 100)
                        visible: width > 0
                        height: parent.height
                        radius: progressTrack.classicBar ? Math.min(height / 2, width / 2) : 4
                        border.width: progressTrack.classicBar ? 1 : 0
                        border.color: "#35572a"
                        gradient: progressTrack.classicBar ? filledBarGradient : null
                        Gradient {
                            id: filledBarGradient
                            // OTCv8 darkens the fill towards the bottom; avoid a central gloss band.
                            GradientStop { position: 0; color: "#72ad5b" }
                            GradientStop { position: 1; color: "#487a38" }
                        }
                        color: progressTrack.classicBar ? "#5d9449" : (Colors.c("loading"))

                        Behavior on width {
                            NumberAnimation { duration: 25; easing.type: Easing.OutCubic }
                        }
                    }

                }

                Text {
                    text: Backend.otbmReader.loadingStage.length > 0
                          ? Backend.otbmReader.loadingStage
                          : "Preparing map..."
                    color: "#7D8590"
                    font.family: (Colors.family())
                    font.pixelSize: 11
                }
            }
        }
    }

    LazyToolHost {
        id: folderHost
        objectName: "startupFolderHost"
        toolSource: Qt.resolvedUrl("tools/StartupFolderDialog.qml")
        initialProperties: ({startupWindow: startupScreen})
    }
    LazyToolHost {
        id: openMapHost
        objectName: "startupOpenMapHost"
        toolSource: Qt.resolvedUrl("tools/StartupOpenMapDialog.qml")
        initialProperties: ({startupWindow: startupScreen})
    }
    LazyToolHost {
        id: newMapHost
        objectName: "startupNewMapHost"
        toolSource: Qt.resolvedUrl("tools/StartupNewMapDialog.qml")
        initialProperties: ({startupWindow: startupScreen})
    }
}

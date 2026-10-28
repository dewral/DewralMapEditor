pragma ComponentBehavior: Bound

import QtQuick
import "themes/tibia" as TibiaSkin
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import Tibia 1.0
import "dialogs"
import "style"
import "controllers"
import "components"
import "themes/github"
import "themes/fluent/Colors.js" as FluentColors

DmeWindow {
    id: root
    readonly property bool clientBusy: typeof Backend.clientLoader !== "undefined" && Backend.clientLoader.busy
    required property var applicationSession
    readonly property var prefs: applicationSession.settings
    readonly property alias appController: app
    readonly property alias profileMapView: workspace.mapView

    function toolEnvironment() {
        return ({activityLoader: activityLoader, aiMapAssistantLoader: aiMapAssistantLoader, app: app, browseFieldLoader: browseFieldLoader, brushEditorLoader: brushEditorLoader, cleanupLoader: cleanupLoader, colorizeLoader: colorizeLoader, containerDialog: containerDialog, containerLoader: containerLoader, creatureManagerLoader: creatureManagerLoader, customMapGeneratorLoader: customMapGeneratorLoader, dungeonGeneratorLoader: dungeonGeneratorLoader, exportMinimapLoader: exportMinimapLoader, findItemLoader: findItemLoader, gotoPosLoader: gotoPosLoader, groundClusterGeneratorLoader: groundClusterGeneratorLoader, importMapLoader: importMapLoader, luaGeneratorLoader: luaGeneratorLoader, mapPropsLoader: mapPropsLoader, newMapLoader: newMapLoader, numberedZonesLoader: numberedZonesLoader, palette: palette, prefs: prefs, propsDialog: propsDialog, propsDialogLoader: propsDialogLoader, searchResultsLoader: searchResultsLoader, selectionItemLoader: selectionItemLoader, statsLoader: statsLoader, terrainGeneratorLoader: terrainGeneratorLoader, themeLoader: themeLoader, tileMapGenDialog: tileMapGenDialog, tileMapGenLoader: tileMapGenLoader, townsLoader: townsLoader, waypointsLoader: waypointsLoader, weatherLoader: weatherLoader, workspace: workspace});
    }
    function smokeMapBrush() {
        if (!generationLibraryLoader.ensureLoaded()) return false;
        generationLibraryLoader.item.startOnMap();
        return generationLibraryLoader.item.mapSessionActive && workspace.proceduralBrushSession!==null;
    }
    function smokeGenerationLibrary() {
        if (!generationLibraryLoader.ensureLoaded()) return false;
        return generationLibraryLoader.status === Loader.Ready && generationLibraryLoader.item !== null;
    }
    function smokeTileMapGen() {
        if (!tileMapGenLoader.ensureLoaded()) return false;
        return tileMapGenLoader.status === Loader.Ready && tileMapGenLoader.item !== null;
    }
    // Both modern themes share one layout. Only their palettes differ.
    readonly property bool fluentUi: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    readonly property bool windowsClassicUi: Backend.uiTheme.style === "windows-classic"
    readonly property bool githubUi: Backend.uiTheme.style !== "classic" && !windowsClassicUi && !fluentUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"
                                   || Backend.uiTheme.style === "gray-modern"
    readonly property bool modernGrayUi: Backend.uiTheme.style === "gray-modern"
    readonly property int fluentTitleHeight: tibiaUi ? 30 : 34
    readonly property int topBarHeight: fluentUi ? fluentTitleHeight + (tibiaUi ? 28 : 32) : (githubUi ? 56 : 45)
    readonly property int minimumPaletteWidth: modernGrayUi ? 330 : (githubUi ? 220 : 160)
    readonly property int maximumPaletteWidth: Math.floor(width * 0.5)

    // Render the native window without exposing an unfinished first frame.
    property bool editorPresented: false
    opacity: editorPresented ? 1 : 0
    Connections {
        target: app
        function onStartedChanged() {
            if (!app.started)
                root.editorPresented = false;
        }
    }
    onFrameSwapped: {
        if (app.started && !Backend.otbmReader.loading)
            editorPresented = true;
    }
    // Present the editor maximized when leaving the startup screen.
    visibility: app.started ? Window.Maximized : Window.Hidden
    width: 1000
    height: 680
    minimumWidth: 720
    minimumHeight: 480
    title: "Dewral Map Editor  -  " + (Backend.otbmReader.filePath !== "" ? Backend.fileTools.fileName(Backend.otbmReader.filePath) : "(no map)") + (Backend.otbmReader.dirty ? "  *" : "")

    flags: Qt.FramelessWindowHint | Qt.Window
    roundedCorners: !root.tibiaUi
    maximizeButton: maximizeWindowButton
    // An opaque native surface lets DWM apply the same window corners as
    // OTEditor's standard ApplicationWindow, including correct maximization.
    color: root.fluentUi && Qt.platform.os === "windows" ? FluentColors.c("background") : "transparent"

    // Custom caption contents cover the native frame's client pixels. Keep
    // the subtle OTEditor-style outline visible above those contents.
    Rectangle {
        anchors.fill: parent
        visible: root.fluentUi && !root.tibiaUi && root.visibility !== Window.Maximized
                 && root.visibility !== Window.FullScreen
        z: 1000
        color: "transparent"
        radius: root.tibiaUi ? 0 : Qt.platform.os === "windows" ? 8 : 6
        antialiasing: true
        border.width: 1
        border.color: "#505050"
    }

    onClosing: function (close) {
        if (app.appCloseAllowed) {
            close.accepted = true;
            return;
        }
        close.accepted = false;
        app.requestAppClose();
    }

    Rectangle {
        anchors.fill: parent
        visible: root.fluentUi
        color: FluentColors.c("background")
        Loader {
            anchors.fill: parent; active: root.tibiaUi
            sourceComponent: TibiaSkin.TibiaMenuBox { }
        }
    }

    DmeDialogBackground {
        anchors.fill: parent
        visible: !root.githubUi && !root.fluentUi

        frameSource: (Backend.uiTheme.tex + "popupwindow_tall.png")
        topBorder: 45
    }

    Rectangle {
        anchors.fill: parent
        visible: root.githubUi
        radius: root.visibility === Window.Maximized ? 0 : 6
        antialiasing: true
        color: root.grayUi ? "#151515" : "#0D1117"
        border {
            width: 1
            color: root.grayUi ? "#383838" : "#242D38"
        }
    }

    Item {
        id: titleBar
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: root.githubUi ? 1 : 0
            leftMargin: root.githubUi || root.fluentUi ? 0 : 6
            rightMargin: root.githubUi || root.fluentUi ? 0 : 6
        }

        height: root.topBarHeight

        Rectangle {
            anchors.fill: parent
            visible: root.githubUi
            radius: root.visibility === Window.Maximized ? 0 : 6
            antialiasing: true
            color: root.grayUi ? "#151515" : "#0D1117"

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 1
                color: root.grayUi ? "#383838" : "#212A35"
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: root.fluentUi && !root.tibiaUi
            color: FluentColors.c("titleBar")
            TibiaSkin.TibiaSkin { anchors.fill: parent }
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 32; color: FluentColors.c("titleBar"); TibiaSkin.TibiaSkin { anchors.fill: parent } }
        }
        Loader {
            x: 6; y: root.fluentTitleHeight; width: parent.width-12; height: 28
            active: root.tibiaUi
            sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "panel" }
        }
        Image {
            visible: root.fluentUi
            x: 10; y: (root.fluentTitleHeight - height) / 2; width: 16; height: width
            source: "qrc:/ui/github/app-icon.png"
        }
        Text {
            visible: root.fluentUi
            x: root.tibiaUi ? 90 : 38; y: 0; height: root.fluentTitleHeight
            width: Math.max(0, parent.width - (root.tibiaUi ? 180 : 190))
            text: root.title
            color: root.tibiaUi ? "#dededb" : FluentColors.c("buttonText")
            font.pixelSize: 13; font.family: FluentColors.family()
            horizontalAlignment: root.tibiaUi ? Text.AlignHCenter : Text.AlignLeft
            font.bold: root.tibiaUi
            verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        }
        Text {
            id: titleText

            visible: !root.githubUi && !root.fluentUi
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: root.fluentUi ? -15 : -5
            text: root.title
            color: root.windowsClassicUi ? "#202020" : "#c0c0c0"
            font.bold: !root.windowsClassicUi
            font.pixelSize: root.fluentUi ? 12 : 15
            elide: Text.ElideMiddle

            width: Math.max(0, Math.min(implicitWidth, parent.width - 2 * (menuBar.width + 24)))
        }

        Text {
            id: githubTitleText
            visible: root.githubUi && root.width >= 920
            anchors.centerIn: parent
            text: "Dewral Map Editor"
            color: root.grayUi ? "#F0F0F0" : "#E6EDF3"
            font {
                pixelSize: 14
                weight: Font.DemiBold
            }
            z: 2
        }

        Rectangle {
            id: dmeAppIcon
            visible: root.githubUi
            anchors {
                left: parent.left
                leftMargin: 20
                verticalCenter: parent.verticalCenter
            }
            width: 30
            height: 30
            radius: 7
            color: root.grayUi ? "#4A3A1F" : "#174D2B"
            border {
                width: 1
                color: root.grayUi ? "#C79A3B" : "#2EA043"
            }
            z: 5

            Image {
                anchors.centerIn: parent
                width: 20
                height: 20
                source: "qrc:/ui/github/app-icon.png"
                sourceSize: Qt.size(20, 20)
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
        }

        Row {
            id: winButtons
            height: root.fluentUi ? root.fluentTitleHeight : titleBar.height
            anchors {
                right: parent.right
                top: parent.top
            }
            anchors.rightMargin: root.tibiaUi ? 6 : 0
            spacing: root.tibiaUi ? 2 : 0
            z: 5

            GithubWindowButton {
                height: parent.height
                controlType: "minimize"
                onTriggered: root.showMinimized()
            }

            GithubWindowButton {
                id: maximizeWindowButton
                height: parent.height
                controlType: "maximize"
                maximized: root.visibility === Window.Maximized
                nativeHovered: root.maximizeHovered
                nativePressed: root.maximizePressed
                onTriggered: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
            }

            GithubWindowButton {
                height: parent.height
                controlType: "close"
                onTriggered: app.requestAppClose()
            }
        }

        MouseArea {
            id: titleDragArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: root.tibiaUi ? undefined : parent.bottom
            height: root.tibiaUi ? root.fluentTitleHeight : parent.height
            anchors {
                leftMargin: root.githubUi ? 460 : 0
                rightMargin: root.githubUi ? 138 : 138
            }
            z: 0
            onPressed: root.startSystemMove()

            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        }
    }

    AppController {
        id: app
        profiles: root.applicationSession.profiles
        settings: prefs
        mapView: workspace.mapView
        appWindow: root
        startupWindow: startupScreen
        versionFolderDialog: versionFolderDialogMain
        saveDialog: saveDialog
        closeTabConfirm: closeTabConfirm
        appCloseConfirm: appCloseConfirm
    }

    Component.onCompleted: {
        if (!prefs.githubLayoutV2Initialized) {
            prefs.paletteWidth = 390;
            prefs.githubLayoutV2Initialized = true;
        }
        Backend.workTimer.ensureLoaded();
        app.initialize();
    }

    // Optional diagnostic launch; does not overwrite the saved client paths.
    function profileOpen(mapPath, clientFolder, clientVersion) {
        var paths = Object.assign({}, app.clientPaths);
        paths[String(clientVersion)] = clientFolder;
        applicationSession.profiles.clientPaths = paths;
        // A repeat can start before the first frame dismisses the startup
        // dialog. Its loadingMap guard still belongs to the previous load.
        if (app.started)
            app.loadEverything(mapPath, String(clientVersion));
        else
            startupScreen.beginLoadMap(mapPath, String(clientVersion));
    }

    function profileReload(mapPath, clientFolder, clientVersion) {
        var index = Backend.docMgr.indexOfPath(mapPath);
        if (index >= 0)
            Backend.docMgr.closeDocument(index);
        profileOpen(mapPath, clientFolder, clientVersion);
    }

    function profilePalettePage(index) {
        prefs.paletteCollapsed = false;
        palette.profilePage(index);
    }

    Connections {
        target: Backend.workTimer
        function onReminderRequested(message) {
            app.showToast(message)
        }
    }

    Connections {
        target: workspace.mapView
        function onOperationWarning(message) { app.showToast(message); }
    }

    MainMenuBar {
        enabled: !root.clientBusy
        id: menuBar
        onActivityRequested: { if (!activityLoader.ensureLoaded()) return false; activityLoader.item.open(); }
        menuLeftInset: root.fluentUi ? 8 : root.githubUi ? 52 : 4
        menuVerticalOffset: root.fluentUi ? root.fluentTitleHeight / 2 : (root.githubUi ? 0 : -4)
        width: root.githubUi ? 520 : implicitWidth
        appController: app
        mapView: workspace.mapView
        mapRenderer: workspace.mapRenderer
        settings: prefs
        titleBarItem: titleBar
        startupWindow: startupScreen
        saveDialog: saveDialog
        newMapDialog: newMapDialog
        importMapDialog: importMapDialog
        exportMinimapDialog: exportMinimapDialog
        cleanupDialog: cleanupDialog
        selectionItemDialog: selItemDialog
        findItemDialog: findItemDialog
        searchResultsDialog: searchResultsDialog
        goToDialog: gotoPosDialog
        townsDialog: townsDialog
        waypointsDialog: waypointsDialog
        creatureManagerDialog: creatureManagerDialog
        weatherDialog: weatherDialog
        numberedZonesDialog: numberedZonesDialog
        mapPropertiesDialog: mapPropsDialog
        statsDialog: statsDialog
        brushEditorDialog: brushEditorDialog
        aiMapAssistantDialog: aiMapAssistantDialog
        luaGeneratorDialog: luaGeneratorDialog
        generationLibraryDialog: generationLibraryDialog
        tileMapGenDialog: tileMapGenDialog
        customMapGeneratorDialog: customMapGeneratorDialog
        terrainGeneratorDialog: terrainGeneratorDialog
        dungeonGeneratorDialog: dungeonGeneratorDialog
        groundClusterGeneratorDialog: groundClusterGeneratorDialog
        themeDialog: themeDialog
        borderizeConfirm: borderizeMapConfirm
        randomizeConfirm: randomizeMapConfirm
        aboutDialog: aboutDialog
    }

    AboutDialog {
        id: aboutDialog
    }

    Shortcut {
        sequence: Backend.hotkeys.activeBindings.save_as_alternative
        enabled: Backend.otbmReader.loaded
        onActivated: saveDialog.open()
    }

    Shortcut {
        sequence: Backend.hotkeys.activeBindings.zoom_in_alternative
        enabled: Backend.otbmReader.loaded
        onActivated: workspace.mapView.zoomSteps(1)
    }

    Shortcut {
        sequence: Backend.hotkeys.activeBindings.redo_alternative
        enabled: Backend.otbmReader.redoCount > 0
        onActivated: workspace.mapView.redo()
    }

    Shortcut {
        sequence: Backend.hotkeys.activeBindings.browse_field
        enabled: Backend.otbmReader.loaded && workspace.mapView.selectionCount === 1
        onActivated: {
            if (workspace.mapView.setContextFromSelection())
                browseFieldDialog.open();
        }
    }

    DmeConfirmDialog {
        id: borderizeMapConfirm
        title: "Borderize Map"
        message: "Recalculate auto-borders on the entire current floor?"
        onAccepted: workspace.mapView.borderizeMap()
    }
    DmeConfirmDialog {
        id: randomizeMapConfirm
        title: "Randomize Map"
        message: "Randomize ground variants on the entire current floor?"
        onAccepted: workspace.mapView.randomizeMap()
    }

    property string dockDragKind: "brush"
    property bool brushDragging: false
    property real brushDragX: 0
    property real brushDragY: 0
    function finishBrushDock(x, y) {
        const p = fluentFileBar.mapFromItem(root.contentItem, x, y);
        if (p.x >= 0 && p.x <= fluentFileBar.width && p.y >= 0 && p.y <= fluentFileBar.height)
            prefs[dockDragKind === "tools" ? "toolsDock" : "brushSizeDock"] = "topbar";
        else {
            const q = palette.mapFromItem(root.contentItem, x, y);
            if (q.x >= 0 && q.x <= palette.width && q.y >= 0 && q.y <= palette.height)
                prefs[dockDragKind === "tools" ? "toolsDock" : "brushSizeDock"] = "palette";
        }
        brushDragging = false;
    }
    Rectangle {
        x: root.brushDragX + 12; y: root.brushDragY + 12
        z: 1000; visible: root.brushDragging
        width: 104; height: 30; radius: 4
        color: "#242424"; border.color: "#4a9ec7"
        Text { anchors.centerIn: parent; text: root.dockDragKind === "tools" ? "Tools" : "Brush size"; color: "white"; font.pixelSize: 12 }
    }
    FluentFileBar {
        enabled: !root.clientBusy
        id: fluentFileBar
        anchors.leftMargin: root.tibiaUi ? 6 : 0
        anchors.rightMargin: root.tibiaUi ? 6 : 0
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: root.fluentUi
        height: visible ? implicitHeight : 0
        settings: prefs
        brushDragActive: root.brushDragging
        onToolsDockDragStarted: { root.dockDragKind = "tools"; root.brushDragging = true; }
        onToolsDoorsRequested: { prefs.paletteCollapsed = false; palette.selectKind("Door Palette"); }
        onBrushDockDragStarted: { root.dockDragKind = "brush"; root.brushDragging = true; }
        onBrushDockDragMoved: (x,y) => { root.brushDragX = x; root.brushDragY = y; }
        onBrushDockDragFinished: (x,y) => root.finishBrushDock(x,y)
        onBrushDockDragCanceled: root.brushDragging = false
        mapView: workspace.mapView
        onNewRequested: newMapDialog.open()
        onOpenRequested: startupScreen.openMapDialog()
        onSaveRequested: app.saveMap()
        onSaveAsRequested: saveDialog.open()
    }

    PaletteDockController {
        id: paletteDock
        settings: prefs
        availableWidth: root.contentItem.width - 12
        availableHeight: Math.max(0, root.contentItem.height - paletteTop - (root.fluentUi ? fluentStatusBar.height + 2 : 28))
        panelWidth: palette.width
    }
    readonly property real paletteTop: root.fluentUi ? fluentFileBar.y + fluentFileBar.height : titleBar.y + titleBar.height
    property real baseMapLeft: root.fluentUi
        ? (!prefs.paletteCollapsed && paletteDock.side === "left" ? palette.width + 12 : 6) : paletteSplitter.x + paletteSplitter.width
    property real baseMapRight: root.githubUi ? 1 : root.fluentUi && !prefs.paletteCollapsed && paletteDock.side === "right" ? palette.width + 12 : 8

    Behavior on baseMapLeft {
        enabled: root.fluentUi && !splitterArea.pressed && !floatingPaletteResize.pressed
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    Behavior on baseMapRight {
        enabled: root.fluentUi && !splitterArea.pressed && !floatingPaletteResize.pressed
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    readonly property bool previewDocked: prefs.showIngamePreviewWindow && (prefs.ingamePreviewDockSide==="left" || prefs.ingamePreviewDockSide==="right")
    readonly property real previewDockWidth: Math.min(Math.max(320,prefs.ingamePreviewDockWidth),Math.max(320,root.width-baseMapLeft-baseMapRight-240))
    readonly property real mapLeft: baseMapLeft+(previewDocked && prefs.ingamePreviewDockSide==="left" ? previewDockWidth+6 : 0)
    readonly property real mapRight: baseMapRight+(previewDocked && prefs.ingamePreviewDockSide==="right" ? previewDockWidth+6 : 0)
    Item {
        id:previewDockHost
        objectName:"previewDockHost"
        visible:root.previewDocked
        x:prefs.ingamePreviewDockSide==="left" ? root.baseMapLeft : root.width-root.baseMapRight-width
        y:root.paletteTop
        width:root.previewDockWidth
        height:Math.max(0,root.height-y-(root.fluentUi ? fluentStatusBar.height + 2 : 6))
        z:3
    }
    Rectangle {
        x:prefs.ingamePreviewDockSide==="left" ? previewDockHost.x+previewDockHost.width : previewDockHost.x-6
        y:previewDockHost.y; width:6; height:previewDockHost.height; z:4
        visible:root.previewDocked; color:resizePreview.containsMouse ? FluentColors.c("accent") : "transparent"
        MouseArea {
            id:resizePreview; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.SizeHorCursor
            property real startX:0; property real startWidth:0
            onPressed:mouse => { startX=mapToGlobal(mouse.x,mouse.y).x; startWidth=previewDockHost.width; }
            onPositionChanged:mouse => { if (pressed) {
                const delta=(mapToGlobal(mouse.x,mouse.y).x-startX)*(prefs.ingamePreviewDockSide==="left" ? 1 : -1);
                prefs.ingamePreviewDockWidth=Math.round(Math.max(320,Math.min(root.width-root.baseMapLeft-root.baseMapRight-240,startWidth+delta)));
            } }
        }
    }
    Rectangle {
        visible: root.fluentUi && paletteDock.dragging && paletteDock.target !== ""
        x: paletteDock.target === "right" ? root.contentItem.width - paletteDock.panelWidth - 6 : 6
        y: root.paletteTop
        width: paletteDock.panelWidth; height: paletteDock.availableHeight
        color: FluentColors.c("accent"); opacity: 0.2; z: 40
    }

    PalettePanel {
        id: palette
        floating: root.fluentUi && paletteDock.side === "floating"
        brushDockSettings: prefs
        onToolsDockDragStarted: { root.dockDragKind = "tools"; root.brushDragging = true; }
        onBrushDockDragStarted: { root.dockDragKind = "brush"; root.brushDragging = true; }
        onBrushDockDragMoved: (x,y) => { root.brushDragX = x; root.brushDragY = y; }
        onBrushDockDragFinished: (x,y) => root.finishBrushDock(x,y)
        onBrushDockDragCanceled: root.brushDragging = false
        x: root.fluentUi ? 6 + paletteDock.x : (root.githubUi ? 1 : 6)
        y: root.paletteTop + (root.fluentUi ? paletteDock.y : 0)
        height: root.fluentUi ? paletteDock.panelHeight : root.contentItem.height - y - (root.githubUi ? 1 : 6)
        z: root.fluentUi && paletteDock.side === "floating" ? 50 : 2
        positionLocked: prefs.palettePositionLocked
        onLockRequested: prefs.palettePositionLocked = !prefs.palettePositionLocked
        onDragStarted: (sx, sy) => paletteDock.begin(sx - 6, sy - root.paletteTop)
        onDragMoved: (sx, sy) => paletteDock.move(sx - 6, sy - root.paletteTop)
        onDragFinished: paletteDock.finish()
        onDragCanceled: paletteDock.cancel()
        width: {
            if (!root.fluentUi && prefs.paletteCollapsed)
                return 0;
            return Math.max(root.minimumPaletteWidth,
                            Math.min(root.tibiaUi ? prefs.tibiaPaletteWidth : root.fluentUi ? prefs.fluentPaletteWidth : prefs.paletteWidth, root.maximumPaletteWidth));
        }
        opacity: root.fluentUi && prefs.paletteCollapsed ? 0 : 1
        visible: !prefs.paletteCollapsed || (root.fluentUi && opacity > 0)
        enabled: !prefs.paletteCollapsed && !root.clientBusy
        Behavior on opacity {
            enabled: root.fluentUi
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
        transform: Translate {
            y: root.fluentUi && prefs.paletteCollapsed ? -8 : 0
            Behavior on y {
                enabled: root.fluentUi
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }
        readonly property bool animateDock: root.fluentUi && !paletteDock.dragging && !paletteDock.pending
            && !splitterArea.pressed && !floatingPaletteResize.pressed
        Behavior on x { enabled: palette.animateDock; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on y { enabled: palette.animateDock; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on height { enabled: palette.animateDock; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        app: app
        mapCtrl: workspace.mapView
        modernLayout: root.modernGrayUi
        onCollapseRequested: prefs.paletteCollapsed = true
        onRevealRequested: prefs.paletteCollapsed = false

        MouseArea {
            id: floatingPaletteResize
            visible: root.fluentUi && paletteDock.side === "floating" && !prefs.palettePositionLocked
            anchors.right: parent.right; anchors.bottom: parent.bottom
            width: 16; height: 16; cursorShape: Qt.SizeFDiagCursor
            property point startPointer
            property real startWidth
            property real startHeight
            onPressed: mouse => {
                startPointer = mapToItem(root.contentItem, mouse.x, mouse.y);
                startWidth = palette.width; startHeight = palette.height;
            }
            onPositionChanged: mouse => {
                if (!pressed) return;
                const p = mapToItem(root.contentItem, mouse.x, mouse.y);
                const nextWidth = Math.max(root.minimumPaletteWidth, Math.min(root.maximumPaletteWidth, startWidth + p.x - startPointer.x));
                if (root.tibiaUi) prefs.tibiaPaletteWidth = nextWidth;
                else prefs.fluentPaletteWidth = nextWidth;
                prefs.paletteFloatingHeight = Math.max(320, Math.min(paletteDock.availableHeight - paletteDock.y, startHeight + p.y - startPointer.y));
            }
        }
    }

    Item {
        id: paletteSplitter
        anchors.top: palette.top
        anchors.bottom: palette.bottom
        x: root.fluentUi && paletteDock.side === "right" ? palette.x - width : palette.x + palette.width
        width: root.githubUi ? 4 : 6
        z: 10
        visible: !prefs.paletteCollapsed && (!root.fluentUi || paletteDock.side !== "floating")

        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: parent.height
            color: splitterArea.containsMouse || splitterArea.pressed
                   ? (root.grayUi ? "#595959" : (root.githubUi ? "#3A4655" : "#4a90e2"))
                   : "transparent"
        }

        MouseArea {
            id: splitterArea
            enabled: !root.fluentUi || !prefs.palettePositionLocked
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property real startX: 0
            property int startWidth: 0

            onPressed: mouse => {
                startX = mapToItem(root.contentItem, mouse.x, 0).x;
                startWidth = palette.width;
            }
            onPositionChanged: mouse => {
                if (pressed) {
                    const value = Math.max(root.minimumPaletteWidth, Math.min(root.maximumPaletteWidth,
                        startWidth + (root.fluentUi && paletteDock.side === "right" ? -1 : 1) * (mapToItem(root.contentItem, mouse.x, 0).x - startX)));
                    if (root.tibiaUi) prefs.tibiaPaletteWidth = value;
                    else if (root.fluentUi) prefs.fluentPaletteWidth = value;
                    else prefs.paletteWidth = value;
                }
            }
        }
    }

    Item {
        id: paletteToggle

        width: 10
        height: 40
        anchors.verticalCenter: palette.verticalCenter

        x: prefs.paletteCollapsed ? 2 : (palette.x + palette.width)
        z: 20
        visible: !root.githubUi && !root.fluentUi

        Rectangle {
            anchors.fill: parent
            color: root.windowsClassicUi ? (toggleArea.containsMouse ? "#e5f1fb" : "#f0f0f0") : (toggleArea.containsMouse ? "#3a3a3a" : "#242424")
            border.color: root.windowsClassicUi ? "#7a7a7a" : "#4a4a4a"
            border.width: 1
        }
        Text {
            anchors.centerIn: parent
            text: prefs.paletteCollapsed ? ">" : "<"
            color: root.windowsClassicUi ? "#202020" : "#ccc"
            font.pixelSize: 10
        }
        MouseArea {
            id: toggleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: prefs.paletteCollapsed = !prefs.paletteCollapsed
        }
    }

    Item {
        id: githubPaletteExpand

        visible: root.githubUi && prefs.paletteCollapsed
        x: 6
        width: visible ? 30 : 0
        height: 54
        anchors.verticalCenter: parent.verticalCenter
        z: 30

        Rectangle {
            anchors.fill: parent
            radius: 7
            color: githubExpandArea.containsMouse ? (root.grayUi ? "#303030" : "#171E27")
                                                    : (root.grayUi ? "#242424" : "#111820")
            border {
                width: 1
                color: githubExpandArea.containsMouse ? (root.grayUi ? "#595959" : "#3A4655")
                                                       : (root.grayUi ? "#484848" : "#242D38")
            }
        }

        Text {
            anchors.centerIn: parent
            text: ">"
            color: githubExpandArea.containsMouse ? "#FFFFFF" : "#A7B1BC"
            font.pixelSize: 16
        }

        MouseArea {
            id: githubExpandArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: prefs.paletteCollapsed = false
        }

        GithubToolTip {
            targetItem: githubExpandArea
            targetHovered: githubExpandArea.containsMouse
            message: "Expand palette (Ctrl+B)"
        }
    }

    ImplicitHeightLoader {
        id: toolBar
        anchors.top: root.fluentUi ? fluentFileBar.bottom : titleBar.bottom
        anchors.topMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: root.mapLeft
        anchors.right: parent.right
        anchors.rightMargin: root.mapRight

        sourceComponent: root.fluentUi ? null : root.githubUi
                         ? githubToolBarComponent
                         : classicToolBarComponent
    }

    Component {
        id: classicToolBarComponent
        EditorToolBar {
            mapView: workspace.mapView
            settings: prefs
        }
    }

    Component {
        id: githubToolBarComponent
        GithubEditorToolBar {
            mapView: workspace.mapView
            settings: prefs
            modernLayout: root.modernGrayUi
        }
    }

    ProceduralBrushBar {
        id: proceduralTopBar
        anchors.top: toolBar.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.mapLeft
        anchors.right: parent.right
        anchors.rightMargin: root.mapRight
        session: generationLibraryLoader.active ? generationLibraryLoader.item : null
        mapView: workspace.mapView
        visible: app.started && Backend.otbmReader.loaded && (!root.tibiaUi || sessionActive)
        height: visible ? implicitHeight : 0
        onActivationRequested: generationLibraryDialog.open()
    }

    Loader {
        id: tabBar
        anchors.top: proceduralTopBar.bottom
        anchors.topMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: root.mapLeft
        anchors.right: parent.right
        anchors.rightMargin: root.mapRight
        height: app.started ? (root.tibiaUi ? 30 : root.fluentUi ? 36 : root.githubUi ? 42 : 22) : 0
        visible: app.started

        sourceComponent: root.githubUi
                         ? githubTabsComponent
                         : classicTabsComponent
    }

    Component {
        id: classicTabsComponent
        DocumentTabs {
            app: app
        }
    }

    Component {
        id: githubTabsComponent
        GithubDocumentTabs {
            app: app
            newMapDialog: newMapDialog
        }
    }

    DmeConfirmDialog {
        id: closeTabConfirm
        property int tabIndex: -1
        onAccepted: app.doCloseTab(tabIndex)
    }

    DmeDialog {
        id: appCloseConfirm
        title: "Unsaved maps"
        property string message: ""
        width: 460

        contentItem: Column {
            spacing: 12
            Text {
                width: appCloseConfirm.width - 24
                text: appCloseConfirm.message
                color: root.windowsClassicUi ? "#202020" : (root.grayUi ? "#E0E0E0" : (root.githubUi ? "#C9D1D9" : "#c0c0c0"))
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                DmeButton {
                    text: "Save all"
                    width: 130
                    variant: "primary"
                    onClicked: {
                        appCloseConfirm.close();
                        app.beginSaveAllAndClose();
                    }
                }
                DmeButton {
                    text: "Discard all"
                    width: 110
                    variant: "danger"
                    onClicked: {
                        appCloseConfirm.close();
                        app.finishAppClose();
                    }
                }
                DmeButton {
                    text: "Cancel"
                    width: 90
                    onClicked: appCloseConfirm.close()
                }
            }
        }
    }

    WorkTimerPopupHost { id: sharedWorkTimerPopup; activityHost: activityLoader }

    MapWorkspace {
        enabled: !root.clientBusy
        workTimerPopupHost: sharedWorkTimerPopup
        id: workspace
        proceduralBrushSession: generationLibraryLoader.active ? generationLibraryLoader.item : null
        previewDockHost:previewDockHost
        anchors.top: tabBar.bottom
        anchors.topMargin: 0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.mapLeft
        anchors.right: parent.right
        anchors.rightMargin: root.mapRight
        anchors.bottomMargin: root.fluentUi ? fluentStatusBar.height + 2 : root.githubUi ? 1 : 6
        visible: app.started
        app: app
        settings: prefs
        propertiesDialog: propsDialog
        browseFieldDialog: browseFieldDialog
        paletteNavigator: palette
    }

    DmeStatusBar {
        workTimerPopupHost: sharedWorkTimerPopup
        id: fluentStatusBar
        visible: root.fluentUi && app.started
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: root.tibiaUi ? 32 : 44
        mapView: workspace.mapView
        loaded: Backend.otbmReader.loaded
        showFps: prefs.showFps
        showWorkTimer: prefs.showWorkTimer
        fps: workspace.mapRenderer.fps
    }

    ModernMapToolRail {
        id: modernMapRail
        visible: root.modernGrayUi && app.started && Backend.otbmReader.loaded
        width: visible ? 58 : 0
        height: visible ? Math.min(570, workspace.height - 28) : 0
        anchors {
            right: parent.right
            rightMargin: 16
            verticalCenter: workspace.verticalCenter
        }
        z: 40
        mapView: workspace.mapView
        settings: prefs
    }

    FolderDialog {
        id: versionFolderDialogMain
        title: "Select client folder for " + app.profileLabel(app.pendingKey) + " (Tibia.dat / Tibia.spr / items.otb)"
        onAccepted: app.onVersionFolderPicked(selectedFolder)
    }

    FileDialog {
        id: saveDialog
        title: "Save map as .otbm"
        fileMode: FileDialog.SaveFile
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        defaultSuffix: "otbm"
        onAccepted: app.handleSaveAsAccepted(selectedFile)
        onRejected: app.handleSaveAsRejected()
    }

    QtObject {
        id: propsDialog
        property var contextOverride: null

        function open() {
            contextOverride = null;
            if (!propsDialogLoader.ensureLoaded()) return false;
            propsDialogLoader.item["open"]();
        }
        function openWithContext(itemContext) {
            contextOverride = itemContext;
            if (!propsDialogLoader.ensureLoaded()) return false;
            propsDialogLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: propsDialogLoader
        toolSource: Qt.resolvedUrl("tools/ItemPropertiesDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: browseFieldDialog
        function open() {
            if (!browseFieldLoader.ensureLoaded()) return false;
            const context = workspace.mapView.contextInfo();
            browseFieldLoader.item["targetX"] = context.x;
            browseFieldLoader.item["targetY"] = context.y;
            browseFieldLoader.item["targetZ"] = context.z;
            browseFieldLoader.item["refresh"]();
            browseFieldLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: browseFieldLoader
        toolSource: Qt.resolvedUrl("tools/BrowseFieldDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: containerDialog
        function open(path, title) {
            if (!containerLoader.ensureLoaded()) return false;
            containerLoader.item["openContainer"](path, title);
        }
    }
    LazyToolHost {
        id: containerLoader
        toolSource: Qt.resolvedUrl("tools/ContainerDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: brushEditorDialog
        function open() {
            if (!brushEditorLoader.ensureLoaded()) return false;
            brushEditorLoader.item["open"]();
        }
    }

    QtObject {
        id: aiMapAssistantDialog
        function open() {
            if (!aiMapAssistantLoader.ensureLoaded()) return false;
            aiMapAssistantLoader.item["open"]();
        }
    }
    QtObject {
        id: luaGeneratorDialog
        function open() {
            if (!luaGeneratorLoader.ensureLoaded()) return false;
            luaGeneratorLoader.item.open();
        }
    }
    QtObject {
        id: generationLibraryDialog
        function open() { if (!generationLibraryLoader.ensureLoaded()) return false; generationLibraryLoader.item.startOnMap(); }
    }
    LazyToolHost {
        id: generationLibraryLoader
        toolSource: Qt.resolvedUrl("tools/GenerationLibraryDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    QtObject {
        id: tileMapGenDialog
        function open() { if (!tileMapGenLoader.ensureLoaded()) return false; tileMapGenLoader.item.open(); }
    }
    LazyToolHost {
        id: tileMapGenLoader
        toolSource: Qt.resolvedUrl("tools/TileMapGenDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    QtObject {
        id: customMapGeneratorDialog
        function open() {
            if (!customMapGeneratorLoader.ensureLoaded()) return false;
            customMapGeneratorLoader.item.open();
        }
    }
    LazyToolHost {
        id: customMapGeneratorLoader
        toolSource: Qt.resolvedUrl("tools/CustomMapGeneratorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    LazyToolHost {
        id: luaGeneratorLoader
        toolSource: Qt.resolvedUrl("tools/LuaGeneratorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    LazyToolHost {
        id: aiMapAssistantLoader
        toolSource: Qt.resolvedUrl("tools/AiMapAssistantDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    QtObject {
        id: terrainGeneratorDialog
        function open() {
            if (!terrainGeneratorLoader.ensureLoaded()) return false;
            terrainGeneratorLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: terrainGeneratorLoader
        toolSource: Qt.resolvedUrl("tools/TerrainGeneratorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    QtObject {
        id: dungeonGeneratorDialog
        function open() {
            if (!dungeonGeneratorLoader.ensureLoaded()) return false;
            dungeonGeneratorLoader.item["open"]();
        }
    }
    QtObject {
        id: groundClusterGeneratorDialog
        function open() {
            if (!groundClusterGeneratorLoader.ensureLoaded()) return false;
            groundClusterGeneratorLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: groundClusterGeneratorLoader
        toolSource: Qt.resolvedUrl("tools/GroundClusterGeneratorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    LazyToolHost {
        id: dungeonGeneratorLoader
        toolSource: Qt.resolvedUrl("tools/DungeonGeneratorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    LazyToolHost {
        id: brushEditorLoader
        toolSource: Qt.resolvedUrl("tools/BrushEditorDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: gotoPosDialog
        function open() {
            if (!gotoPosLoader.ensureLoaded()) return false;
            gotoPosLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: gotoPosLoader
        toolSource: Qt.resolvedUrl("tools/GoToPositionDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: statsDialog
        function open() {
            if (!statsLoader.ensureLoaded()) return false;
            statsLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: statsLoader
        toolSource: Qt.resolvedUrl("tools/MapAnalyzerDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: weatherDialog
        function open() {
            if (!weatherLoader.ensureLoaded()) return false;
            weatherLoader.item.open();
        }
    }
    LazyToolHost {
        id: weatherLoader
        toolSource: Qt.resolvedUrl("tools/WeatherDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: numberedZonesDialog
        function open() {
            if (!numberedZonesLoader.ensureLoaded()) return false;
            numberedZonesLoader.item.open();
        }
    }
    LazyToolHost {
        id: numberedZonesLoader
        toolSource: Qt.resolvedUrl("tools/NumberedZonesDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: mapPropsDialog
        function open() {
            if (!mapPropsLoader.ensureLoaded()) return false;
            mapPropsLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: mapPropsLoader
        toolSource: Qt.resolvedUrl("tools/MapPropertiesDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: selItemDialog
        property string mode: "find"
        property string scope: "map"
        function open() {
            if (!selectionItemLoader.ensureLoaded()) return false;
            selectionItemLoader.item["mode"] = mode;
            selectionItemLoader.item["scope"] = scope;
            selectionItemLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: selectionItemLoader
        toolSource: Qt.resolvedUrl("tools/SelectionItemDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: findItemDialog
        function open(windowTitle) {
            if (!findItemLoader.ensureLoaded()) return false;
            findItemLoader.item.title = windowTitle || "Search for Item";
            findItemLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: findItemLoader
        toolSource: Qt.resolvedUrl("tools/FindItemDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: searchResultsDialog
        property string searchType: "everything"
        property bool selectionOnly: false

        function openSearch(type, selectedOnly) {
            searchType = type;
            selectionOnly = selectedOnly;
            if (!searchResultsLoader.ensureLoaded()) return false;
            searchResultsLoader.item["searchType"] = type;
            searchResultsLoader.item["selectionOnly"] = selectedOnly;
            searchResultsLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: searchResultsLoader
        toolSource: Qt.resolvedUrl("tools/SearchResultsDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: themeDialog
        function open() {
            if (!themeLoader.ensureLoaded()) return false;
            themeLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: themeLoader
        toolSource: Qt.resolvedUrl("tools/PreferencesDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    LazyToolHost {
        id: colorizeLoader
        toolSource: Qt.resolvedUrl("tools/UiColorizeDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }
    ColorHighlight { targetItem: titleBar; colorKeys: ["titleBar", "background", "text", "buttonText"] }
    ColorHighlight { targetItem: fluentStatusBar; colorKeys: ["cell", "separator", "muted", "placeholder", "accent"] }

    QtObject {
        id: newMapDialog
        function open() {
            if (!newMapLoader.ensureLoaded()) return false;
            newMapLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: newMapLoader
        toolSource: Qt.resolvedUrl("tools/NewMapDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: importMapDialog
        function open() {
            if (!importMapLoader.ensureLoaded()) return false;
            importMapLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: importMapLoader
        toolSource: Qt.resolvedUrl("tools/ImportMapDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: exportMinimapDialog
        function open() {
            if (!exportMinimapLoader.ensureLoaded()) return false;
            exportMinimapLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: exportMinimapLoader
        toolSource: Qt.resolvedUrl("tools/ExportMinimapDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: cleanupDialog
        function open() {
            if (!cleanupLoader.ensureLoaded()) return false;
            cleanupLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: cleanupLoader
        toolSource: Qt.resolvedUrl("tools/CleanupDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: townsDialog
        function open() {
            if (!townsLoader.ensureLoaded()) return false;
            townsLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: townsLoader
        toolSource: Qt.resolvedUrl("tools/TownsDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: waypointsDialog
        function open() {
            if (!waypointsLoader.ensureLoaded()) return false;
            waypointsLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: waypointsLoader
        toolSource: Qt.resolvedUrl("tools/WaypointsDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    QtObject {
        id: creatureManagerDialog
        function open() {
            if (!creatureManagerLoader.ensureLoaded()) return false;
            creatureManagerLoader.item["open"]();
        }
    }
    LazyToolHost {
        id: creatureManagerLoader
        toolSource: Qt.resolvedUrl("tools/CreatureManagerDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    readonly property var startupScreen: applicationSession.startupWindow

    LazyToolHost {
        id: activityLoader
        toolSource: Qt.resolvedUrl("tools/ActivityDialogTool.qml")
        initialProperties: ({environment: toolEnvironment()})
        onLoadFailed: message => app.showToast(message)
    }

    WindowResizeHandles {
        targetWindow: root
    }
}

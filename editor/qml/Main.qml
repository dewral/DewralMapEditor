pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Dialogs
import Tibia 1.0
import "dialogs"
import "style"
import "controllers"
import "components"
import "themes/github"

Window {
    id: root
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"
    readonly property int topBarHeight: githubUi ? 56 : 45

    visible: app.started
    width: 1000
    height: 680
    minimumWidth: 720
    minimumHeight: 480
    title: "Dewral Map Editor  -  " + (Backend.otbmReader.filePath !== "" ? Backend.fileTools.fileName(Backend.otbmReader.filePath) : "(no map)") + (Backend.otbmReader.dirty ? "  *" : "")

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    onClosing: function (close) {
        if (app.appCloseAllowed) {
            close.accepted = true;
            return;
        }
        close.accepted = false;
        app.requestAppClose();
    }

    DmeDialogBackground {
        anchors.fill: parent
        visible: !root.githubUi

        frameSource: (Backend.uiTheme.tex + "popupwindow_tall.png")
        topBorder: 45
    }

    Rectangle {
        anchors.fill: parent
        visible: root.githubUi
        radius: root.visibility === Window.Maximized ? 0 : 6
        antialiasing: true
        color: "#0D1117"
        border {
            width: 1
            color: "#242D38"
        }
    }

    Item {
        id: titleBar
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: root.githubUi ? 1 : 0
            leftMargin: root.githubUi ? 1 : 6
            rightMargin: root.githubUi ? 1 : 6
        }

        height: root.topBarHeight

        Rectangle {
            anchors.fill: parent
            visible: root.githubUi
            radius: root.visibility === Window.Maximized ? 0 : 6
            antialiasing: true
            color: "#0D1117"

            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 1
                color: "#212A35"
            }
        }

        Text {
            id: titleText

            visible: !root.githubUi
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -5
            text: root.title
            color: "#c0c0c0"
            font.bold: true
            font.pixelSize: 15
            elide: Text.ElideMiddle

            width: Math.max(0, Math.min(implicitWidth, parent.width - 2 * (menuBar.width + 24)))
        }

        Text {
            id: githubTitleText
            visible: root.githubUi
            anchors.centerIn: parent
            text: "Dewral Map Editor"
            color: "#E6EDF3"
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
            color: "#174D2B"
            border {
                width: 1
                color: "#2EA043"
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
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            spacing: 0
            z: 5

            GithubWindowButton {
                height: parent.height
                controlType: "minimize"
                onTriggered: root.showMinimized()
            }

            GithubWindowButton {
                height: parent.height
                controlType: "maximize"
                maximized: root.visibility === Window.Maximized
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
            anchors.fill: parent
            anchors {
                leftMargin: root.githubUi ? 460 : 0
                rightMargin: root.githubUi ? 138 : 138
            }
            z: 0
            onPressed: root.startSystemMove()

            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
        }
    }

    AppSettings {
        id: prefs
    }

    AppController {
        id: app
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
        app.initialize();
    }

    MainMenuBar {
        id: menuBar
        menuLeftInset: root.githubUi ? 52 : 4
        menuVerticalOffset: root.githubUi ? 0 : -4
        width: root.githubUi ? 450 : implicitWidth
        appController: app
        mapView: workspace.mapView
        mapGl: workspace.mapGl
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
        mapPropertiesDialog: mapPropsDialog
        statsDialog: statsDialog
        brushEditorDialog: brushEditorDialog
        aiMapAssistantDialog: aiMapAssistantDialog
        themeDialog: themeDialog
        borderizeConfirm: borderizeMapConfirm
        randomizeConfirm: randomizeMapConfirm
    }

    Shortcut {
        sequence: "Ctrl+Alt+S"
        enabled: Backend.otbmReader.loaded
        onActivated: saveDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+="
        enabled: Backend.otbmReader.loaded
        onActivated: workspace.mapView.zoomSteps(1)
    }

    Shortcut {
        sequence: "Ctrl+Y"
        enabled: Backend.otbmReader.redoCount > 0
        onActivated: workspace.mapView.redo()
    }

    Shortcut {
        sequence: "Ctrl+B"
        enabled: root.githubUi && app.started
        onActivated: prefs.paletteCollapsed = !prefs.paletteCollapsed
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

    PalettePanel {
        id: palette
        anchors.top: titleBar.bottom
        anchors.topMargin: 0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: root.githubUi ? 1 : 6
        anchors.bottomMargin: root.githubUi ? 1 : 6
        width: {
            if (prefs.paletteCollapsed)
                return 0;
            if (root.githubUi)
                return Math.max(260, Math.min(Math.max(prefs.paletteWidth, 260), 480));
            return Math.max(160, Math.min(prefs.paletteWidth, root.width - 300));
        }
        visible: !prefs.paletteCollapsed
        app: app
        mapCtrl: workspace.mapView
        onCollapseRequested: prefs.paletteCollapsed = true
        onRevealRequested: prefs.paletteCollapsed = false
    }

    Item {
        id: paletteSplitter
        anchors.top: palette.top
        anchors.bottom: palette.bottom
        anchors.left: palette.right
        width: root.githubUi ? 4 : 6
        z: 10
        visible: !prefs.paletteCollapsed

        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: parent.height
            color: splitterArea.containsMouse || splitterArea.pressed
                   ? (root.githubUi ? "#3A4655" : "#4a90e2")
                   : "transparent"
        }

        MouseArea {
            id: splitterArea
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
                if (pressed)
                    prefs.paletteWidth = Math.max(260, Math.min(480, startWidth + (mapToItem(root.contentItem, mouse.x, 0).x - startX)));
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
        visible: !root.githubUi

        Rectangle {
            anchors.fill: parent
            color: toggleArea.containsMouse ? "#3a3a3a" : "#242424"
            border.color: "#4a4a4a"
            border.width: 1
        }
        Text {
            anchors.centerIn: parent
            text: prefs.paletteCollapsed ? ">" : "<"
            color: "#ccc"
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
            color: githubExpandArea.containsMouse ? "#171E27" : "#111820"
            border {
                width: 1
                color: githubExpandArea.containsMouse ? "#3A4655" : "#242D38"
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

    Loader {
        id: toolBar
        anchors.top: titleBar.bottom
        anchors.topMargin: 0
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: root.githubUi ? 0 : 4
        anchors.rightMargin: root.githubUi ? 1 : 8

        sourceComponent: Backend.uiTheme.style === "github-dark"
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
        }
    }

    Loader {
        id: tabBar
        anchors.top: toolBar.bottom
        anchors.topMargin: 0
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: root.githubUi ? 0 : 4
        anchors.rightMargin: root.githubUi ? 1 : 8
        height: app.started ? (root.githubUi ? 42 : 22) : 0
        visible: app.started

        sourceComponent: Backend.uiTheme.style === "github-dark"
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
                color: root.githubUi ? "#C9D1D9" : "#c0c0c0"
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

    MapWorkspace {
        id: workspace
        anchors.top: tabBar.bottom
        anchors.topMargin: 0
        anchors.bottom: parent.bottom
        anchors.left: paletteSplitter.right
        anchors.right: parent.right
        anchors.leftMargin: root.githubUi ? 0 : 4
        anchors.rightMargin: root.githubUi ? 1 : 8
        anchors.bottomMargin: root.githubUi ? 1 : 6
        visible: app.started
        app: app
        settings: prefs
        propertiesDialog: propsDialog
        browseFieldDialog: browseFieldDialog
        paletteNavigator: palette
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
            propsDialogLoader.active = true;
            propsDialogLoader.item["open"]();
        }
        function openWithContext(itemContext) {
            contextOverride = itemContext;
            propsDialogLoader.active = true;
            propsDialogLoader.item["open"]();
        }
    }
    Loader {
        id: propsDialogLoader
        active: false
        sourceComponent: ItemPropertiesDialog {
            ctx: propsDialog.contextOverride || workspace.context
            mapCtrl: workspace.mapView
            containerDialog: containerDialog
            onClosed: Qt.callLater(() => {
                propsDialogLoader.active = false;
                propsDialog.contextOverride = null;
            })
        }
    }

    QtObject {
        id: browseFieldDialog
        function open() {
            browseFieldLoader.active = true;
            browseFieldLoader.item["open"]();
        }
    }
    Loader {
        id: browseFieldLoader
        active: false
        sourceComponent: BrowseFieldDialog {
            mapCtrl: workspace.mapView
            propertiesDialog: propsDialog
            paletteNavigator: palette
            onClosed: Qt.callLater(() => browseFieldLoader.active = false)
        }
    }

    QtObject {
        id: containerDialog
        function open(path, title) {
            containerLoader.active = true;
            containerLoader.item["openContainer"](path, title);
        }
    }
    Loader {
        id: containerLoader
        active: false
        sourceComponent: ContainerDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => containerLoader.active = false)
        }
    }

    QtObject {
        id: brushEditorDialog
        function open() {
            brushEditorLoader.active = true;
            brushEditorLoader.item["open"]();
        }
    }

    QtObject {
        id: aiMapAssistantDialog
        function open() {
            aiMapAssistantLoader.active = true;
            aiMapAssistantLoader.item["open"]();
        }
    }
    Loader {
        id: aiMapAssistantLoader
        active: false
        sourceComponent: AiMapAssistantDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => aiMapAssistantLoader.active = false)
        }
    }
    Loader {
        id: brushEditorLoader
        active: false
        sourceComponent: BrushEditorDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => brushEditorLoader.active = false)
        }
    }

    QtObject {
        id: gotoPosDialog
        function open() {
            gotoPosLoader.active = true;
            gotoPosLoader.item["open"]();
        }
    }
    Loader {
        id: gotoPosLoader
        active: false
        sourceComponent: GoToPositionDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => gotoPosLoader.active = false)
        }
    }

    QtObject {
        id: statsDialog
        function open() {
            statsLoader.active = true;
            statsLoader.item["open"]();
        }
    }
    Loader {
        id: statsLoader
        active: false
        sourceComponent: MapStatsDialog {
            onClosed: Qt.callLater(() => statsLoader.active = false)
        }
    }

    QtObject {
        id: mapPropsDialog
        function open() {
            mapPropsLoader.active = true;
            mapPropsLoader.item["open"]();
        }
    }
    Loader {
        id: mapPropsLoader
        active: false
        sourceComponent: MapPropertiesDialog {
            app: app
            onClosed: Qt.callLater(() => mapPropsLoader.active = false)
        }
    }

    QtObject {
        id: selItemDialog
        property string mode: "find"
        property string scope: "map"
        function open() {
            selectionItemLoader.active = true;
            selectionItemLoader.item["mode"] = mode;
            selectionItemLoader.item["scope"] = scope;
            selectionItemLoader.item["open"]();
        }
    }
    Loader {
        id: selectionItemLoader
        active: false
        sourceComponent: SelectionItemDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => selectionItemLoader.active = false)
        }
    }

    QtObject {
        id: findItemDialog
        function open() {
            findItemLoader.active = true;
            findItemLoader.item["open"]();
        }
    }
    Loader {
        id: findItemLoader
        active: false
        sourceComponent: FindItemDialog {
            paletteNavigator: palette
            onClosed: Qt.callLater(() => findItemLoader.active = false)
        }
    }

    QtObject {
        id: searchResultsDialog
        property string searchType: "everything"
        property bool selectionOnly: false

        function openSearch(type, selectedOnly) {
            searchType = type;
            selectionOnly = selectedOnly;
            searchResultsLoader.active = true;
            searchResultsLoader.item["searchType"] = type;
            searchResultsLoader.item["selectionOnly"] = selectedOnly;
            searchResultsLoader.item["open"]();
        }
    }
    Loader {
        id: searchResultsLoader
        active: false
        sourceComponent: SearchResultsDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => searchResultsLoader.active = false)
        }
    }

    QtObject {
        id: themeDialog
        function open() {
            themeLoader.active = true;
            themeLoader.item["open"]();
        }
    }
    Loader {
        id: themeLoader
        active: false
        sourceComponent: ThemeDialog {
            onClosed: Qt.callLater(() => themeLoader.active = false)
        }
    }

    QtObject {
        id: newMapDialog
        function open() {
            newMapLoader.active = true;
            newMapLoader.item["open"]();
        }
    }
    Loader {
        id: newMapLoader
        active: false
        sourceComponent: NewMapDialog {
            app: app
            onClosed: Qt.callLater(() => newMapLoader.active = false)
        }
    }

    QtObject {
        id: importMapDialog
        function open() {
            importMapLoader.active = true;
            importMapLoader.item["open"]();
        }
    }
    Loader {
        id: importMapLoader
        active: false
        sourceComponent: ImportMapDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => importMapLoader.active = false)
        }
    }

    QtObject {
        id: exportMinimapDialog
        function open() {
            exportMinimapLoader.active = true;
            exportMinimapLoader.item["open"]();
        }
    }
    Loader {
        id: exportMinimapLoader
        active: false
        sourceComponent: ExportMinimapDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => exportMinimapLoader.active = false)
        }
    }

    QtObject {
        id: cleanupDialog
        function open() {
            cleanupLoader.active = true;
            cleanupLoader.item["open"]();
        }
    }
    Loader {
        id: cleanupLoader
        active: false
        sourceComponent: CleanupDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => cleanupLoader.active = false)
        }
    }

    QtObject {
        id: townsDialog
        function open() {
            townsLoader.active = true;
            townsLoader.item["open"]();
        }
    }
    Loader {
        id: townsLoader
        active: false
        sourceComponent: TownsDialog {
            app: app
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => townsLoader.active = false)
        }
    }

    QtObject {
        id: waypointsDialog
        function open() {
            waypointsLoader.active = true;
            waypointsLoader.item["open"]();
        }
    }
    Loader {
        id: waypointsLoader
        active: false
        sourceComponent: WaypointsDialog {
            mapCtrl: workspace.mapView
            onClosed: Qt.callLater(() => waypointsLoader.active = false)
        }
    }

    QtObject {
        id: creatureManagerDialog
        function open() {
            creatureManagerLoader.active = true;
            creatureManagerLoader.item["open"]();
        }
    }
    Loader {
        id: creatureManagerLoader
        active: false
        sourceComponent: CreatureManagerDialog {
            onClosed: Qt.callLater(() => creatureManagerLoader.active = false)
        }
    }

    QtObject {
        id: startupScreen
        function ensureWindow() {
            startupLoader.active = true;
            return startupLoader.item;
        }
        function openMapDialog() {
            ensureWindow().openMapDialog();
        }
        function openVersionFolderDialog() {
            ensureWindow().openVersionFolderDialog();
        }
        function beginRecoveryLoad(path) {
            ensureWindow().beginRecoveryLoad(path);
        }
    }
    Loader {
        id: startupLoader
        active: !app.started
        sourceComponent: StartupWindow {
            app: app
            settings: prefs
        }
    }

    WindowResizeHandles {
        targetWindow: root
    }
}

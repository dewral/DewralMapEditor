import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtCore
import Tibia 1.0
import "style"
import "components"
import "themes/github"
import "themes/fluent/Colors.js" as FluentColors

Rectangle {
    id: paletteRoot

    required property var app

    required property var mapCtrl
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"
    readonly property bool windowsClassicUi: Backend.uiTheme.style === "windows-classic"
    readonly property bool githubUi: Backend.uiTheme.style !== "classic" && !windowsClassicUi && !fluentUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"
                                   || Backend.uiTheme.style === "gray-modern"
    property bool modernLayout: false
    property var brushDockSettings
    signal toolsDockDragStarted()
    signal brushDockDragStarted()
    signal brushDockDragMoved(real sceneX, real sceneY)
    signal brushDockDragFinished(real sceneX, real sceneY)
    signal brushDockDragCanceled()
    readonly property string currentKind: paletteCol.currentKind

    signal collapseRequested
    signal revealRequested
    property bool positionLocked: false
    signal lockRequested
    signal dragStarted(real sceneX, real sceneY)
    signal dragMoved(real sceneX, real sceneY)
    signal dragFinished
    signal dragCanceled

    function selectKind(kind) {
        paletteCol.selectKind(kind);
    }

    function profilePage(index) {
        clearPaletteSearch();
        paletteCol.selectKind("All Items");
        grid.positionViewAtIndex(Math.max(0, Math.min(index, grid.count - 1)), GridView.Beginning);
    }

    function clearPaletteSearch() {
        palSearch.text = "";
        paletteCol.pendingSearchText = "";
        searchDebounce.stop();
        paletteFilter.searchText = "";
    }

    function positionItem(serverId) {
        Qt.callLater(function () {
            Qt.callLater(function () {
                if (paletteCol.currentKind === "Doodad Palette") {
                    const doodadRow = doodadGrid.rowForServerId(serverId);
                    if (doodadRow >= 0) {
                        doodadGrid.currentIndex = doodadRow;
                        doodadGrid.positionViewAtIndex(doodadRow, GridView.Center);
                    }
                    return;
                }
                const row = grid.directAllItems
                        ? Backend.otbReader.rowForServerId(serverId)
                        : paletteFilter.rowForServerId(serverId);
                if (row >= 0) {
                    grid.currentIndex = row;
                    grid.positionViewAtIndex(row, GridView.Center);
                }
            });
        });
    }

    function showItemLocation(category, tileset, serverId) {
        revealRequested();
        clearPaletteSearch();
        if (tileset && tileset.length > 0) {
            paletteCol.selectCategoryTileset(category, tileset);
        } else {
            paletteCol.selectKind("All Items");
            paletteFilter.mode = "all";
        }
        positionItem(serverId);
    }

    function selectRaw(serverId) {
        const tileset = Backend.tilesetStore.tilesetForItem("raw", serverId);
        showItemLocation("raw", tileset, serverId);
        mapCtrl.brushServerId = serverId;
    }

    function selectBrush(serverId) {
        const brushName = mapCtrl.brushForServerId(serverId);
        if (brushName.length === 0) {
            selectRaw(serverId);
            return;
        }

        let category = Backend.brushStore.isDoodadBrushItem(serverId)
                ? "doodad" : "terrain";
        if (Backend.brushStore.isDoorItem(serverId)) {
            category = "door";
        } else if (Backend.brushStore.isCarpetBrushItem(serverId)
                   || Backend.brushStore.isTableBrushItem(serverId)) {
            category = Backend.tilesetStore.tilesetForItem("collection", serverId) !== ""
                    ? "collection" : "doodad";
        }
        let tileset = Backend.tilesetStore.tilesetForItem(category, serverId);
        let displayedServerId = serverId;

        if (tileset.length === 0) {
            const names = Backend.tilesetStore.namesFor(category);
            for (let i = 0; i < names.length && tileset.length === 0; ++i) {
                const ids = Backend.tilesetStore.itemsFor(category, names[i]);
                for (let j = 0; j < ids.length; ++j) {
                    if (mapCtrl.brushForServerId(ids[j]) === brushName) {
                        tileset = names[i];
                        displayedServerId = ids[j];
                        break;
                    }
                }
            }
        }

        if (tileset.length === 0) {
            showItemLocation("raw",
                             Backend.tilesetStore.tilesetForItem("raw", serverId),
                             serverId);
            mapCtrl.useGroundBrush(serverId);
            return;
        }

        showItemLocation(category, tileset, displayedServerId);
        mapCtrl.useGroundBrush(displayedServerId);
    }

    function selectCreature(name, isNpc) {
        if (!name || name.length === 0)
            return;
        revealRequested();
        clearPaletteSearch();
        paletteCol.selectKind("Creature Palette");
        paletteCol.creatureFilterType = isNpc ? "npc" : "monster";
        mapCtrl.selectCreatureBrush(name, isNpc === true);
        Qt.callLater(function () {
            creatureList.positionCreature(name, isNpc === true);
        });
    }

    function selectHouse(houseId) {
        if (houseId <= 0)
            return;
        revealRequested();
        clearPaletteSearch();
        paletteCol.selectKind("House Palette");
        Qt.callLater(function () { houseCol.selectHouseId(houseId); });
    }

    function selectPrefabPalette(name, prefabName) {
        revealRequested();
        clearPaletteSearch();
        paletteCol.selectCategoryTileset("doodad", name);
        Qt.callLater(function () {
            const index = paletteCol.subNames.indexOf(name);
            if (index >= 0)
                subCombo.currentIndex = index;
            if (prefabName && prefabName.length > 0)
                mapCtrl.useDoodadBrush(prefabName);
        });
    }

    width: 210
    color: grayUi ? "#1A1A1A" : (githubUi ? "#0F141B" : "transparent")
    radius: 0
    border {
        width: githubUi ? 1 : 0
        color: paletteRoot.grayUi ? "#3A3A3A" : "#242D38"
    }

    Rectangle {
        id: paletteDockEdge
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            rightMargin: 1
            topMargin: 8
            bottomMargin: 8
        }
        width: 2
        radius: 1
        visible: false
        color: "#7A7A7A"
    }

    DmePanel {
        anchors.fill: parent
        visible: !paletteRoot.githubUi && !paletteRoot.fluentUi
    }

    Rectangle {
        visible: paletteRoot.fluentUi
        x: 6; y: 6; width: parent.width - 12
        height: parent.height - 12
        color: FluentColors.c("surface"); border.color: FluentColors.c("border"); radius: 6
        Rectangle { x: 1; y: 30; width: parent.width - 2; height: 1; color: FluentColors.c("border") }
        Rectangle { x: 10; y: paletteCol.y + fluentTools.y - 14; width: parent.width - 20; height: 1; color: FluentColors.c("separator") }
        Rectangle { x: 10; y: paletteCol.y + brushSizeBox.y - 14; width: parent.width - 20; height: 1; color: FluentColors.c("separator") }
    }

    GithubActivityRail {
        id: modernActivityRail
        visible: paletteRoot.modernLayout
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
        }
        width: 76
        z: 20
        currentKind: paletteCol.currentKind
        paletteCollapsed: false
        onKindRequested: kind => paletteCol.selectKind(kind)
    }

    PaletteFilter {
        id: paletteFilter
        sourceModel: Backend.otbReader
        sprReader: Backend.sprReader
        brushStore: Backend.brushStore
        hideInvisibleSprites: paletteRoot.app.settings.hideInvisibleSprites
        hideNamedItems: paletteRoot.app.settings.hideNamedItems
    }

    Column {
        id: paletteCol
        anchors.fill: parent
        anchors.leftMargin: paletteRoot.modernLayout ? 88 : (paletteRoot.githubUi || paletteRoot.fluentUi ? 16 : 6)
        anchors.rightMargin: paletteRoot.githubUi || paletteRoot.fluentUi ? 16 : 6
        anchors.topMargin: paletteRoot.fluentUi ? 16 : paletteRoot.githubUi ? 8 : 6
        anchors.bottomMargin: paletteRoot.githubUi || paletteRoot.fluentUi ? 16 : 6
        spacing: paletteRoot.fluentUi ? 16 : paletteRoot.githubUi ? 10 : 4

        property var kinds: ["Recent", "Favorites", "All Items", "Terrain Palette", "Doodad Palette", "Collection Palette", "Door Palette", "Item Palette", "RAW Palette", "Creature Palette", "House Palette", "My Palettes"]
        property bool creatureMode: currentKind === "Creature Palette"
        property string creatureFilterType: "all"
        property bool houseMode: currentKind === "House Palette"
        property string currentKind: kindCombo.currentText
        property int displayedCount: creatureMode ? creatureList.count
                                                  : (houseMode ? houseCol.houses.length
                                                               : (currentKind === "Doodad Palette"
                                                                  ? doodadGrid.count : grid.count))

        property string currentCategory: {
            switch (currentKind) {
            case "Terrain Palette":
                return "terrain";
            case "Doodad Palette":
                return "doodad";
            case "Collection Palette":
                return "collection";
            case "Door Palette":
                return "door";
            case "Item Palette":
                return "item";
            case "RAW Palette":
                return "raw";
            default:
                return "";
            }
        }

        property var subNames: {
            const _r = Backend.tilesetStore.revision;
            if (currentKind === "All Items")
                return Backend.tilesetStore.namesFor("raw");
            if (currentCategory !== "")
                return Backend.tilesetStore.namesFor(currentCategory);
            if (currentKind === "My Palettes")
                return app.customPaletteNames;
            return [];
        }
        property bool quickCollection: currentKind === "Recent" || currentKind === "Favorites"
        property bool showSub: currentKind !== "All Items" && !quickCollection
                               && !creatureMode && !houseMode
        property string currentSubName: (subCombo.currentIndex >= 0 && subCombo.currentIndex < subNames.length) ? subNames[subCombo.currentIndex] : ""
        onCurrentSubNameChanged: {
            grid.clearSelection();
            doodadGrid.clearSelection();
        }

        property string currentCustomName: currentKind === "My Palettes" ? currentSubName : ""

        property bool canDeleteCurrentTileset: {
            const _r = Backend.tilesetStore.revision;
            return currentSubName !== "" && (currentKind === "My Palettes" || (currentCategory !== "" && Backend.tilesetStore.isCustomOnly(currentCategory, currentSubName)));
        }

        property var currentIds: {
            const _r = Backend.tilesetStore.revision;
            if (currentKind === "All Items")
                return null;
            if (currentKind === "Recent")
                return app.recentBrushIds;
            if (currentKind === "Favorites")
                return app.favoriteBrushIds;
            if (currentSubName === "")
                return [];
            if (currentKind === "My Palettes")
                return app.customPalettes[currentSubName] || [];
            return Backend.tilesetStore.itemsFor(currentCategory, currentSubName);
        }
        onCurrentIdsChanged: {
            if (currentIds === null) {
                paletteFilter.mode = "all";
                return;
            }
            if (quickCollection)
                paletteFilter.setOrderedIds(currentIds);
            else
                paletteFilter.setIds(currentIds);
        }

        property string pendingSearchText: ""
        function queueSearch(text) {
            pendingSearchText = text;
            searchDebounce.restart();
        }

        Timer {
            id: searchDebounce
            interval: 120
            repeat: false
            onTriggered: {
                if (paletteCol.currentKind === "All Items")
                    paletteFilter.mode = "all";
                paletteFilter.searchText = paletteCol.pendingSearchText;
            }
        }

        function selectCustomPalette(name) {
            kindCombo.currentIndex = kinds.indexOf("My Palettes");
            Qt.callLater(function () {
                var idx = app.customPaletteNames.indexOf(name);
                if (idx >= 0)
                    subCombo.currentIndex = idx;
            });
        }

        function selectKind(kind) {
            var idx = kinds.indexOf(kind);
            if (idx >= 0)
                kindCombo.currentIndex = idx;
        }

        function selectCategoryTileset(category, name) {
            const kindName = {
                terrain: "Terrain Palette",
                doodad: "Doodad Palette",
                collection: "Collection Palette",
                door: "Door Palette",
                item: "Item Palette",
                raw: "RAW Palette"
            }[category];
            kindCombo.currentIndex = kinds.indexOf(kindName);
            Qt.callLater(function () {
                var idx = paletteCol.subNames.indexOf(name);
                if (idx >= 0)
                    subCombo.currentIndex = idx;
            });
        }

        Column {
            id: githubControlsColumn
            visible: paletteRoot.githubUi
            width: parent.width
            height: visible ? implicitHeight : 0
            spacing: 10

            Row {
                id: githubCategoryRow
                width: parent.width
                height: paletteRoot.modernLayout ? 0 : 62
                visible: !paletteRoot.modernLayout
                spacing: 4
                readonly property int categoryCount: 6
                property real categoryWidth: Math.floor((width - spacing * (categoryCount - 1)) / categoryCount)

                Repeater {
                    model: [
                        { label: "RAW", icon: "items", kind: "RAW Palette" },
                        { label: "Items", icon: "items", kind: "Item Palette" },
                        { label: "Terrain", icon: "terrain", kind: "Terrain Palette" },
                        { label: "Doodads", icon: "doodads", kind: "Doodad Palette" },
                        { label: "Creatures", icon: "creatures", kind: "Creature Palette" },
                        { label: "Houses", icon: "houses", kind: "House Palette" }
                    ]

                    delegate: Item {
                        id: categoryTab

                        required property var modelData
                        readonly property bool active: modelData.kind === "RAW Palette"
                                                     ? (paletteCol.currentKind === "RAW Palette" || paletteCol.currentKind === "All Items")
                                                     : paletteCol.currentKind === modelData.kind

                        width: githubCategoryRow.categoryWidth
                        height: githubCategoryRow.height

                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: categoryTab.active
                                   ? (paletteRoot.grayUi ? "#4A3A1F" : "#174D2B")
                                   : (categoryTabArea.containsMouse ? (paletteRoot.grayUi ? "#303030" : "#151C24") : "transparent")
                            border {
                                width: 1
                                color: categoryTab.active ? (paletteRoot.grayUi ? "#C79A3B" : "#2EA043")
                                                          : (categoryTabArea.containsMouse ? (paletteRoot.grayUi ? "#505050" : "#2D3743") : "transparent")
                            }
                        }

                        Column {
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                            spacing: 6

                            GithubIcon {
                                width: 23
                                height: 23
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: categoryTab.modelData.icon
                                opacity: categoryTab.active ? 1 : 0.72
                            }

                            Text {
                                width: parent.width
                                text: categoryTab.modelData.label
                                color: categoryTab.active ? "#FFFFFF" : (paletteRoot.grayUi ? "#A0A0A0" : "#A7B1BC")
                                font {
                                    pixelSize: githubCategoryRow.width < 300 ? 9 : 11
                                    weight: categoryTab.active ? Font.DemiBold : Font.Normal
                                }
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: categoryTabArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: paletteCol.selectKind(categoryTab.modelData.kind)
                        }
                    }
                }

            }

            Row {
                width: parent.width
                height: 32
                spacing: 8

                Repeater {
                    model: [
                        { label: "Recent", kind: "Recent" },
                        { label: "Favorites", kind: "Favorites" }
                    ]
                    delegate: DmeButton {
                        required property var modelData
                        width: (githubControlsColumn.width - 8) / 2
                        height: 32
                        text: modelData.label
                        checked: paletteCol.currentKind === modelData.kind
                        onClicked: paletteCol.selectKind(modelData.kind)
                    }
                }
            }

            Row {
                width: parent.width
                height: 42
                spacing: 8

                TextField {
                    id: githubSearch
                    width: parent.width - filterButton.width - parent.spacing
                    height: parent.height
                    leftPadding: 38
                    rightPadding: 12
                    placeholderText: paletteCol.creatureMode
                                     ? "Search creatures..." : "Search items..."
                    placeholderTextColor: paletteRoot.grayUi ? "#8A8A8A" : "#768390"
                    color: paletteRoot.grayUi ? "#E8E8E8" : "#E6EDF3"
                    selectionColor: paletteRoot.grayUi ? "#C79A3B" : "#2EA043"
                    selectedTextColor: "#FFFFFF"
                    font.pixelSize: 12
                    background: Rectangle {
                        radius: 4
                        color: paletteRoot.grayUi ? "#242424" : "#0D1117"
                        border {
                            width: githubSearch.activeFocus ? 2 : 1
                            color: githubSearch.activeFocus ? (paletteRoot.grayUi ? "#C79A3B" : "#3A7D55")
                                                            : (paletteRoot.grayUi ? "#484848" : "#242D38")
                        }
                    }
                    onTextChanged: paletteCol.queueSearch(text)

                    GithubIcon {
                        anchors {
                            left: parent.left
                            leftMargin: 11
                            verticalCenter: parent.verticalCenter
                        }
                        width: 18
                        height: 18
                        name: "search"
                    }
                }

                Item {
                    id: filterButton
                    width: 42
                    height: 42

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: filterArea.containsMouse ? (paletteRoot.grayUi ? "#303030" : "#171E27")
                                                        : (paletteRoot.grayUi ? "#242424" : "#111820")
                        border.width: 1
                        border.color: filterArea.containsMouse ? (paletteRoot.grayUi ? "#595959" : "#3A4655")
                                                               : (paletteRoot.grayUi ? "#484848" : "#242D38")
                    }

                    GithubIcon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        name: "filter"
                    }

                    MouseArea {
                        id: filterArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: githubSubCombo.popup.open()
                    }

                    GithubToolTip {
                        targetItem: filterArea
                        targetHovered: filterArea.containsMouse
                        message: "Choose palette category"
                    }
                }
            }

            Item {
                width: parent.width
                height: 40

                GithubPaletteCombo {
                    id: githubSubCombo
                    anchors.fill: parent
                    model: {
                        if (paletteCol.currentKind === "RAW Palette" || paletteCol.currentKind === "All Items")
                            return ["All Items"].concat(paletteCol.subNames);
                        if (paletteCol.showSub)
                            return paletteCol.subNames;
                        if (paletteCol.creatureMode)
                            return ["All creatures", "Monsters", "NPCs"];
                        if (paletteCol.houseMode)
                            return ["All houses"];
                        return ["All categories"];
                    }
                    currentIndex: {
                        if (paletteCol.currentKind === "All Items")
                            return 0;
                        if (paletteCol.currentKind === "RAW Palette")
                            return subCombo.currentIndex + 1;
                        if (paletteCol.creatureMode)
                            return paletteCol.creatureFilterType === "monster" ? 1
                                   : (paletteCol.creatureFilterType === "npc" ? 2 : 0);
                        return paletteCol.showSub ? subCombo.currentIndex : 0;
                    }
                    enabled: paletteCol.currentKind === "All Items"
                             || paletteCol.showSub || paletteCol.creatureMode
                    onActivated: index => {
                        if (paletteCol.currentKind === "RAW Palette" || paletteCol.currentKind === "All Items") {
                            if (index === 0) {
                                kindCombo.currentIndex = paletteCol.kinds.indexOf("All Items");
                            } else {
                                kindCombo.currentIndex = paletteCol.kinds.indexOf("RAW Palette");
                                Qt.callLater(function () {
                                    subCombo.currentIndex = index - 1;
                                });
                            }
                        } else if (paletteCol.creatureMode) {
                            paletteCol.creatureFilterType = index === 1
                                    ? "monster" : (index === 2 ? "npc" : "all");
                        } else if (paletteCol.showSub) {
                            subCombo.currentIndex = index;
                        }
                    }
                }
            }

            Flow {
                width: parent.width
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                spacing: 12

                DmeCheckBox {
                    objectName: "githubHideInvisibleSprites"
                    text: "Hide invisible sprites"
                    checked: paletteRoot.app.settings.hideInvisibleSprites
                    onClicked: paletteRoot.app.settings.hideInvisibleSprites = !checked
                }

                DmeCheckBox {
                    objectName: "githubHideNamedItems"
                    text: "Hide Named Items"
                    checked: paletteRoot.app.settings.hideNamedItems
                    onClicked: paletteRoot.app.settings.hideNamedItems = !checked
                }
            }

            Text {
                width: parent.width
                text: (paletteCol.showSub && paletteCol.currentSubName !== ""
                       ? paletteCol.currentSubName
                       : paletteCol.currentKind)
                      + "  (" + paletteCol.displayedCount + ")"
                color: paletteRoot.grayUi ? "#929292" : "#8B949E"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Column {
            id: controlsColumn
            visible: !paletteRoot.githubUi
            width: parent.width
            height: visible ? implicitHeight : 0
            spacing: paletteRoot.fluentUi ? 8 : 4

            Item {
                visible: paletteRoot.fluentUi; width: parent.width; height: visible ? 22 : 0
                Text { text: "Palette"; color: FluentColors.c("heading"); font.family: "Segoe UI"; font.pixelSize: 12 }
                MouseArea {
                    anchors.fill: parent; anchors.rightMargin: 52
                    cursorShape: paletteRoot.positionLocked ? Qt.ArrowCursor : Qt.SizeAllCursor
                    onPressed: mouse => {
                        const p = mapToItem(null, mouse.x, mouse.y);
                        paletteRoot.dragStarted(p.x, p.y);
                    }
                    onPositionChanged: mouse => {
                        if (!pressed) return;
                        const p = mapToItem(null, mouse.x, mouse.y);
                        paletteRoot.dragMoved(p.x, p.y);
                    }
                    onReleased: paletteRoot.dragFinished()
                    onCanceled: paletteRoot.dragCanceled()
                    onDoubleClicked: paletteRoot.lockRequested()
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 24; text: paletteRoot.positionLocked ? "\uD83D\uDD12" : "\uD83D\uDD13"; color: FluentColors.c("text"); font.pixelSize: 12
                    MouseArea { id: lockArea; anchors.fill: parent; anchors.margins: -3; hoverEnabled: true; onClicked: paletteRoot.lockRequested() }
                    ToolTip.visible: lockArea.containsMouse
                    ToolTip.text: paletteRoot.positionLocked ? "Unlock palette position" : "Lock palette position"
                }
                Text {
                    anchors.right: parent.right; text: "\u00d7"
                    color: FluentColors.c("text"); font.pixelSize: 16
                    MouseArea {
                        id: closePaletteArea
                        anchors.fill: parent; anchors.margins: -3; hoverEnabled: true
                        onClicked: paletteRoot.collapseRequested()
                    }
                    ToolTip.visible: closePaletteArea.containsMouse
                    ToolTip.text: "Close palette (Ctrl+B)"
                }
            }

            DmeComboBox {
                id: kindCombo
                objectName: "paletteKindCombo"
                width: parent.width
                height: paletteRoot.fluentUi ? 29 : 23
                model: paletteCol.kinds
                currentIndex: paletteRoot.fluentUi ? paletteCol.kinds.indexOf("All Items") : paletteRoot.githubUi ? paletteCol.kinds.indexOf("Item Palette") : 0
            }

            Text {
                visible: paletteCol.showSub || paletteCol.creatureMode
                text: paletteCol.creatureMode ? "Type"
                      : (paletteCol.currentKind === "My Palettes" ? "Palette" : "Tileset")
                color: paletteRoot.fluentUi ? "#D8DADD" : paletteRoot.windowsClassicUi ? "#202020" : "#7fdc8f"
                font.pixelSize: 10
                font.bold: true
            }
            DmeComboBox {
                id: subCombo
                visible: paletteCol.showSub || paletteCol.creatureMode
                width: parent.width
                height: 23
                model: paletteCol.creatureMode
                       ? ["All creatures", "Monsters", "NPCs"] : paletteCol.subNames
                currentIndex: paletteCol.creatureMode
                              ? (paletteCol.creatureFilterType === "monster" ? 1
                                 : (paletteCol.creatureFilterType === "npc" ? 2 : 0))
                              : 0
                onActivated: index => {
                    if (paletteCol.creatureMode)
                        paletteCol.creatureFilterType = index === 1
                                ? "monster" : (index === 2 ? "npc" : "all");
                }
                onModelChanged: {
                    if (currentIndex >= model.length)
                        currentIndex = 0;
                }
            }

            Row {
                width: parent.width
                spacing: 4
                DmeTextField {
                    id: palSearch
                    width: paletteRoot.fluentUi ? parent.width - 36 : parent.width - 4
                    height: paletteRoot.fluentUi ? 29 : 22
                    placeholderText: paletteRoot.fluentUi ? "Search items or ID…" : "Search..."
                    onTextChanged: paletteCol.queueSearch(text)
                }
                ToolButton {
                    id: fluentFilterButton
                    objectName: "fluentFilterButton"
                    visible: paletteRoot.fluentUi
                    width: 32; height: 29
                    Accessible.name: "Palette filters"
                    onClicked: fluentFilterMenu.popup()
                    background: Rectangle {
                        radius: 4; border.color: FluentColors.c("border")
                        color: paletteRoot.app.settings.hideInvisibleSprites || paletteRoot.app.settings.hideNamedItems ? FluentColors.c("selected") : fluentFilterButton.down ? FluentColors.c("pressed") : fluentFilterButton.hovered ? FluentColors.c("hover") : FluentColors.c("button")
                    }
                    contentItem: Canvas {
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset(); ctx.strokeStyle = FluentColors.c("muted"); ctx.lineWidth = 1.5;
                            const x = width / 2, y = height / 2;
                            ctx.beginPath(); ctx.moveTo(x - 9, y - 8); ctx.lineTo(x + 9, y - 8);
                            ctx.lineTo(x + 3, y - 1); ctx.lineTo(x + 3, y + 8);
                            ctx.lineTo(x - 3, y + 5); ctx.lineTo(x - 3, y - 1); ctx.closePath(); ctx.stroke();
                        }
                    }
                    DmeMenu {
                        id: fluentFilterMenu
                        objectName: "fluentFilterMenu"
                        DmeMenuItem {
                            objectName: "fluentHideInvisibleSprites"
                            text: "Hide invisible sprites"; checkable: true
                            checked: paletteRoot.app.settings.hideInvisibleSprites
                            onTriggered: paletteRoot.app.settings.hideInvisibleSprites = checked
                        }
                        DmeMenuItem {
                            objectName: "fluentHideNamedItems"
                            text: "Hide named items"; checkable: true
                            checked: paletteRoot.app.settings.hideNamedItems
                            onTriggered: paletteRoot.app.settings.hideNamedItems = checked
                        }
                    }
                }
            }

            Flow {
                width: parent.width
                visible: !paletteRoot.fluentUi && !paletteCol.creatureMode && !paletteCol.houseMode
                spacing: paletteRoot.fluentUi ? 8 : 12

                DmeCheckBox {
                    objectName: "paletteHideInvisibleSprites"
                    width: paletteRoot.fluentUi ? parent.width : implicitWidth
                    text: "Hide invisible sprites"
                    checked: paletteRoot.app.settings.hideInvisibleSprites
                    onClicked: paletteRoot.app.settings.hideInvisibleSprites = !checked
                }

                DmeCheckBox {
                    objectName: "paletteHideNamedItems"
                    width: paletteRoot.fluentUi ? parent.width : implicitWidth
                    text: paletteRoot.fluentUi ? "Hide named items" : "Hide Named Items"
                    checked: paletteRoot.app.settings.hideNamedItems
                    onClicked: paletteRoot.app.settings.hideNamedItems = !checked
                }
            }

            Text {
                text: (paletteCol.showSub && paletteCol.currentSubName !== "" ? paletteCol.currentSubName : paletteCol.currentKind) + "  (" + paletteCol.displayedCount + ")"
                color: paletteRoot.fluentUi ? FluentColors.c("heading") : paletteRoot.windowsClassicUi ? "#202020" : "#ddd"
                font.pixelSize: 12
                font.bold: true
                font.family: paletteRoot.fluentUi ? "Segoe UI" : Qt.application.font.family
                elide: Text.ElideRight
                width: parent.width
            }
        }

        Item {
            width: parent.width
            height: parent.height - controlsColumn.height - githubControlsColumn.height - brushSizeBox.height - fluentTools.height - paletteCol.spacing * 3

            PaletteItemGrid {
                id: grid
                objectName: "paletteItemGrid"
                anchors.fill: parent
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                         && paletteCol.currentKind !== "Doodad Palette"
                app: paletteRoot.app
                mapCtrl: paletteRoot.mapCtrl
                filterModel: paletteFilter
                currentKind: paletteCol.currentKind
                githubUi: paletteRoot.githubUi
                onContextMenuRequested: serverId => {
                    palItemMenu.sid = serverId;
                    palItemMenu.serverIds = grid.selectedServerIds.slice();
                    palItemMenu.popup();
                }
            }

            CreaturePaletteView {
                id: creatureList
                objectName: "creaturePaletteView"
                anchors.fill: parent
                visible: paletteCol.creatureMode
                app: paletteRoot.app
                mapCtrl: paletteRoot.mapCtrl
                githubUi: paletteRoot.githubUi
                searchText: paletteFilter.searchText
                filterMode: paletteCol.creatureFilterType
            }

            HousePaletteView {
                id: houseCol
                anchors.fill: parent
                visible: paletteCol.houseMode
                mapCtrl: paletteRoot.mapCtrl
                githubUi: paletteRoot.githubUi
            }

            DoodadPaletteGrid {
                id: doodadGrid
                objectName: "doodadPaletteGrid"
                filterModel: paletteFilter
                anchors.fill: parent
                visible: paletteCol.currentKind === "Doodad Palette"
                app: paletteRoot.app
                mapCtrl: paletteRoot.mapCtrl
                itemIds: paletteCol.currentIds || []
                categoryName: paletteCol.currentSubName
                searchText: paletteFilter.searchText
                githubUi: paletteRoot.githubUi
                onContextMenuRequested: serverId => {
                    palItemMenu.sid = serverId;
                    palItemMenu.serverIds = doodadGrid.selectedServerIds.slice();
                    palItemMenu.popup();
                }
            }

        }

        FluentTools {
            id: fluentTools
            width: parent.width
            visible: paletteRoot.fluentUi && (!paletteRoot.brushDockSettings || paletteRoot.brushDockSettings.toolsDock !== "topbar")
            height: visible ? implicitHeight : 0
            onDockDragStarted: paletteRoot.toolsDockDragStarted()
            onDockDragMoved: (x,y) => paletteRoot.brushDockDragMoved(x,y)
            onDockDragFinished: (x,y) => paletteRoot.brushDockDragFinished(x,y)
            onDockDragCanceled: paletteRoot.brushDockDragCanceled()
            mapView: paletteRoot.mapCtrl
            onDoorsRequested: paletteRoot.selectKind("Door Palette")
        }

        PaletteBrushSizeSelector {
            id: brushSizeBox
            visible: !paletteRoot.fluentUi || !paletteRoot.brushDockSettings || paletteRoot.brushDockSettings.brushSizeDock !== "topbar"
            height: visible ? implicitHeight : 0
            onDockDragStarted: paletteRoot.brushDockDragStarted()
            onDockDragMoved: (x,y) => paletteRoot.brushDockDragMoved(x,y)
            onDockDragFinished: (x,y) => paletteRoot.brushDockDragFinished(x,y)
            onDockDragCanceled: paletteRoot.brushDockDragCanceled()
            width: parent.width
            mapCtrl: paletteRoot.mapCtrl
            githubUi: paletteRoot.githubUi
        }
    }

    Dialogs.FileDialog {
        id: spriteExportDialog
        property int serverId: 0
        title: "Export sprite as PNG"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["PNG images (*.png)"]
        defaultSuffix: "png"
        onAccepted: {
            const error = Backend.exportSprite(serverId, selectedFile);
            if (error !== "") {
                spriteExportError.text = error;
                spriteExportError.open();
            }
        }
    }

    Dialogs.MessageDialog {
        id: spriteExportError
        title: "Export sprite failed"
        buttons: Dialogs.MessageDialog.Ok
    }

    DmeMenu {
        id: palItemMenu
        objectName: "paletteItemMenu"
        property int sid: 0
        property var serverIds: []

        DmeMenuItem {
            text: palItemMenu.serverIds.length + " items selected"
            enabled: false
            visible: palItemMenu.serverIds.length > 1
            height: visible ? implicitHeight : 0
        }

        DmeMenuItem {
            text: "Copy Server ID"
            enabled: palItemMenu.sid > 0
            onTriggered: Backend.fileTools.setClipboard(String(palItemMenu.sid))
        }
        DmeMenuItem {
            text: "Export sprite..."
            enabled: palItemMenu.sid > 0
            onTriggered: {
                spriteExportDialog.serverId = palItemMenu.sid;
                spriteExportDialog.currentFolder = StandardPaths.writableLocation(StandardPaths.DownloadLocation);
                spriteExportDialog.selectedFile = Backend.spriteExportUrl(palItemMenu.sid);
                spriteExportDialog.open();
            }
        }
        MenuSeparator {}

        CategoryAddMenu {
            category: "terrain"
            label: "Terrain Palette"
        }
        CategoryAddMenu {
            category: "doodad"
            label: "Doodad Palette"
        }
        CategoryAddMenu {
            category: "item"
            label: "Item Palette"
        }
        CategoryAddMenu {
            category: "collection"
            label: "Collection Palette"
        }
        CategoryAddMenu {
            category: "door"
            label: "Door Palette"
        }
        CategoryAddMenu {
            category: "raw"
            label: "RAW Palette"
        }

        DmeMenu {
            id: addToMenu
            title: "My Palettes"
            Instantiator {
                model: app.customPaletteNames
                delegate: DmeMenuItem {
                    text: modelData
                    onTriggered: app.addItemsToPalette(modelData, palItemMenu.serverIds)
                }
                onObjectAdded: (index, object) => addToMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addToMenu.removeItem(object)
            }
            MenuSeparator {
                visible: app.customPaletteNames.length > 0
            }
            DmeMenuItem {
                text: "New palette..."
                onTriggered: {
                    newPaletteField.text = "";
                    newPaletteDialog.pendingServerIds = palItemMenu.serverIds.slice();
                    newPaletteDialog.targetCategory = "";
                    newPaletteDialog.open();
                }
            }
        }

        MenuSeparator {
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
        }
        DmeMenuItem {
            readonly property bool allFavorites: palItemMenu.serverIds.length > 0
                    && palItemMenu.serverIds.every(id => app.isFavoriteBrush(id))
            text: allFavorites
                  ? "Remove from Favorites" : "Add to Favorites"
            onTriggered: {
                const remove = allFavorites;
                for (const id of palItemMenu.serverIds) {
                    if (app.isFavoriteBrush(id) === remove)
                        app.toggleFavoriteBrush(id);
                }
            }
        }
        DmeMenuItem {
            text: "Clear Recent"
            visible: paletteCol.currentKind === "Recent"
            height: visible ? implicitHeight : 0
            onTriggered: app.clearRecentBrushes()
        }
        MenuSeparator {}
        DmeMenuItem {
            text: "Remove from \"" + (paletteCol.currentKind === "My Palettes" ? paletteCol.currentCustomName : paletteCol.currentSubName) + "\""
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
            height: visible ? implicitHeight : 0
            onTriggered: {
                for (const id of palItemMenu.serverIds) {
                    if (paletteCol.currentKind === "My Palettes")
                        app.removeItemFromPalette(paletteCol.currentCustomName, id);
                    else
                        Backend.tilesetStore.removeItem(paletteCol.currentCategory, paletteCol.currentSubName, id);
                }
            }
        }
    }

    component CategoryAddMenu: DmeMenu {
        id: catMenu
        required property string category
        required property string label
        readonly property int tilesetRevision: Backend.tilesetStore.revision
        objectName: "paletteAdd_" + category
        title: label
        Instantiator {

            model: {
                catMenu.tilesetRevision;
                return Backend.tilesetStore.namesFor(catMenu.category);
            }
            delegate: DmeMenuItem {
                text: modelData
                onTriggered: Backend.tilesetStore.addItems(catMenu.category, modelData, palItemMenu.serverIds)
            }
            onObjectAdded: (index, object) => catMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => catMenu.removeItem(object)
        }
        MenuSeparator {
            visible: {
                catMenu.tilesetRevision;
                return Backend.tilesetStore.namesFor(catMenu.category).length > 0;
            }
        }
        DmeMenuItem {
            text: "New tileset..."
            onTriggered: {
                newPaletteField.text = "";
                newPaletteDialog.pendingServerIds = palItemMenu.serverIds.slice();
                newPaletteDialog.targetCategory = catMenu.category;
                newPaletteDialog.open();
            }
        }
    }

    DmeDialog {
        id: newPaletteDialog
        objectName: "newPaletteDialog"
        property var pendingServerIds: []
        property string targetCategory: ""
        title: targetCategory === "" ? "New palette" : "New tileset"

        function commit() {
            var name = newPaletteField.text.trim();
            if (name === "")
                return;
            if (targetCategory === "") {
                if (app.addCustomPalette(name) && pendingServerIds.length > 0)
                    app.addItemsToPalette(name, pendingServerIds);
                paletteCol.selectCustomPalette(name);
            } else {
                if (Backend.tilesetStore.newTileset(targetCategory, name) && pendingServerIds.length > 0)
                    Backend.tilesetStore.addItems(targetCategory, name, pendingServerIds);
                paletteCol.selectCategoryTileset(targetCategory, name);
            }
            pendingServerIds = [];
            newPaletteDialog.close();
        }

        onOpened: {
            newPaletteField.text = "";
            newPaletteField.forceActiveFocus();
        }

        contentItem: Column {
            spacing: 10
            DmeTextField {
                id: newPaletteField
                objectName: "newPaletteName"
                width: 220
                placeholderText: newPaletteDialog.targetCategory === "" ? "Palette name" : "Tileset name"
                onAccepted: newPaletteDialog.commit()
            }
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                DmeButton {
                    text: "OK"
                    width: 90
                    onClicked: newPaletteDialog.commit()
                }
                DmeButton {
                    text: "Cancel"
                    width: 90
                    onClicked: newPaletteDialog.close()
                }
            }
        }
    }

    Connections {
        target: paletteRoot.mapCtrl
        function onBrushUsed(serverId) {
            paletteRoot.app.recordBrushUse(serverId);
        }
        function onBrushChanged() {
            if (paletteRoot.mapCtrl.brushServerId > 0) {
                if (paletteCol.currentKind === "Doodad Palette") {
                    const doodadRow = doodadGrid.rowForServerId(paletteRoot.mapCtrl.brushServerId);
                    if (doodadRow >= 0 && doodadGrid.currentIndex !== doodadRow) {
                        doodadGrid.currentIndex = doodadRow;
                        doodadGrid.positionViewAtIndex(doodadRow, GridView.Center);
                    }
                    return;
                }
                var row = grid.directAllItems
                        ? Backend.otbReader.rowForServerId(paletteRoot.mapCtrl.brushServerId)
                        : paletteFilter.rowForServerId(paletteRoot.mapCtrl.brushServerId);
                if (row >= 0 && grid.currentIndex !== row) {
                    grid.currentIndex = row;
                    grid.positionViewAtIndex(row, GridView.Center);
                }
            }
        }
    }
    ColorHighlight { targetItem: paletteRoot; colorKeys: ["surface", "border", "heading", "separator"] }
}

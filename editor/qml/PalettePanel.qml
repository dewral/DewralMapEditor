import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "style"

Rectangle {
    id: paletteRoot

    required property var app

    required property var mapCtrl
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"
    readonly property string currentKind: paletteCol.currentKind

    signal collapseRequested
    signal revealRequested

    function selectKind(kind) {
        paletteCol.selectKind(kind);
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

    function selectCreature(name) {
        if (!name || name.length === 0)
            return;
        revealRequested();
        clearPaletteSearch();
        paletteCol.selectKind("Creature Palette");
        mapCtrl.creatureBrush = name;
        Qt.callLater(function () {
            const row = Backend.creatureStore.rowForName(name);
            if (row >= 0) {
                creatureList.currentIndex = row;
                creatureList.positionViewAtIndex(row, GridView.Center);
            }
        });
    }

    width: 210
    color: githubUi ? "#0F141B" : "transparent"
    radius: 0
    border {
        width: githubUi ? 1 : 0
        color: "#242D38"
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

    TibiaPanel {
        anchors.fill: parent
        visible: !paletteRoot.githubUi
    }

    PaletteFilter {
        id: paletteFilter
        sourceModel: Backend.otbReader
    }

    Column {
        id: paletteCol
        anchors.fill: parent
        anchors.leftMargin: paletteRoot.githubUi ? 16 : 6
        anchors.rightMargin: paletteRoot.githubUi ? 16 : 6
        anchors.topMargin: paletteRoot.githubUi ? 8 : 6
        anchors.bottomMargin: paletteRoot.githubUi ? 16 : 6
        spacing: paletteRoot.githubUi ? 10 : 4

        property var kinds: ["All Items", "Terrain Palette", "Doodad Palette", "Collection Palette", "Door Palette", "Item Palette", "RAW Palette", "Creature Palette", "House Palette", "My Palettes"]
        property bool creatureMode: currentKind === "Creature Palette"
        property bool houseMode: currentKind === "House Palette"
        property string currentKind: kindCombo.currentText

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
                return Backend.tilesetStore.namesFor("item");
            if (currentCategory !== "")
                return Backend.tilesetStore.namesFor(currentCategory);
            if (currentKind === "My Palettes")
                return app.customPaletteNames;
            return [];
        }
        property bool showSub: currentKind !== "All Items" && !creatureMode && !houseMode
        property string currentSubName: (subCombo.currentIndex >= 0 && subCombo.currentIndex < subNames.length) ? subNames[subCombo.currentIndex] : ""

        property string currentCustomName: currentKind === "My Palettes" ? currentSubName : ""

        property bool canDeleteCurrentTileset: {
            const _r = Backend.tilesetStore.revision;
            return currentSubName !== "" && (currentKind === "My Palettes" || (currentCategory !== "" && Backend.tilesetStore.isCustomOnly(currentCategory, currentSubName)));
        }

        property var currentIds: {
            const _r = Backend.tilesetStore.revision;
            if (currentKind === "All Items")
                return null;
            if (currentSubName === "")
                return [];
            if (currentKind === "My Palettes")
                return app.customPalettes[currentSubName] || [];
            return Backend.tilesetStore.itemsFor(currentCategory, currentSubName);
        }
        onCurrentIdsChanged: {
            if (currentIds === null) {
                if (paletteFilter.searchText !== "")
                    paletteFilter.mode = "all";
                return;
            }
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
                height: 62
                spacing: 4
                property real categoryWidth: Math.floor((width - spacing * 4) / 5)

                Repeater {
                    model: [
                        { label: "Items", icon: "items", kind: "Item Palette" },
                        { label: "Terrain", icon: "terrain", kind: "Terrain Palette" },
                        { label: "Doodads", icon: "doodads", kind: "Doodad Palette" },
                        { label: "Creatures", icon: "creatures", kind: "Creature Palette" },
                        { label: "Houses", icon: "houses", kind: "House Palette" }
                    ]

                    delegate: Item {
                        id: categoryTab

                        required property var modelData
                        readonly property bool active: modelData.kind === "Item Palette"
                                                     ? (paletteCol.currentKind === "Item Palette" || paletteCol.currentKind === "All Items")
                                                     : paletteCol.currentKind === modelData.kind

                        width: githubCategoryRow.categoryWidth
                        height: githubCategoryRow.height

                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: categoryTab.active
                                   ? "#174D2B"
                                   : (categoryTabArea.containsMouse ? "#151C24" : "transparent")
                            border {
                                width: 1
                                color: categoryTab.active ? "#2EA043"
                                                          : (categoryTabArea.containsMouse ? "#2D3743" : "transparent")
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
                                color: categoryTab.active ? "#FFFFFF" : "#A7B1BC"
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
                height: 42
                spacing: 8

                TextField {
                    id: githubSearch
                    width: parent.width - filterButton.width - parent.spacing
                    height: parent.height
                    leftPadding: 38
                    rightPadding: 12
                    placeholderText: "Search items..."
                    placeholderTextColor: "#768390"
                    color: "#E6EDF3"
                    selectionColor: "#2EA043"
                    selectedTextColor: "#FFFFFF"
                    font.pixelSize: 12
                    background: Rectangle {
                        radius: 4
                        color: "#0D1117"
                        border {
                            width: githubSearch.activeFocus ? 2 : 1
                            color: githubSearch.activeFocus ? "#3A7D55" : "#242D38"
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
                        color: filterArea.containsMouse ? "#171E27" : "#111820"
                        border.width: 1
                        border.color: filterArea.containsMouse ? "#3A4655" : "#242D38"
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

                GithubCombo {
                    id: githubSubCombo
                    anchors.fill: parent
                    model: {
                        if (paletteCol.currentKind === "Item Palette" || paletteCol.currentKind === "All Items")
                            return ["All Items"].concat(paletteCol.subNames);
                        if (paletteCol.showSub)
                            return paletteCol.subNames;
                        if (paletteCol.creatureMode)
                            return ["All creatures"];
                        if (paletteCol.houseMode)
                            return ["All houses"];
                        return ["All categories"];
                    }
                    currentIndex: {
                        if (paletteCol.currentKind === "All Items")
                            return 0;
                        if (paletteCol.currentKind === "Item Palette")
                            return subCombo.currentIndex + 1;
                        return paletteCol.showSub ? subCombo.currentIndex : 0;
                    }
                    enabled: paletteCol.currentKind === "Item Palette" || paletteCol.currentKind === "All Items" || paletteCol.showSub
                    onActivated: index => {
                        if (paletteCol.currentKind === "Item Palette" || paletteCol.currentKind === "All Items") {
                            if (index === 0) {
                                kindCombo.currentIndex = paletteCol.kinds.indexOf("All Items");
                            } else {
                                kindCombo.currentIndex = paletteCol.kinds.indexOf("Item Palette");
                                Qt.callLater(function () {
                                    subCombo.currentIndex = index - 1;
                                });
                            }
                        } else if (paletteCol.showSub) {
                            subCombo.currentIndex = index;
                        }
                    }
                }
            }

            Text {
                width: parent.width
                text: (paletteCol.showSub && paletteCol.currentSubName !== ""
                       ? paletteCol.currentSubName
                       : paletteCol.currentKind)
                      + "  (" + (paletteCol.creatureMode
                                  ? Backend.creatureStore.count
                                  : (paletteCol.houseMode ? houseCol.houses.length : grid.count)) + ")"
                color: "#8B949E"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Column {
            id: controlsColumn
            visible: !paletteRoot.githubUi
            width: parent.width
            height: visible ? implicitHeight : 0
            spacing: 4

            Item {
                width: 0
                height: 0
            }

            TibiaComboBox {
                id: kindCombo
                width: parent.width
                height: 23
                model: paletteCol.kinds
                currentIndex: paletteRoot.githubUi ? paletteCol.kinds.indexOf("Item Palette") : 0
            }

            Text {
                visible: paletteCol.showSub
                text: paletteCol.currentKind === "My Palettes" ? "Palette" : "Tileset"
                color: "#7fdc8f"
                font.pixelSize: 10
                font.bold: true
            }
            TibiaComboBox {
                id: subCombo
                visible: paletteCol.showSub
                width: parent.width
                height: 23
                model: paletteCol.subNames
                currentIndex: 0
                onModelChanged: {
                    if (currentIndex >= model.length)
                        currentIndex = 0;
                }
            }

            TibiaTextField {
                id: palSearch
                width: parent.width - 4
                height: 22
                placeholderText: "Search..."
                onTextChanged: paletteCol.queueSearch(text)
            }

            Text {
                text: (paletteCol.showSub && paletteCol.currentSubName !== "" ? paletteCol.currentSubName : paletteCol.currentKind) + "  (" + (paletteCol.creatureMode ? Backend.creatureStore.count : (paletteCol.houseMode ? houseCol.houses.length : grid.count)) + ")"
                color: "#ddd"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                width: parent.width
            }
        }

        Item {
            width: parent.width
            height: parent.height - controlsColumn.height - githubControlsColumn.height - brushSizeBox.height - paletteCol.spacing * 3

            GridView {
                id: grid
                readonly property int githubGridGap: 8
                readonly property int githubPreferredCellWidth: Math.max(72, paletteRoot.app.iconSizePx + 14)
                readonly property int githubMaxNativeColumns: Math.max(1, Math.floor(width / 76))
                readonly property int githubColumns: Math.min(githubMaxNativeColumns,
                                                               Math.max(1, Math.floor((width + githubGridGap)
                                                                                      / (githubPreferredCellWidth + githubGridGap) + 0.4)))
                readonly property real githubCellHeight: paletteRoot.app.iconSizePx + 22
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - (paletteRoot.githubUi ? 4 : 14)
                clip: true
                cellWidth: paletteRoot.githubUi
                           ? Math.max(1, Math.floor(width / githubColumns))
                           : paletteRoot.app.iconSizePx
                cellHeight: paletteRoot.githubUi
                            ? githubCellHeight
                            : paletteRoot.app.iconSizePx

                onCellWidthChanged: grid.positionViewAtBeginning()
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                readonly property bool directAllItems:
                    Backend.otbReader.loaded
                    && paletteCol.currentKind === "All Items"
                    && paletteFilter.searchText === ""

                model: Backend.otbReader.loaded
                       ? (directAllItems ? Backend.otbReader : paletteFilter)
                       : (Backend.datReader.loaded ? Backend.datReader : Backend.sprReader)

                delegate: Rectangle {
                    width: grid.cellWidth - (paletteRoot.githubUi ? grid.githubGridGap : 2)
                    height: grid.cellHeight - (paletteRoot.githubUi ? grid.githubGridGap : 2)
                    clip: true
                    property bool isBrush: typeof serverId !== "undefined" && mapCtrl.brushServerId === serverId
                    radius: paletteRoot.githubUi ? 4 : 0
                    color: isBrush
                           ? (paletteRoot.githubUi ? "#163B2C" : "#2f6f4f")
                           : (paletteCell.containsMouse
                              ? (paletteRoot.githubUi ? "#161E27" : "#303030")
                              : (paletteRoot.githubUi ? "#0D1117" : "#252525"))
                    border.color: isBrush
                                  ? (paletteRoot.githubUi ? "#2EA043" : "#7fdc8f")
                                  : (paletteRoot.githubUi
                                     ? (paletteCell.containsMouse ? "#3A4655" : "#202A35")
                                     : "#3a3a3a")
                    border.width: isBrush ? 2 : 1

                    property string doodadPrev: (typeof serverId !== "undefined") ? mapCtrl.doodadPreviewSource(serverId) : ""

                    Image {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: paletteRoot.githubUi ? -4 : 0

                        readonly property int nativeW: parent.doodadPrev !== ""
                                                               ? Math.max(1, implicitWidth)
                                                               : Math.max(1, (typeof itemWidth !== "undefined" ? itemWidth : 1) * 32)
                        readonly property int nativeH: parent.doodadPrev !== ""
                                                               ? Math.max(1, implicitHeight)
                                                               : Math.max(1, (typeof itemHeight !== "undefined" ? itemHeight : 1) * 32)
                        readonly property real availableW: Math.max(1, parent.width - (paletteRoot.githubUi ? 12 : 6))
                        readonly property real availableH: Math.max(1, parent.height - (paletteRoot.githubUi ? 24 : 6))
                        readonly property real tileScale: (grid.cellWidth - (paletteRoot.githubUi ? 16 : 6)) / 64

                        readonly property real fitScale: Math.min(
                            paletteRoot.githubUi ? 1 : tileScale,
                            availableW / nativeW,
                            availableH / nativeH)
                        width: nativeW * fitScale
                        height: nativeH * fitScale
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: {
                            if (parent.doodadPrev !== "")
                                return parent.doodadPrev;
                            if (typeof spriteIds !== "undefined" && spriteIds.length > 0) {
                                return Backend.sprReader.itemImageSource(spriteIds, typeof itemWidth !== "undefined" ? itemWidth : 1, typeof itemHeight !== "undefined" ? itemHeight : 1, typeof layers !== "undefined" ? layers : 1);
                            } else if (typeof spriteImageSource !== "undefined") {
                                return spriteImageSource;
                            }
                            return "";
                        }
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: paletteRoot.githubUi ? parent.horizontalCenter : undefined
                        anchors.right: paletteRoot.githubUi ? undefined : parent.right
                        anchors.margins: paletteRoot.githubUi ? 4 : 2
                        font.pixelSize: paletteRoot.githubUi
                                        ? (paletteRoot.app.iconSizePx >= 88 ? 13 : 11)
                                        : 9
                        color: paletteRoot.githubUi ? "#7D8590" : "#777"
                        text: {
                            if (typeof serverId !== "undefined")
                                return serverId;
                            if (typeof itemId !== "undefined")
                                return itemId;
                            if (typeof spriteId !== "undefined")
                                return spriteId;
                            return "";
                        }
                    }

                    MouseArea {
                        id: paletteCell
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: !paletteRoot.githubUi && paletteCell.containsMouse && typeof itemName !== "undefined" && itemName.length > 0
                        ToolTip.text: (typeof itemName !== "undefined" ? itemName : "") + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")
                        ToolTip.delay: 550
                        GithubToolTip {
                            targetItem: paletteCell
                            targetHovered: paletteRoot.githubUi && paletteCell.containsMouse && typeof itemName !== "undefined" && itemName.length > 0
                            message: (typeof itemName !== "undefined" ? itemName : "") + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")
                        }

                        onClicked: mouse => {
                            if (typeof serverId === "undefined")
                                return;
                            if (mouse.button === Qt.RightButton) {
                                palItemMenu.sid = serverId;
                                palItemMenu.popup();
                            } else if (mapCtrl.brushServerId === serverId) {
                                mapCtrl.brushServerId = 0;
                            } else if (paletteCol.currentKind === "All Items" || paletteCol.currentKind === "RAW Palette") {
                                mapCtrl.brushServerId = serverId;
                            } else {
                                mapCtrl.useGroundBrush(serverId);
                            }
                        }
                    }
                }
            }

            Column {
                anchors.fill: parent
                visible: paletteCol.creatureMode
                spacing: 4

                onVisibleChanged: {
                    if (!visible) {
                        if (mapCtrl.creatureBrush !== "")
                            mapCtrl.creatureBrush = "";
                        if (mapCtrl.spawnBrush)
                            mapCtrl.spawnBrush = false;
                    }
                }

                TibiaButton {
                    text: "Spawn brush"
                    width: parent.width - 14
                    checked: mapCtrl.spawnBrush
                    onClicked: mapCtrl.spawnBrush = !mapCtrl.spawnBrush
                }
                Row {
                    id: spawntimeRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: stLabel
                        text: "Spawntime"
                        color: "#999"
                        font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawntimeRow.width - stLabel.width - spawntimeRow.spacing
                        from: 1
                        to: 86400
                        value: mapCtrl.creatureSpawntime
                        onValueModified: mapCtrl.creatureSpawntime = value
                    }
                }
                Row {
                    id: spawnRadiusRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: srLabel
                        text: "Spawn radius"
                        color: "#999"
                        font.pixelSize: 11
                        width: 80
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: spawnRadiusRow.width - srLabel.width - spawnRadiusRow.spacing
                        from: 1
                        to: 15
                        value: mapCtrl.spawnBrushRadius
                        onValueModified: mapCtrl.spawnBrushRadius = value
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 26 - 22 - 22 - parent.spacing * 3

                    GridView {
                        id: creatureList
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width - 14
                        clip: true

                        cellWidth: app.iconSizePx
                        cellHeight: app.iconSizePx + 14
                        onCellWidthChanged: creatureList.positionViewAtBeginning()
                        model: Backend.creatureStore
                        delegate: Rectangle {
                            required property string name
                            required property bool isNpc
                            required property int lookType
                            width: creatureList.cellWidth - 2
                            height: creatureList.cellHeight - 2
                            property bool isBrush: mapCtrl.creatureBrush === name
                            color: isBrush
                                   ? (paletteRoot.githubUi ? "#163B2C" : "#2f6f4f")
                                   : (paletteRoot.githubUi
                                      ? (cma.containsMouse ? "#161E27" : "#0D1117")
                                      : (cma.containsMouse ? "#3A3A3A" : "#2A2A2A"))
                            border.color: isBrush
                                          ? (paletteRoot.githubUi ? "#2EA043" : "#7fdc8f")
                                          : (paletteRoot.githubUi ? "#202A35" : "#3a3a3a")
                            border.width: isBrush ? 2 : 1

                            Column {
                                anchors.centerIn: parent
                                spacing: 1
                                Image {

                                    width: creatureList.cellWidth - 14
                                    height: creatureList.cellHeight - 18
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    smooth: false
                                    cache: false
                                    fillMode: Image.PreserveAspectFit
                                    source: {
                                        var p = Backend.datReader.outfitPreview(lookType);
                                        return (p.ids !== undefined && p.ids.length > 0) ? Backend.sprReader.itemImageSource(p.ids, p.width, p.height, 1) : "";
                                    }
                                }
                                Text {

                                    text: name
                                    color: paletteRoot.githubUi ? "#A7B1BC" : "#c0c0c0"
                                    font.pixelSize: 10
                                    width: creatureList.cellWidth - 8
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                id: cma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: !paletteRoot.githubUi && cma.containsMouse
                                ToolTip.delay: 550
                                ToolTip.text: name + (isNpc ? "  (NPC)" : "")
                                GithubToolTip {
                                    targetItem: cma
                                    targetHovered: paletteRoot.githubUi && cma.containsMouse
                                    message: name + (isNpc ? "  (NPC)" : "")
                                }

                                onClicked: mapCtrl.creatureBrush = isBrush ? "" : name
                            }
                        }
                    }
                    TibiaScrollBar {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        flickable: creatureList
                    }
                }
            }

            Column {
                id: houseCol
                anchors.fill: parent
                visible: paletteCol.houseMode
                spacing: 4

                property var allHouses: []
                property var towns: []
                property int selHouseId: -1

                readonly property var houses: {
                    var tid = townCombo.currentTownId;
                    if (tid <= 0)
                        return [];
                    return allHouses.filter(function (h) {
                        return h.townId === tid;
                    });
                }
                function refresh() {
                    allHouses = Backend.otbmReader.housesList();
                    towns = Backend.otbmReader.townsList();
                }
                Connections {
                    target: Backend.otbmReader
                    function onMapChanged() {
                        if (paletteCol.houseMode)
                            houseCol.refresh();
                    }
                    function onLoadedChanged() {
                        houseCol.refresh();
                        houseCol.selHouseId = -1;
                    }
                }

                Row {
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        text: "Town"
                        color: "#999"
                        font.pixelSize: 11
                        width: 40
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: townCombo
                        width: parent.width - 46
                        model: houseCol.towns.length > 0 ? houseCol.towns.map(function (t) {
                            return t.name;
                        }) : ["No Town"]
                        currentIndex: houseCol.towns.length > 0 ? 0 : -1
                        readonly property int currentTownId: (currentIndex >= 0 && currentIndex < houseCol.towns.length) ? houseCol.towns[currentIndex].id : -1

                        onModelChanged: {
                            if (currentIndex < 0 || currentIndex >= model.length)
                                currentIndex = houseCol.towns.length > 0 ? 0 : -1;
                        }

                        onActivated: houseCol.selHouseId = -1
                    }
                }
                onVisibleChanged: {
                    if (visible) {
                        refresh();
                    } else {
                        if (mapCtrl.houseBrush > 0)
                            mapCtrl.houseBrush = 0;
                        mapCtrl.houseExitMode = false;
                        selHouseId = -1;
                    }
                }

                Row {
                    width: parent.width - 14
                    spacing: 6
                    TibiaButton {
                        text: "Add house"
                        width: (parent.width - 6) / 2

                        enabled: townCombo.currentTownId > 0
                        onClicked: {
                            houseCol.selHouseId = Backend.otbmReader.addHouse(townCombo.currentTownId);
                            houseCol.refresh();
                        }
                    }
                    TibiaButton {
                        text: "Remove"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        onClicked: {
                            if (mapCtrl.houseBrush === houseCol.selHouseId)
                                mapCtrl.houseBrush = 0;
                            Backend.otbmReader.removeHouse(houseCol.selHouseId);
                            houseCol.selHouseId = -1;
                            houseCol.refresh();
                        }
                    }
                }
                Row {
                    width: parent.width - 14
                    spacing: 6
                    TibiaButton {
                        text: "Draw"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        checked: mapCtrl.houseBrush === houseCol.selHouseId && !mapCtrl.houseExitMode
                        onClicked: {
                            mapCtrl.houseExitMode = false;
                            mapCtrl.houseBrush = (mapCtrl.houseBrush === houseCol.selHouseId && !mapCtrl.houseExitMode) ? 0 : houseCol.selHouseId;
                        }
                    }
                    TibiaButton {
                        text: "Set exit"
                        width: (parent.width - 6) / 2
                        enabled: houseCol.selHouseId > 0
                        checked: mapCtrl.houseExitMode && mapCtrl.houseBrush === houseCol.selHouseId
                        onClicked: {
                            mapCtrl.houseBrush = houseCol.selHouseId;
                            mapCtrl.houseExitMode = !mapCtrl.houseExitMode;
                        }
                    }
                }

                TibiaTextField {
                    id: houseNameField
                    width: parent.width - 14
                    enabled: houseCol.selHouseId > 0
                    placeholderText: "House name"
                    onEditingFinished: {
                        if (houseCol.selHouseId > 0 && text !== "") {
                            Backend.otbmReader.setHouseName(houseCol.selHouseId, text);
                            houseCol.refresh();
                        }
                    }
                }
                Row {
                    id: rentRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: rentLabel
                        text: "Rent"
                        color: "#999"
                        font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaSpinBox {
                        width: rentRow.width - rentLabel.width - rentRow.spacing
                        from: 0
                        to: 100000000
                        enabled: houseCol.selHouseId > 0
                        value: {
                            for (var i = 0; i < houseCol.houses.length; ++i)
                                if (houseCol.houses[i].id === houseCol.selHouseId)
                                    return houseCol.houses[i].rent;
                            return 0;
                        }
                        onValueModified: {
                            if (houseCol.selHouseId > 0)
                                Backend.otbmReader.setHouseRent(houseCol.selHouseId, value);
                        }
                    }
                }

                Row {
                    id: houseTownRow
                    width: parent.width - 14
                    spacing: 6
                    Text {
                        id: houseTownLabel
                        text: "Town"
                        color: "#999"
                        font.pixelSize: 11
                        width: 50
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    TibiaComboBox {
                        id: houseTownCombo
                        width: houseTownRow.width - houseTownLabel.width - houseTownRow.spacing
                        enabled: houseCol.selHouseId > 0 && houseCol.towns.length > 0
                        model: houseCol.towns.length > 0 ? houseCol.towns.map(function (t) {
                            return t.name;
                        }) : ["No Town"]
                        onActivated: {
                            if (houseCol.selHouseId > 0 && currentIndex >= 0 && currentIndex < houseCol.towns.length) {
                                Backend.otbmReader.setHouseTownId(houseCol.selHouseId, houseCol.towns[currentIndex].id);
                                houseCol.refresh();
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: parent.height - 23 * 2 - 26 * 2 - 22 * 2 - parent.spacing * 6

                    ListView {
                        id: houseList
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width - 14
                        clip: true
                        model: houseCol.houses
                        delegate: Rectangle {
                            required property var modelData
                            width: houseList.width
                            height: 34
                            property bool isSel: houseCol.selHouseId === modelData.id
                            color: isSel
                                   ? (paletteRoot.githubUi ? "#163B2C" : "#2f6f4f")
                                   : (paletteRoot.githubUi
                                      ? (hma.containsMouse ? "#161E27" : "#0D1117")
                                      : (hma.containsMouse ? "#3A3A3A" : "#2A2A2A"))
                            border.color: isSel
                                          ? (paletteRoot.githubUi ? "#2EA043" : "#7fdc8f")
                                          : (paletteRoot.githubUi ? "#202A35" : "#3a3a3a")
                            border.width: 1

                            Column {
                                anchors {
                                    left: parent.left
                                    leftMargin: 6
                                    verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: modelData.name
                                    color: paletteRoot.githubUi ? "#A7B1BC" : "#c0c0c0"
                                    font.pixelSize: 12
                                    width: houseList.width - 12
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: "id " + modelData.id + "   " + modelData.size + " sqm   rent " + modelData.rent
                                    color: "#888"
                                    font.pixelSize: 10
                                }
                            }
                            MouseArea {
                                id: hma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    houseCol.selHouseId = modelData.id;
                                    houseNameField.text = modelData.name;

                                    for (var i = 0; i < houseCol.towns.length; ++i)
                                        if (houseCol.towns[i].id === modelData.townId) {
                                            houseTownCombo.currentIndex = i;
                                            break;
                                        }

                                    mapCtrl.houseExitMode = false;
                                    mapCtrl.houseBrush = modelData.id;
                                }
                                onDoubleClicked: {
                                    if (modelData.entryX > 0)
                                        mapCtrl.centerOnTile(modelData.entryX, modelData.entryY, modelData.entryZ);
                                }
                            }
                        }
                    }
                    TibiaScrollBar {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        flickable: houseList
                    }
                }
            }

            TibiaScrollBar {
                visible: !paletteCol.creatureMode && !paletteCol.houseMode
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                flickable: grid
            }
        }

        Column {
            id: brushSizeBox
            width: parent.width
            spacing: paletteRoot.githubUi ? 9 : 3

            Rectangle {
                visible: paletteRoot.githubUi
                width: parent.width
                height: visible ? 1 : 0
                color: "#242D38"
            }

            Text {
                text: "Brush size"
                color: paletteRoot.githubUi ? "#E6EDF3" : "#ddd"
                font.pixelSize: paletteRoot.githubUi ? 12 : 11
                font.bold: true
            }

            Flow {
                width: parent.width
                spacing: paletteRoot.githubUi ? 5 : 3

                Repeater {
                    model: ["square", "circle"]
                    delegate: BrushBtn {
                        required property string modelData
                        githubStyle: paletteRoot.githubUi
                        active: mapCtrl.brushShape === modelData
                        round: modelData === "circle"
                        iconSize: 14
                        onClicked: mapCtrl.brushShape = modelData
                    }
                }

                Item {
                    width: paletteRoot.githubUi ? 6 : 10
                    height: 26
                }

                Repeater {
                    model: [0, 1, 2, 4, 6, 8, 11]
                    delegate: BrushBtn {
                        required property int modelData
                        required property int index
                        githubStyle: paletteRoot.githubUi
                        active: mapCtrl.brushSize === modelData
                        round: mapCtrl.brushShape === "circle"
                        iconSize: 6 + index * 2
                        onClicked: mapCtrl.brushSize = modelData
                        ToolTip.visible: !paletteRoot.githubUi && hovered
                        ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                        GithubToolTip {
                            targetItem: hoverArea
                            targetHovered: hovered
                            message: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                        }
                    }
                }
            }
        }
    }

    component GithubCombo: ComboBox {
        id: combo

        height: 40
        leftPadding: 12
        rightPadding: 34
        font.pixelSize: 13

        contentItem: Text {
            leftPadding: combo.leftPadding
            rightPadding: combo.rightPadding
            text: combo.displayText
            color: combo.enabled ? "#E6EDF3" : "#768390"
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Text {
            x: combo.width - width - 13
            anchors.verticalCenter: parent.verticalCenter
            text: "\u2304"
            color: combo.enabled ? "#C9D1D9" : "#768390"
            font.pixelSize: 18
        }

        background: Rectangle {
            radius: 4
            color: combo.down ? "#171E27" : "#0D1117"
            border {
                width: combo.activeFocus ? 2 : 1
                color: combo.activeFocus ? "#3A7D55" : "#242D38"
            }
        }

        delegate: ItemDelegate {
            id: comboDelegate
            required property var modelData
            required property int index
            width: combo.width
            height: 34
            leftPadding: 12
            text: modelData
            highlighted: combo.highlightedIndex === comboDelegate.index
            contentItem: Text {
                text: comboDelegate.text
                color: "#E6EDF3"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                color: comboDelegate.highlighted ? "#1B2632" : "#10151C"
            }
        }

        popup: Popup {
            y: combo.height + 4
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
            padding: 4
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
            background: Rectangle {
                radius: 4
                color: "#10151C"
                border {
                    width: 1
                    color: "#2D3743"
                }
            }
        }
    }

    component BrushBtn: Item {
        id: bb
        property bool active: false
        property bool round: false
        property bool githubStyle: false
        property int iconSize: 14
        property alias hovered: bbMa.containsMouse
        property alias hoverArea: bbMa
        signal clicked
        width: bb.githubStyle ? 32 : 26
        height: bb.githubStyle ? 32 : 26

        BorderImage {
            anchors.fill: parent
            visible: !bb.githubStyle
            source: (Backend.uiTheme.tex + "panel_side.png")
            smooth: false
            border {
                left: 1
                right: 1
                top: 1
                bottom: 1
            }
            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat
        }

        Rectangle {
            anchors.fill: parent
            color: bb.active ? "#992f6f4f" : (bbMa.containsMouse ? "#28ffffff" : "transparent")
            border.width: bb.active ? 1 : 0
            border.color: "#7fdc8f"
        }

        Rectangle {
            anchors.fill: parent
            visible: bb.githubStyle
            radius: 5
            color: bbMa.containsMouse ? "#171E27" : "#111820"
                border {
                    width: bb.active ? 2 : 1
                color: bb.active ? "#2EA043" : "#242D38"
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: bb.iconSize
            height: bb.iconSize
            radius: bb.round ? width / 2 : 0
            color: bb.active ? "#3FB950" : "#7D8590"
            border.color: bb.active ? "#7EE787" : "#A7B1BC"
            border.width: 1
        }
        MouseArea {
            id: bbMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: bb.clicked()
        }
    }

    TibiaMenu {
        id: palItemMenu
        property int sid: 0

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

        TibiaMenu {
            id: addToMenu
            title: "My Palettes"
            Instantiator {
                model: app.customPaletteNames
                delegate: TibiaMenuItem {
                    text: modelData
                    onTriggered: app.addItemToPalette(modelData, palItemMenu.sid)
                }
                onObjectAdded: (index, object) => addToMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addToMenu.removeItem(object)
            }
            MenuSeparator {
                visible: app.customPaletteNames.length > 0
            }
            TibiaMenuItem {
                text: "New palette..."
                onTriggered: {
                    newPaletteField.text = "";
                    newPaletteDialog.pendingSid = palItemMenu.sid;
                    newPaletteDialog.targetCategory = "";
                    newPaletteDialog.open();
                }
            }
        }

        MenuSeparator {
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
        }
        TibiaMenuItem {
            text: "Remove from \"" + (paletteCol.currentKind === "My Palettes" ? paletteCol.currentCustomName : paletteCol.currentSubName) + "\""
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
            height: visible ? implicitHeight : 0
            onTriggered: {
                if (paletteCol.currentKind === "My Palettes")
                    app.removeItemFromPalette(paletteCol.currentCustomName, palItemMenu.sid);
                else
                    Backend.tilesetStore.removeItem(paletteCol.currentCategory, paletteCol.currentSubName, palItemMenu.sid);
            }
        }
    }

    component CategoryAddMenu: TibiaMenu {
        id: catMenu
        required property string category
        required property string label
        readonly property int tilesetRevision: Backend.tilesetStore.revision
        title: label
        Instantiator {

            model: {
                catMenu.tilesetRevision;
                return Backend.tilesetStore.namesFor(catMenu.category);
            }
            delegate: TibiaMenuItem {
                text: modelData
                onTriggered: Backend.tilesetStore.addItem(catMenu.category, modelData, palItemMenu.sid)
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
        TibiaMenuItem {
            text: "New tileset..."
            onTriggered: {
                newPaletteField.text = "";
                newPaletteDialog.pendingSid = palItemMenu.sid;
                newPaletteDialog.targetCategory = catMenu.category;
                newPaletteDialog.open();
            }
        }
    }

    TibiaDialog {
        id: newPaletteDialog
        property int pendingSid: 0
        property string targetCategory: ""
        title: targetCategory === "" ? "New palette" : "New tileset"

        function commit() {
            var name = newPaletteField.text.trim();
            if (name === "")
                return;
            if (targetCategory === "") {
                if (app.addCustomPalette(name) && pendingSid > 0)
                    app.addItemToPalette(name, pendingSid);
                pendingSid = 0;
                paletteCol.selectCustomPalette(name);
            } else {
                if (Backend.tilesetStore.newTileset(targetCategory, name) && pendingSid > 0)
                    Backend.tilesetStore.addItem(targetCategory, name, pendingSid);
                pendingSid = 0;
                paletteCol.selectCategoryTileset(targetCategory, name);
            }
            newPaletteDialog.close();
        }

        onOpened: {
            newPaletteField.text = "";
            newPaletteField.forceActiveFocus();
        }

        contentItem: Column {
            spacing: 10
            TibiaTextField {
                id: newPaletteField
                width: 220
                placeholderText: newPaletteDialog.targetCategory === "" ? "Palette name" : "Tileset name"
                onAccepted: newPaletteDialog.commit()
            }
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                TibiaButton {
                    text: "OK"
                    width: 90
                    onClicked: newPaletteDialog.commit()
                }
                TibiaButton {
                    text: "Cancel"
                    width: 90
                    onClicked: newPaletteDialog.close()
                }
            }
        }
    }

    Connections {
        target: paletteRoot.mapCtrl
        function onBrushChanged() {
            if (paletteRoot.mapCtrl.brushServerId > 0) {
                var row = grid.directAllItems
                        ? Backend.otbReader.rowForServerId(paletteRoot.mapCtrl.brushServerId)
                        : paletteFilter.rowForServerId(paletteRoot.mapCtrl.brushServerId);
                if (row >= 0)
                    grid.positionViewAtIndex(row, GridView.Center);
            }
        }
    }
}

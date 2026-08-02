import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "style"
import "components"

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

    function selectCreature(name) {
        if (!name || name.length === 0)
            return;
        revealRequested();
        clearPaletteSearch();
        paletteCol.selectKind("Creature Palette");
        mapCtrl.creatureBrush = name;
        Qt.callLater(function () {
            creatureList.positionCreature(name);
        });
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

    DmePanel {
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
        property int displayedCount: creatureMode ? Backend.creatureStore.count
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

                GithubPaletteCombo {
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
                      + "  (" + paletteCol.displayedCount + ")"
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

            DmeComboBox {
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
            DmeComboBox {
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

            DmeTextField {
                id: palSearch
                width: parent.width - 4
                height: 22
                placeholderText: "Search..."
                onTextChanged: paletteCol.queueSearch(text)
            }

            Text {
                text: (paletteCol.showSub && paletteCol.currentSubName !== "" ? paletteCol.currentSubName : paletteCol.currentKind) + "  (" + paletteCol.displayedCount + ")"
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

            PaletteItemGrid {
                id: grid
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
                    palItemMenu.popup();
                }
            }

            CreaturePaletteView {
                id: creatureList
                anchors.fill: parent
                visible: paletteCol.creatureMode
                app: paletteRoot.app
                mapCtrl: paletteRoot.mapCtrl
                githubUi: paletteRoot.githubUi
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
                    palItemMenu.popup();
                }
            }

        }

        PaletteBrushSizeSelector {
            id: brushSizeBox
            width: parent.width
            mapCtrl: paletteRoot.mapCtrl
            githubUi: paletteRoot.githubUi
        }
    }

    DmeMenu {
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

        DmeMenu {
            id: addToMenu
            title: "My Palettes"
            Instantiator {
                model: app.customPaletteNames
                delegate: DmeMenuItem {
                    text: modelData
                    onTriggered: app.addItemToPalette(modelData, palItemMenu.sid)
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
                    newPaletteDialog.pendingSid = palItemMenu.sid;
                    newPaletteDialog.targetCategory = "";
                    newPaletteDialog.open();
                }
            }
        }

        MenuSeparator {
            visible: paletteCol.showSub && paletteCol.currentSubName !== ""
        }
        DmeMenuItem {
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

    component CategoryAddMenu: DmeMenu {
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
            delegate: DmeMenuItem {
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
        DmeMenuItem {
            text: "New tileset..."
            onTriggered: {
                newPaletteField.text = "";
                newPaletteDialog.pendingSid = palItemMenu.sid;
                newPaletteDialog.targetCategory = catMenu.category;
                newPaletteDialog.open();
            }
        }
    }

    DmeDialog {
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
            DmeTextField {
                id: newPaletteField
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
        function onBrushChanged() {
            if (paletteRoot.mapCtrl.brushServerId > 0) {
                if (paletteCol.currentKind === "Doodad Palette") {
                    const doodadRow = doodadGrid.rowForServerId(paletteRoot.mapCtrl.brushServerId);
                    if (doodadRow >= 0)
                        doodadGrid.positionViewAtIndex(doodadRow, GridView.Center);
                    return;
                }
                var row = grid.directAllItems
                        ? Backend.otbReader.rowForServerId(paletteRoot.mapCtrl.brushServerId)
                        : paletteFilter.rowForServerId(paletteRoot.mapCtrl.brushServerId);
                if (row >= 0)
                    grid.positionViewAtIndex(row, GridView.Center);
            }
        }
    }
}

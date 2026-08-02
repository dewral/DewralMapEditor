import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: root
    property var mapCtrl: null

    title: "Brush Editor"

    property string tab: "ground"
    property string curGround: ""
    property string curWall: ""

    property var borderSets: ({
            "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        })
    property string borderTarget: ""

    readonly property var borderIds: borderSets[borderTarget] || [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    property var wallIds: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    function setBorderTile(bt, sid) {
        var sets = JSON.parse(JSON.stringify(borderSets));
        if (!sets[borderTarget])
            sets[borderTarget] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        sets[borderTarget][bt] = sid;
        borderSets = sets;
    }

    function borderTargetKeys() {
        var keys = ["", "*"];
        var names = Backend.brushStore.groundBrushNames();
        for (var i = 0; i < names.length; ++i)
            if (names[i] !== groundNameField.text.trim())
                keys.push(names[i]);
        return keys;
    }
    function borderTargetLabel(key) {
        if (key === "")
            return "Empty (no neighbor)";
        if (key === "*")
            return "Any other brush";
        return key;
    }

    function iconSrc(id) {
        if (id <= 0)
            return "";
        var row = Backend.otbReader.rowForServerId(id);
        if (row < 0)
            return "";
        var d = Backend.otbReader.detailsAt(row);
        return Backend.sprReader.itemImageSource(d.spriteIds, d.itemWidth, d.itemHeight, d.layers);
    }

    function loadGround(name) {
        curGround = name;
        var d = Backend.brushStore.groundBrushEdit(name);
        zorderField.value = d.zorder;
        gItems.clear();
        for (var i = 0; i < d.items.length; ++i)
            gItems.append({
                sid: d.items[i].id,
                chance: d.items[i].chance
            });

        var sets = {
            "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        };
        for (var b = 0; b < d.borders.length; ++b)
            sets[d.borders[b].to] = d.borders[b].tiles.slice();
        borderSets = sets;
        borderTarget = "";
        groundNameField.text = name;
        targetCombo.syncFromApp();
    }
    function newGround() {
        curGround = "";
        groundNameField.text = "";
        zorderField.value = 3500;
        gItems.clear();
        borderSets = ({
                "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
            });
        borderTarget = "";
        targetCombo.syncFromApp();
    }
    function saveGround() {
        var name = groundNameField.text.trim();
        if (name === "" || gItems.count === 0)
            return;
        var items = [];
        for (var i = 0; i < gItems.count; ++i)
            items.push({
                id: gItems.get(i).sid,
                chance: gItems.get(i).chance
            });

        var blocks = [];
        for (var key in borderSets)
            blocks.push({
                to: key,
                tiles: borderSets[key]
            });
        if (Backend.brushStore.saveGroundBrush(name, zorderField.value, items, blocks))
            loadGround(name);
    }

    function loadWall(name) {
        curWall = name;
        wallIds = Backend.brushStore.wallBrushEdit(name);
        wallNameField.text = name;
    }
    function newWall() {
        curWall = "";
        wallNameField.text = "";
        wallIds = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    }
    function saveWall() {
        var name = wallNameField.text.trim();
        if (name === "")
            return;
        if (Backend.brushStore.saveWallBrush(name, wallIds))
            loadWall(name);
    }

    Connections {
        target: Backend.brushStore
        function onBrushesChanged() {
            groundCombo.model = Backend.brushStore.groundBrushNames();
            wallCombo.model = Backend.brushStore.wallBrushNames();
        }
    }
    onOpened: {
        groundCombo.model = Backend.brushStore.groundBrushNames();
        wallCombo.model = Backend.brushStore.wallBrushNames();
        newGround();
        newWall();
    }

    PaletteFilter {
        id: pf
        sourceModel: Backend.otbReader
        mode: "all"
    }

    contentItem: Item {
        id: body
        implicitWidth: 660
        implicitHeight: Math.max(480, editorColumn.implicitHeight)

        Column {
            id: pickerCol
            width: 216
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            spacing: 6

            DmeTextField {
                width: parent.width
                placeholderText: "Search by name or ID..."
                onTextChanged: pf.searchText = text
            }

            DmePanel {
                width: parent.width
                height: pickerCol.height - 30

                GridView {
                    id: pickerGrid
                    anchors.fill: parent
                    anchors.margins: 3
                    clip: true
                    cellWidth: 42
                    cellHeight: 42
                    model: pf

                    delegate: Rectangle {
                        width: 40
                        height: 40
                        color: cellMa.containsMouse ? "#303030" : "#252525"
                        border.color: "#3a3a3a"
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: (typeof spriteIds !== "undefined" && spriteIds.length > 0) ? Backend.sprReader.itemImageSource(spriteIds, typeof itemWidth !== "undefined" ? itemWidth : 1, typeof itemHeight !== "undefined" ? itemHeight : 1, typeof layers !== "undefined" ? layers : 1) : ""
                        }
                        Text {
                            anchors {
                                right: parent.right
                                bottom: parent.bottom
                                margins: 1
                            }
                            text: typeof serverId !== "undefined" ? serverId : ""
                            color: "#888"
                            font.pixelSize: 8
                        }

                        MouseArea {
                            id: cellMa
                            anchors.fill: parent
                            hoverEnabled: true
                            drag.target: dragGhost
                            onPressed: mouse => {
                                dragGhost.sid = (typeof serverId !== "undefined") ? serverId : 0;
                                dragGhost.source = parent.children[0].source;
                                var p = mapToItem(body, mouse.x, mouse.y);
                                dragGhost.x = p.x - 16;
                                dragGhost.y = p.y - 16;
                            }
                            drag.onActiveChanged: dragGhost.visible = drag.active
                            onReleased: {
                                if (dragGhost.visible)
                                    dragGhost.Drag.drop();
                                dragGhost.visible = false;
                            }
                        }
                    }
                }
                DmeScrollBar {
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    anchors.margins: 2
                    flickable: pickerGrid
                }
            }
        }

        Column {
            id: editorColumn
            anchors {
                left: pickerCol.right
                right: parent.right
                top: parent.top
            }
            anchors.leftMargin: 10
            spacing: 8

            Row {
                spacing: 6
                DmeButton {
                    text: "Ground brush"
                    width: 110
                    opacity: root.tab === "ground" ? 1.0 : 0.55
                    onClicked: root.tab = "ground"
                }
                DmeButton {
                    text: "Wall brush"
                    width: 110
                    opacity: root.tab === "wall" ? 1.0 : 0.55
                    onClicked: root.tab = "wall"
                }
            }

            Column {
                visible: root.tab === "ground"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    DmeComboBox {
                        id: groundCombo
                        width: 150
                        height: 23
                        onActivated: root.loadGround(model[currentIndex])
                    }
                    DmeButton {
                        text: "New"
                        width: 60
                        onClicked: root.newGround()
                    }
                    DmeButton {
                        text: "Remove"
                        width: 60
                        enabled: root.curGround !== ""
                        onClicked: {
                            Backend.brushStore.deleteGroundBrush(root.curGround);
                            root.newGround();
                        }
                    }
                }
                Row {
                    spacing: 6
                    Text {
                        text: "Name"
                        color: "#999"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeTextField {
                        id: groundNameField
                        width: 150
                        height: 22
                    }
                    Text {
                        text: "Z-order"
                        color: "#999"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeSpinBox {
                        id: zorderField
                        width: 80
                        from: 0
                        to: 65535
                        value: 3500
                    }
                }

                Text {
                    text: "Ground items (drop an item; weight = chance)"
                    color: "#999"
                    font.pixelSize: 11
                }
                Rectangle {
                    width: parent.width
                    height: 66
                    color: gDrop.containsDrag ? "#2f4f3f" : "#252525"
                    border.color: gDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"
                    border.width: 1

                    DropArea {
                        id: gDrop
                        anchors.fill: parent
                        onDropped: drop => {
                            var sid = drop.source.sid;
                            if (sid > 0)
                                gItems.append({
                                    sid: sid,
                                    chance: 10
                                });
                        }
                    }
                    ListView {
                        anchors.fill: parent
                        anchors.margins: 4
                        orientation: ListView.Horizontal
                        spacing: 4
                        clip: true
                        model: ListModel {
                            id: gItems
                        }
                        delegate: Column {
                            spacing: 1
                            Image {
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(sid)
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: gItems.remove(index)
                                }
                            }
                            DmeSpinBox {
                                width: 44
                                height: 18
                                from: 1
                                to: 1000
                                value: chance
                                onValueModified: gItems.setProperty(index, "chance", value)
                            }
                        }
                    }
                }

                Text {
                    text: "Borders (drop tiles; right click clears a slot)"
                    color: "#999"
                    font.pixelSize: 11
                }

                Row {
                    spacing: 6
                    Text {
                        text: "Border target:"
                        color: "#999"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeComboBox {
                        id: targetCombo
                        width: 190
                        height: 23
                        property var keys: []
                        function syncFromApp() {
                            keys = root.borderTargetKeys();
                            model = keys.map(function (k) {
                                return root.borderTargetLabel(k);
                            });
                            var idx = keys.indexOf(root.borderTarget);
                            currentIndex = idx >= 0 ? idx : 0;
                        }
                        onActivated: root.borderTarget = keys[currentIndex]
                    }
                }
                Item {

                    width: 5 * 50 - 6
                    height: 5 * 50 - 6

                    Rectangle {
                        x: 2 * 50
                        y: 2 * 50
                        width: 44
                        height: 44
                        color: "#1c1c1c"
                        border.color: "#3a3a3a"
                        border.width: 1
                        Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            smooth: false
                            cache: false
                            fillMode: Image.PreserveAspectFit
                            source: gItems.count > 0 ? root.iconSrc(gItems.get(0).sid) : ""
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: gItems.count === 0
                            text: "Ground"
                            color: "#777"
                            font.pixelSize: 9
                        }
                    }

                    Repeater {
                        model: [
                            {
                                bt: 9,
                                lab: "DNW",
                                cx: 0,
                                cy: 0
                            },
                            {
                                bt: 1,
                                lab: "N",
                                cx: 2,
                                cy: 0
                            },
                            {
                                bt: 10,
                                lab: "DNE",
                                cx: 4,
                                cy: 0
                            },
                            {
                                bt: 5,
                                lab: "CNW",
                                cx: 1,
                                cy: 1
                            },
                            {
                                bt: 6,
                                lab: "CNE",
                                cx: 3,
                                cy: 1
                            },
                            {
                                bt: 4,
                                lab: "W",
                                cx: 0,
                                cy: 2
                            },
                            {
                                bt: 2,
                                lab: "E",
                                cx: 4,
                                cy: 2
                            },
                            {
                                bt: 7,
                                lab: "CSW",
                                cx: 1,
                                cy: 3
                            },
                            {
                                bt: 8,
                                lab: "CSE",
                                cx: 3,
                                cy: 3
                            },
                            {
                                bt: 12,
                                lab: "DSW",
                                cx: 0,
                                cy: 4
                            },
                            {
                                bt: 3,
                                lab: "S",
                                cx: 2,
                                cy: 4
                            },
                            {
                                bt: 11,
                                lab: "DSE",
                                cx: 4,
                                cy: 4
                            }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            x: modelData.cx * 50
                            y: modelData.cy * 50
                            width: 44
                            height: 44
                            color: slotDrop.containsDrag ? "#2f4f3f" : "#252525"
                            border.color: slotDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.borderIds[modelData.bt])
                            }
                            Text {
                                anchors {
                                    left: parent.left
                                    top: parent.top
                                    margins: 1
                                }
                                text: modelData.lab
                                color: "#777"
                                font.pixelSize: 8
                            }
                            DropArea {
                                id: slotDrop
                                anchors.fill: parent
                                onDropped: drop => root.setBorderTile(modelData.bt, drop.source.sid)
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: root.setBorderTile(modelData.bt, 0)
                            }
                        }
                    }
                }

                Row {
                    spacing: 6
                    DmeButton {
                        text: "Save"
                        width: 90
                        onClicked: root.saveGround()
                    }
                    DmeButton {
                        text: "Test on map"
                        width: 120
                        enabled: gItems.count > 0 && root.curGround !== ""
                        onClicked: if (root.mapCtrl)
                            root.mapCtrl.useGroundBrush(gItems.get(0).sid)
                    }
                }
            }

            Column {
                visible: root.tab === "wall"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    DmeComboBox {
                        id: wallCombo
                        width: 150
                        height: 23
                        onActivated: root.loadWall(model[currentIndex])
                    }
                    DmeButton {
                        text: "New"
                        width: 60
                        onClicked: root.newWall()
                    }
                    DmeButton {
                        text: "Remove"
                        width: 60
                        enabled: root.curWall !== ""
                        onClicked: {
                            Backend.brushStore.deleteWallBrush(root.curWall);
                            root.newWall();
                        }
                    }
                }
                Row {
                    spacing: 6
                    Text {
                        text: "Name"
                        color: "#999"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeTextField {
                        id: wallNameField
                        width: 150
                        height: 22
                    }
                }

                Text {
                    text: "Wall slots by connection (drop; right click clears)"
                    color: "#999"
                    font.pixelSize: 11
                }
                Grid {
                    columns: 6
                    spacing: 3
                    Repeater {

                        model: ["•", "╵", "╴", "┘", "╶", "└", "─", "┴", "╷", "│", "┐", "┤", "┌", "├", "┬", "┼", "✦"]
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            width: 44
                            height: 52
                            color: wDrop.containsDrag ? "#2f4f3f" : "#252525"
                            border.color: wDrop.containsDrag ? "#7fdc8f" : "#3a3a3a"
                            border.width: 1
                            Image {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    top: parent.top
                                    topMargin: 2
                                }
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.wallIds[index])
                            }
                            Text {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    bottom: parent.bottom
                                    bottomMargin: 1
                                }
                                text: modelData
                                color: "#9a9a9a"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            DropArea {
                                id: wDrop
                                anchors.fill: parent
                                onDropped: drop => {
                                    var w = root.wallIds.slice();
                                    w[index] = drop.source.sid;
                                    root.wallIds = w;
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    var w = root.wallIds.slice();
                                    w[index] = 0;
                                    root.wallIds = w;
                                }
                            }
                        }
                    }
                }

                DmeButton {
                    text: "Save"
                    width: 90
                    onClicked: root.saveWall()
                }
            }

            DmeButton {
                text: "Close"
                width: 90
                onClicked: root.close()
            }
        }

        Image {
            id: dragGhost
            width: 32
            height: 32
            visible: false
            z: 1000
            smooth: false
            cache: false
            fillMode: Image.PreserveAspectFit
            opacity: 0.85
            property int sid: 0
            Drag.active: visible
            Drag.source: dragGhost
            Drag.hotSpot.x: 16
            Drag.hotSpot.y: 16
        }
    }
}

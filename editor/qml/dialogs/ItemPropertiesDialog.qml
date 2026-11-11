pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"
import "../themes/tibia" as TibiaSkin
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: propsDialog
    required property var ctx
    property var mapCtrl: null
    property var containerDialog: null

    title: hasCreature
           ? "Creature & Spawn Properties"
           : (hasSpawn ? "Spawn Properties" : "Item Properties")
    width: isWritable ? Math.max(500, Math.min(740, Overlay.overlay.width - 24)) : 500

    ListModel {
        id: attributeModel
    }

    readonly property bool isTeleport: ctx.teleport === true
    readonly property bool isWritable: ctx.writable === true
    readonly property bool isFluid: ctx.fluid === true
    // Subtype IDs follow RME, including the alternate water and slime values.
    readonly property var fluidTypes: [
        {id: 0, name: "None (empty)"}, {id: 1, name: "Water"},
        {id: 2, name: "Blood"}, {id: 3, name: "Beer"},
        {id: 4, name: "Slime"}, {id: 5, name: "Lemonade"},
        {id: 6, name: "Milk"}, {id: 7, name: "Mana fluid"},
        {id: 8, name: "Ink"}, {id: 9, name: "Water (alternative)"},
        {id: 10, name: "Life fluid"}, {id: 11, name: "Oil"},
        {id: 12, name: "Slime (alternative)"}, {id: 13, name: "Urine"},
        {id: 14, name: "Coconut milk"}, {id: 15, name: "Wine"},
        {id: 19, name: "Mud"}, {id: 21, name: "Fruit juice"},
        {id: 26, name: "Lava"}, {id: 27, name: "Rum"},
        {id: 28, name: "Swamp"}, {id: 35, name: "Tea"},
        {id: 43, name: "Mead"}
    ]
    property var fluidChoices: []
    readonly property bool hasCreature: ctx.creatureName !== undefined && ctx.creatureName !== ""
    readonly property bool hasSpawn: ctx.spawnRadius !== undefined && ctx.spawnRadius > 0
    readonly property int spawnCreatureCount: ctx.spawnCreatureCount || 0
    readonly property bool isContainer: ctx.groupName === "Container"
                                        || (ctx.childCount !== undefined && ctx.childCount > 0)

    onOpened: resetFields()

    function resetFields() {
        countField.from = ctx.subtypeMinimum !== undefined ? ctx.subtypeMinimum : 1;
        countField.to = ctx.subtypeMaximum !== undefined ? ctx.subtypeMaximum : 100;
        countField.value = ctx.count !== undefined ? ctx.count : countField.from;
        fluidChoices = fluidTypes.slice();
        let fluidIndex = fluidChoices.findIndex(entry => entry.id === countField.value);
        if (fluidIndex < 0) {
            fluidIndex = fluidChoices.length;
            fluidChoices = fluidChoices.concat([{id: countField.value, name: "Unknown fluid (" + countField.value + ")"}]);
        }
        fluidField.currentIndex = fluidIndex;
        aidField.value = ctx.actionId > 0 ? ctx.actionId : 0;
        uidField.value = ctx.uniqueId > 0 ? ctx.uniqueId : 0;
        textField.text = ctx.text !== undefined ? ctx.text : "";
        descriptionField.text = ctx.description !== undefined ? ctx.description : "";
        depotField.value = ctx.depotId > 0 ? ctx.depotId : 0;
        doorField.value = ctx.doorId > 0 ? ctx.doorId : 0;
        tierField.value = ctx.tier > 0 ? ctx.tier : 0;
        teleX.value = ctx.teleportX > 0 ? ctx.teleportX : 0;
        teleY.value = ctx.teleportY > 0 ? ctx.teleportY : 0;
        teleZ.value = ctx.teleportZ > 0 ? ctx.teleportZ : 0;
        spawntimeField.value = ctx.creatureSpawntime > 0 ? ctx.creatureSpawntime : 60;
        areaSpawntimeField.value = ctx.spawnCreatureSpawntime > 0
                ? ctx.spawnCreatureSpawntime : 60;
        radiusField.value = ctx.spawnRadius > 0 ? ctx.spawnRadius : 1;
        attributeModel.clear();
        const attributes = ctx.customAttributes || [];
        for (let i = 0; i < attributes.length; ++i) {
            const attribute = attributes[i];
            attributeModel.append({
                "key": attribute.key || "",
                "attributeType": attribute.type || "Unknown",
                "value": attribute.value || "",
                "typeId": attribute.typeId || 0,
                "keyRawBase64": attribute.keyRawBase64 || "",
                "rawBase64": attribute.rawBase64 || ""
            });
        }
    }

    function pasteTeleportPosition() {
        const match = Backend.fileTools.clipboardText().match(/^\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$/);
        if (!match)
            return false;

        const x = Number(match[1]);
        const y = Number(match[2]);
        const z = Number(match[3]);
        if (x > teleX.to || y > teleY.to || z > teleZ.to)
            return false;

        teleX.value = x;
        teleY.value = y;
        teleZ.value = z;
        return true;
    }

    function applyAndClose() {
        if (mapCtrl && ctx.hasItem === true) {
            var p = {
                "actionId": aidField.value,
                "uniqueId": uidField.value,
                "count": isFluid && fluidField.currentIndex >= 0
                         ? fluidChoices[fluidField.currentIndex].id : countField.value,
                "description": descriptionField.text,
                "depotId": depotField.value,
                "doorId": doorField.value,
                "tier": tierField.value
            };
            if (isWritable)
                p["text"] = textField.text;
            if (isTeleport) {
                if (teleX.value === 0 && teleY.value === 0 && teleZ.value === 0)
                    p["teleportClear"] = true;
                else {
                    p["teleportX"] = teleX.value;
                    p["teleportY"] = teleY.value;
                    p["teleportZ"] = teleZ.value;
                }
            }
            if (ctx.customAttributesSupported === true) {
                const attributes = [];
                for (let i = 0; i < attributeModel.count; ++i) {
                    const attribute = attributeModel.get(i);
                    attributes.push({
                        "key": attribute.key,
                        "type": attribute.attributeType,
                        "value": attribute.value,
                        "typeId": attribute.typeId,
                        "keyRawBase64": attribute.keyRawBase64,
                        "rawBase64": attribute.rawBase64
                    });
                }
                p["customAttributes"] = attributes;
            }
            mapCtrl.applyContextItemProperties(p);
        }

        if (mapCtrl && hasCreature)
            mapCtrl.setContextCreatureSpawntime(spawntimeField.value);
        if (mapCtrl && hasSpawn && !hasCreature && spawnCreatureCount > 0)
            mapCtrl.setContextSpawnCreatureSpawntime(areaSpawntimeField.value);
        if (mapCtrl && hasSpawn)
            mapCtrl.setContextSpawnRadius(radiusField.value);
        propsDialog.close();
    }

    contentItem: Item {
        implicitWidth: 476
        implicitHeight: body.implicitHeight + 8

        Column {
            id: body
            x: 4
            y: 4
            width: parent.width - 8
            spacing: 6

            Row {
                spacing: 8
                Text {
                    text: "ID " + propsDialog.ctx.serverId
                    color: "#c0c0c0"
                    font.pixelSize: 13
                    font.bold: true
                }
                Text {
                    text: propsDialog.ctx.name && propsDialog.ctx.name.length ? propsDialog.ctx.name : "(unnamed)"
                    color: "#999"
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                text: "Client Id " + propsDialog.ctx.clientId + "   -   " + propsDialog.ctx.x + ", " + propsDialog.ctx.y + ", " + propsDialog.ctx.z
                color: "#777"
                font.pixelSize: 10
            }

            Item {
                width: 1
                height: 4
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: propsDialog.ctx.subtypeLabel || "Count"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.ctx.hasItem === true
                DmeSpinBox {
                    id: countField
                    visible: !propsDialog.isFluid
                    width: 100
                    from: 1
                    to: 100
                    enabled: propsDialog.ctx.subtypeEditable === true
                    editable: propsDialog.ctx.subtypeEditable === true
                    opacity: enabled ? 1.0 : 0.45
                }
                DmeComboBox {
                    id: fluidField
                    objectName: "fluidSubtypeField"
                    visible: propsDialog.isFluid
                    width: 220
                    model: propsDialog.fluidChoices.map(entry => entry.name)
                    enabled: propsDialog.ctx.subtypeEditable === true
                }
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: "Action ID / Unique ID"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                spacing: 6
                visible: propsDialog.ctx.hasItem === true
                DmeSpinBox {
                    id: aidField
                    width: 145
                    from: 0
                    to: 65535
                }
                DmeSpinBox {
                    id: uidField
                    width: 145
                    from: 0
                    to: 65535
                }
            }

            Text {
                visible: propsDialog.isTeleport
                text: "Destination"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                spacing: 6
                visible: propsDialog.isTeleport
                DmeSpinBox {
                    id: teleX
                    objectName: "teleportXField"
                    width: 100
                    from: 0
                    to: 65535
                    pasteHandler: function() { return propsDialog.pasteTeleportPosition(); }
                }
                DmeSpinBox {
                    id: teleY
                    objectName: "teleportYField"
                    width: 100
                    from: 0
                    to: 65535
                }
                DmeSpinBox {
                    id: teleZ
                    objectName: "teleportZField"
                    width: 84
                    from: 0
                    to: 15
                }
            }

            Row {
                visible: propsDialog.isWritable
                width: parent.width
                spacing: 10

                Column {
                    width: parent.width - textPreview.width - parent.spacing
                    spacing: 4

                    Text {
                        text: "Text"
                        color: Colors.c("text")
                        font.pixelSize: Colors.size(12)
                    }

                    ScrollView {
                        width: parent.width
                        height: 112
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        background: Rectangle {
                            color: Colors.c("base")
                            radius: propsDialog.tibiaTheme ? 0 : 4
                            border.color: textField.activeFocus ? Colors.c("accent") : Colors.c("border")
                            TibiaSkin.TibiaSkin {
                                anchors.fill: parent
                                kind: "field"
                                selected: textField.activeFocus
                            }
                        }

                        TextArea {
                            id: textField
                            objectName: "itemPropertiesText"
                            background: null
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            padding: 8
                            color: Colors.c("text")
                            selectionColor: Colors.c("accent")
                            font.family: Colors.family()
                            font.pixelSize: Colors.size(13)
                        }
                    }
                }

                Column {
                    id: textPreview
                    width: 256
                    spacing: 4

                    Text {
                        text: "In-game preview (Tibia 7.72)"
                        color: Colors.c("text")
                        font.pixelSize: Colors.size(12)
                    }

                    Rectangle {
                        width: parent.width
                        height: 112
                        color: Colors.c("base")
                        radius: propsDialog.tibiaTheme ? 0 : 4
                        border.color: Colors.c("border")

                        TibiaSkin.TibiaSkin {
                            anchors.fill: parent
                            kind: "field"
                        }

                        ListView {
                            objectName: "itemPropertiesTextPreview"
                            x: 6
                            y: 6
                            width: 232
                            height: parent.height - 12
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: propsDialog.visible && propsDialog.isWritable
                                ? Backend.uiTheme.classicItemTextLines(textField.text) : []
                            ScrollBar.vertical: ScrollBar {
                                x: parent.width + 2
                                policy: ScrollBar.AsNeeded
                            }
                            delegate: TibiaSkin.TibiaPreviewLabel {
                                required property string modelData
                                width: 232
                                height: 14
                                text: modelData
                            }
                        }
                    }
                }
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: "Description"
                color: "#999"
                font.pixelSize: 11
            }
            DmeTextField {
                id: descriptionField
                visible: propsDialog.ctx.hasItem === true
                width: parent.width
                placeholderText: "Optional item description"
            }

            Text {
                visible: propsDialog.ctx.hasItem === true
                text: "Depot ID / Door ID / Tier"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.ctx.hasItem === true
                spacing: 6
                DmeSpinBox {
                    id: depotField
                    width: 100
                    from: 0
                    to: 65535
                }
                DmeSpinBox {
                    id: doorField
                    width: 90
                    from: 0
                    to: 255
                }
                DmeSpinBox {
                    id: tierField
                    width: 90
                    from: 0
                    to: 255
                }
            }

            Text {
                visible: propsDialog.hasCreature
                text: "Spawn time - " + (propsDialog.ctx.creatureName || "")
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.hasCreature
                DmeSpinBox {
                    id: spawntimeField
                    width: 100
                    from: 1
                    to: 86400
                }
                Text {
                    text: "seconds"
                    color: "#999"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                visible: propsDialog.hasSpawn && !propsDialog.hasCreature
                text: propsDialog.spawnCreatureCount > 0
                      ? "Spawn time for all creatures in this area"
                      : "Spawn time is stored per creature"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.hasSpawn && !propsDialog.hasCreature
                         && propsDialog.spawnCreatureCount > 0
                spacing: 6
                DmeSpinBox {
                    id: areaSpawntimeField
                    width: 100
                    from: 1
                    to: 86400
                }
                Text {
                    text: "seconds  (" + propsDialog.spawnCreatureCount
                          + (propsDialog.spawnCreatureCount === 1
                             ? " creature)" : " creatures)")
                    color: "#999"
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                visible: propsDialog.hasSpawn && !propsDialog.hasCreature
                         && propsDialog.ctx.spawnCreatureSpawntimeMixed === true
                text: "Creatures currently use different spawn times. Applying will make them equal."
                color: "#D29922"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                width: 456
            }
            Text {
                visible: propsDialog.hasSpawn && !propsDialog.hasCreature
                         && propsDialog.spawnCreatureCount === 0
                text: "This area is empty. Set the time in the Creatures palette before placing a creature."
                color: "#777"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                width: 456
            }
            Text {
                visible: propsDialog.hasSpawn
                text: "Spawn radius"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.hasSpawn
                DmeSpinBox {
                    id: radiusField
                    width: 100
                    from: 1
                    to: 15
                }
            }

            Text {
                visible: propsDialog.ctx.customAttributesSupported === true
                text: "Custom attributes (OTBM 4)"
                color: "#999"
                font.pixelSize: 11
            }
            Rectangle {
                visible: propsDialog.ctx.customAttributesSupported === true
                width: 456
                height: 116
                color: (("#242424"))
                border.width: 1
                border.color: (("#555"))
                clip: true

                Text {
                    anchors.centerIn: parent
                    visible: attributeModel.count === 0
                    text: "No custom attributes"
                    color: "#777"
                    font.pixelSize: 11
                }

                ListView {
                    id: attributeList
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4
                    clip: true
                    model: attributeModel

                    delegate: Item {
                        id: attributeRow
                        required property int index
                        required property string key
                        required property string attributeType
                        required property string value
                        required property int typeId
                        required property string rawBase64

                        width: attributeList.width
                        height: 24
                        readonly property bool editable: attributeType !== "Unknown"

                        Row {
                            anchors.fill: parent
                            spacing: 4

                            DmeTextField {
                                width: 125
                                text: attributeRow.key
                                placeholderText: "Key"
                                onEditingFinished: attributeModel.setProperty(
                                    attributeRow.index, "key", text)
                            }
                            DmeComboBox {
                                width: 92
                                model: attributeRow.editable
                                       ? ["Number", "Float", "Boolean", "String", "Double"]
                                       : ["Unknown"]
                                currentIndex: {
                                    const found = model.indexOf(attributeRow.attributeType);
                                    return found >= 0 ? found : 0;
                                }
                                enabled: attributeRow.editable
                                onActivated: {
                                    attributeModel.setProperty(
                                        attributeRow.index, "attributeType", currentText);
                                    if (currentText === "Boolean"
                                            && attributeRow.value !== "true"
                                            && attributeRow.value !== "false") {
                                        attributeModel.setProperty(
                                            attributeRow.index, "value", "false");
                                    }
                                }
                            }
                            DmeTextField {
                                width: 174
                                text: attributeRow.value
                                placeholderText: attributeRow.editable ? "Value" : "Preserved binary value"
                                enabled: attributeRow.editable
                                onEditingFinished: attributeModel.setProperty(
                                    attributeRow.index, "value", text)
                            }
                            DmeButton {
                                width: 43
                                text: "X"
                                variant: "danger"
                                onClicked: attributeModel.remove(attributeRow.index)
                            }
                        }
                    }
                }
            }
            DmeButton {
                visible: propsDialog.ctx.customAttributesSupported === true
                text: "Add Attribute"
                width: 110
                onClicked: attributeModel.append({
                    "key": "",
                    "attributeType": "Number",
                    "value": "0",
                    "typeId": 2,
                    "keyRawBase64": "",
                    "rawBase64": ""
                })
            }

            Item {
                width: 1
                height: 4
            }

            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                DmeButton {
                    text: "Open Container"
                    width: 120
                    visible: propsDialog.isContainer
                    onClicked: {
                        propsDialog.close();
                        if (propsDialog.containerDialog)
                            propsDialog.containerDialog.open(
                                propsDialog.mapCtrl.contextItemPath(),
                                propsDialog.ctx.name || "Container");
                    }
                }
                DmeButton {
                    text: "OK"
                    width: 90
                    onClicked: propsDialog.applyAndClose()
                }
                DmeButton {
                    text: "Cancel"
                    width: 90
                    onClicked: propsDialog.close()
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import "../style"

DmeDialog {
    id: propsDialog
    required property var ctx
    property var mapCtrl: null
    property var containerDialog: null

    title: "Item Properties"
    width: 500

    ListModel {
        id: attributeModel
    }

    readonly property bool isTeleport: ctx.teleport === true
    readonly property bool isWritable: ctx.writable === true
    readonly property bool hasCreature: ctx.creatureName !== undefined && ctx.creatureName !== ""
    readonly property bool hasSpawn: ctx.spawnRadius !== undefined && ctx.spawnRadius > 0
    readonly property bool isContainer: ctx.groupName === "Container"
                                        || (ctx.childCount !== undefined && ctx.childCount > 0)

    onOpened: resetFields()

    function resetFields() {
        countField.from = ctx.subtypeMinimum !== undefined ? ctx.subtypeMinimum : 1;
        countField.to = ctx.subtypeMaximum !== undefined ? ctx.subtypeMaximum : 100;
        countField.value = ctx.count !== undefined ? ctx.count : countField.from;
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
                "rawBase64": attribute.rawBase64 || ""
            });
        }
    }

    function applyAndClose() {
        if (mapCtrl && ctx.hasItem === true) {
            var p = {
                "actionId": aidField.value,
                "uniqueId": uidField.value,
                "count": countField.value,
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
                        "rawBase64": attribute.rawBase64
                    });
                }
                p["customAttributes"] = attributes;
            }
            mapCtrl.applyContextItemProperties(p);
        }

        if (mapCtrl && hasCreature)
            mapCtrl.setContextCreatureSpawntime(spawntimeField.value);
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
                    width: 100
                    from: 1
                    to: 100
                    enabled: propsDialog.ctx.subtypeEditable === true
                    editable: propsDialog.ctx.subtypeEditable === true
                    opacity: enabled ? 1.0 : 0.45
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
                    width: 100
                    from: 0
                    to: 65535
                }
                DmeSpinBox {
                    id: teleY
                    width: 100
                    from: 0
                    to: 65535
                }
                DmeSpinBox {
                    id: teleZ
                    width: 84
                    from: 0
                    to: 15
                }
            }

            Text {
                visible: propsDialog.isWritable
                text: "Text"
                color: "#999"
                font.pixelSize: 11
            }
            Row {
                visible: propsDialog.isWritable
                DmeTextField {
                    id: textField
                    width: 456
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
                width: 456
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
                text: "Spawntime (" + (propsDialog.ctx.creatureName || "") + ")"
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
                color: Backend.uiTheme.style === "github-dark" ? "#0D1117" : "#242424"
                border.width: 1
                border.color: Backend.uiTheme.style === "github-dark" ? "#30363D" : "#555"
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

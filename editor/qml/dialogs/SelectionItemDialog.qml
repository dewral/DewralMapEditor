import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: root

    property string mode: "find"
    property string scope: "selection"
    property var mapCtrl: null
    property int defaultFrom: (mapCtrl && mapCtrl.brushServerId > 0) ? mapCtrl.brushServerId : 100
    property string resultText: ""
    property int selectedRule: -1
    property string pickerTarget: "from"

    readonly property string scopeLabel: scope === "map" ? "map" : "selection"

    title: (mode === "find" ? "Find Item" : mode === "remove" ? "Remove Item" : "Replace Items")
           + (scope === "map" ? " (entire map)" : " on selection")
    width: mode === "replace" ? 620 : 340
    height: mode === "replace" ? 520 : 170

    function openItemPicker(target) {
        pickerTarget = target;
        itemPicker.openFor(target === "from" ? fromField.value : toField.value);
    }

    function itemName(serverId) {
        return Backend.otbReader ? Backend.otbReader.nameForServerId(serverId) : "";
    }

    function itemIcon(serverId) {
        if (!Backend.otbReader || !Backend.sprReader || serverId <= 0)
            return "";
        var row = Backend.otbReader.rowForServerId(serverId);
        if (row < 0)
            return "";
        var details = Backend.otbReader.detailsAt(row);
        return Backend.sprReader.itemImageSource(details.spriteIds,
                                                  details.itemWidth,
                                                  details.itemHeight,
                                                  details.layers);
    }

    function addRule() {
        if (fromField.value <= 0 || toField.value <= 0 || fromField.value === toField.value)
            return;

        for (var i = 0; i < replacementRules.count; ++i) {
            if (replacementRules.get(i).fromId === fromField.value) {
                replacementRules.set(i, {
                    "fromId": fromField.value,
                    "toId": toField.value,
                    "fromName": itemName(fromField.value),
                    "toName": itemName(toField.value),
                    "fromIcon": itemIcon(fromField.value),
                    "toIcon": itemIcon(toField.value)
                });
                selectedRule = i;
                return;
            }
        }

        replacementRules.append({
            "fromId": fromField.value,
            "toId": toField.value,
            "fromName": itemName(fromField.value),
            "toName": itemName(toField.value),
            "fromIcon": itemIcon(fromField.value),
            "toIcon": itemIcon(toField.value)
        });
        selectedRule = replacementRules.count - 1;
    }

    function removeSelectedRule() {
        if (selectedRule < 0 || selectedRule >= replacementRules.count)
            return;
        replacementRules.remove(selectedRule);
        selectedRule = Math.min(selectedRule, replacementRules.count - 1);
    }

    function executeRules() {
        if (!mapCtrl || replacementRules.count === 0)
            return;

        var total = 0;
        var onMap = scope === "map";
        for (var i = 0; i < replacementRules.count; ++i) {
            var rule = replacementRules.get(i);
            total += onMap
                    ? mapCtrl.replaceItemsOnMap(rule.fromId, rule.toId)
                    : mapCtrl.replaceItemsOnSelection(rule.fromId, rule.toId);
        }
        resultText = "Replaced: " + total;
    }

    onOpened: {
        fromField.value = defaultFrom;
        toField.value = defaultFrom;
        simpleFromField.value = defaultFrom;
        replacementRules.clear();
        selectedRule = -1;
        resultText = "";
    }

    ListModel {
        id: replacementRules
    }

    ItemPickerDialog {
        id: itemPicker

        onItemSelected: function(serverId) {
            if (root.pickerTarget === "from")
                fromField.value = serverId;
            else
                toField.value = serverId;
        }
    }

    contentItem: Item {
        implicitWidth: root.mode === "replace" ? 588 : 308
        implicitHeight: root.mode === "replace" ? 430 : simpleContent.implicitHeight

        Column {
            id: simpleContent
            visible: root.mode !== "replace"
            width: parent.width
            spacing: 8

            Text {
                text: root.mode === "remove"
                      ? ("Remove items by server ID from " + root.scopeLabel + ":")
                      : ("Find items by server ID in " + root.scopeLabel + ":")
                color: "#C9D1D9"
                font.pixelSize: 12
            }

            Row {
                spacing: 6

                Text {
                    width: 80
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Server ID"
                    color: "#8B949E"
                    font.pixelSize: 11
                }

                DmeSpinBox {
                    id: simpleFromField
                    width: 110
                    from: 100
                    to: 65535
                    value: root.defaultFrom
                }

                Text {
                    width: 100
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.itemName(simpleFromField.value)
                    color: "#8B949E"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            Text {
                visible: root.resultText.length > 0
                text: root.resultText
                color: "#F0F6FC"
                font.pixelSize: 11
                font.bold: true
            }

            Row {
                spacing: 6

                DmeButton {
                    width: 100
                    text: root.mode === "find" ? "Count" : "Remove"
                    variant: root.mode === "remove" ? "danger" : "primary"
                    onClicked: {
                        if (!root.mapCtrl)
                            return;
                        var onMap = root.scope === "map";
                        if (root.mode === "find") {
                            var count = onMap
                                    ? Backend.otbmReader.countItemsOnMap(simpleFromField.value)
                                    : root.mapCtrl.countItemOnSelection(simpleFromField.value);
                            if (onMap && count > 0)
                                root.mapCtrl.jumpToItemOnMap(simpleFromField.value);
                            root.resultText = count > 0
                                    ? ("Found: " + count + (onMap ? " (jumped to first)" : ""))
                                    : "Not found";
                        } else {
                            var removed = onMap
                                    ? root.mapCtrl.removeItemsOnMap(simpleFromField.value)
                                    : root.mapCtrl.removeItemOnSelection(simpleFromField.value);
                            root.resultText = "Removed: " + removed;
                        }
                    }
                }

                DmeButton {
                    width: 100
                    text: "Close"
                    onClicked: root.close()
                }
            }
        }

        Column {
            id: replaceContent
            visible: root.mode === "replace"
            width: parent.width
            spacing: 12

            Rectangle {
                width: parent.width
                height: 276
                radius: 6
                color: "#0D1117"
                border.width: 1
                border.color: "#30363D"
                clip: true

                Text {
                    anchors.centerIn: parent
                    visible: replacementRules.count === 0
                    text: "No replacement rules.\nSelect two items below and click Add."
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.35
                    color: "#7D8590"
                    font.pixelSize: 12
                }

                ListView {
                    id: rulesView
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    spacing: 4
                    model: replacementRules
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        required property int index
                        required property int fromId
                        required property int toId
                        required property string fromName
                        required property string toName
                        required property string fromIcon
                        required property string toIcon

                        width: rulesView.width - (rulesView.ScrollBar.vertical.visible ? 12 : 0)
                        height: 58
                        radius: 5
                        color: root.selectedRule === index ? "#174D2B" : ruleMouse.containsMouse ? "#21262D" : "#161B22"
                        border.width: 1
                        border.color: root.selectedRule === index ? "#3FB950" : "#30363D"

                        Row {
                            anchors {
                                fill: parent
                                leftMargin: 12
                                rightMargin: 12
                            }
                            spacing: 12

                            ItemPreview {
                                anchors.verticalCenter: parent.verticalCenter
                                itemId: fromId
                                itemTitle: fromName
                                imageSource: fromIcon
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "\u2192"
                                color: "#3FB950"
                                font.pixelSize: 22
                            }

                            ItemPreview {
                                anchors.verticalCenter: parent.verticalCenter
                                itemId: toId
                                itemTitle: toName
                                imageSource: toIcon
                            }
                        }

                        MouseArea {
                            id: ruleMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.selectedRule = index
                            onDoubleClicked: {
                                fromField.value = fromId;
                                toField.value = toId;
                            }
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: 58
                spacing: 12

                ItemPreview {
                    anchors.verticalCenter: parent.verticalCenter
                    itemId: fromField.value
                    itemTitle: root.itemName(fromField.value)
                    imageSource: root.itemIcon(fromField.value)
                    clickable: true
                    onClicked: root.openItemPicker("from")
                }

                DmeSpinBox {
                    id: fromField
                    anchors.verticalCenter: parent.verticalCenter
                    width: 96
                    height: 30
                    from: 100
                    to: 65535
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u2192"
                    color: "#3FB950"
                    font.pixelSize: 22
                }

                ItemPreview {
                    anchors.verticalCenter: parent.verticalCenter
                    itemId: toField.value
                    itemTitle: root.itemName(toField.value)
                    imageSource: root.itemIcon(toField.value)
                    clickable: true
                    onClicked: root.openItemPicker("to")
                }

                DmeSpinBox {
                    id: toField
                    anchors.verticalCenter: parent.verticalCenter
                    width: 96
                    height: 30
                    from: 100
                    to: 65535
                }
            }

            Item {
                width: parent.width
                height: 34

                Row {
                    anchors.left: parent.left
                    spacing: 8

                    DmeButton {
                        width: 88
                        height: 32
                        text: "Add"
                        variant: "primary"
                        enabled: fromField.value !== toField.value
                        onClicked: root.addRule()
                    }

                    DmeButton {
                        width: 88
                        height: 32
                        text: "Remove"
                        enabled: root.selectedRule >= 0
                        onClicked: root.removeSelectedRule()
                    }
                }

                Row {
                    anchors.right: parent.right
                    spacing: 8

                    DmeButton {
                        width: 88
                        height: 32
                        text: "Execute"
                        variant: "primary"
                        enabled: replacementRules.count > 0
                        onClicked: root.executeRules()
                    }

                    DmeButton {
                        width: 88
                        height: 32
                        text: "Close"
                        onClicked: root.close()
                    }
                }
            }

            Text {
                visible: root.resultText.length > 0
                text: root.resultText
                color: "#F0F6FC"
                font.pixelSize: 11
                font.bold: true
            }
        }
    }

    component ItemPreview: Item {
        property int itemId: 0
        property string itemTitle: ""
        property string imageSource: ""
        property bool clickable: false

        signal clicked

        width: 160
        height: 48

        Rectangle {
            id: iconFrame
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
            width: 44
            height: 44
            radius: 5
            color: "#0D1117"
            border.width: 1
            border.color: previewMouse.containsMouse && clickable ? "#3FB950" : "#30363D"

            Image {
                anchors.centerIn: parent
                width: 36
                height: 36
                source: imageSource
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
        }

        Column {
            anchors {
                left: iconFrame.right
                leftMargin: 8
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            spacing: 2

            Text {
                width: parent.width
                text: itemTitle.length > 0 ? itemTitle : "Item " + itemId
                color: "#F0F6FC"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                text: "Server ID " + itemId
                color: "#8B949E"
                font.pixelSize: 10
            }
        }

        MouseArea {
            id: previewMouse
            anchors.fill: parent
            enabled: clickable
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}

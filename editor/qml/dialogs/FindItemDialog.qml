pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var paletteNavigator
    property string searchMode: "server"
    property string selectedType: "depot"
    property var results: []
    property int total: 0
    property bool truncated: false
    readonly property bool propertyMode: searchMode === "properties"
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    title: "Search for Item"
    width: 850

    function spriteSource(item) {
        if (!item || !item.spriteIds || item.spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(item.spriteIds,
                                                item.itemWidth || 1,
                                                item.itemHeight || 1,
                                                item.layers || 1);
    }

    function selectedProperties() {
        const values = [];
        if (unpassable.checked) values.push("unpassable");
        if (unmovable.checked) values.push("unmovable");
        if (blockMissiles.checked) values.push("blockmissiles");
        if (blockPathfinder.checked) values.push("blockpathfinder");
        if (readable.checked) values.push("readable");
        if (writeable.checked) values.push("writeable");
        if (pickupable.checked) values.push("pickupable");
        if (stackable.checked) values.push("stackable");
        if (rotatable.checked) values.push("rotatable");
        if (hangable.checked) values.push("hangable");
        if (hookEast.checked) values.push("hookeast");
        if (hookSouth.checked) values.push("hooksouth");
        if (hasElevation.checked) values.push("haselevation");
        if (ignoreLook.checked) values.push("ignorelook");
        if (floorChange.checked) values.push("floorchange");
        return values;
    }

    function runSearch() {
        const response = Backend.otbReader.findItems({
            mode: searchMode,
            serverId: serverIdField.value,
            clientId: clientIdField.value,
            name: nameField.text,
            type: selectedType,
            properties: selectedProperties(),
            force: forceSelect.checked
        });
        results = response.results || [];
        total = response.total || 0;
        truncated = response.truncated === true;
        resultList.currentIndex = results.length > 0 ? 0 : -1;
    }

    function scheduleSearch() {
        searchTimer.restart();
    }

    function acceptSelection() {
        if (resultList.currentIndex < 0
            || resultList.currentIndex >= results.length)
            return;
        paletteNavigator.selectRaw(results[resultList.currentIndex].serverId);
        close();
    }

    onOpened: {
        searchMode = "server";
        serverIdField.value = 100;
        clientIdField.value = 100;
        nameField.text = "";
        selectedType = "depot";
        runSearch();
    }

    Timer {
        id: searchTimer
        interval: 180
        repeat: false
        onTriggered: dialog.runSearch()
    }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 8

            DmePanel {
                width: 178
                height: 500

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    Choice {
                        text: "Find by Server ID"
                        checked: dialog.searchMode === "server"
                        onClicked: {
                            dialog.searchMode = "server";
                            dialog.runSearch();
                        }
                    }
                    Choice {
                        text: "Find by Client ID"
                        checked: dialog.searchMode === "client"
                        onClicked: {
                            dialog.searchMode = "client";
                            dialog.runSearch();
                        }
                    }
                    Choice {
                        text: "Find by Name"
                        checked: dialog.searchMode === "name"
                        onClicked: {
                            dialog.searchMode = "name";
                            dialog.runSearch();
                        }
                    }
                    Choice {
                        text: "Find by Types"
                        checked: dialog.searchMode === "type"
                        onClicked: {
                            dialog.searchMode = "type";
                            dialog.runSearch();
                        }
                    }
                    Choice {
                        text: "Find by Properties"
                        checked: dialog.searchMode === "properties"
                        onClicked: {
                            dialog.searchMode = "properties";
                            dialog.runSearch();
                        }
                    }

                    SectionLabel { text: "Server ID" }
                    DmeSpinBox {
                        id: serverIdField
                        width: parent.width
                        from: 1
                        to: 65535
                        enabled: dialog.searchMode === "server"
                        onValueChanged: dialog.scheduleSearch()
                    }
                    DmeCheckBox {
                        id: forceSelect
                        text: "Force select"
                        enabled: dialog.searchMode === "server"
                        onClicked: {
                            checked = !checked;
                            dialog.runSearch();
                        }
                    }

                    SectionLabel { text: "Client ID" }
                    DmeSpinBox {
                        id: clientIdField
                        width: parent.width
                        from: 1
                        to: 65535
                        enabled: dialog.searchMode === "client"
                        onValueChanged: dialog.scheduleSearch()
                    }

                    SectionLabel { text: "Name" }
                    DmeTextField {
                        id: nameField
                        width: parent.width
                        enabled: dialog.searchMode === "name"
                        placeholderText: "At least 2 characters"
                        onTextChanged: dialog.scheduleSearch()
                    }
                }
            }

            DmePanel {
                width: 150
                height: 500

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    SectionLabel { text: "Types" }
                    Choice { text: "Depot"; checked: dialog.selectedType === "depot"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("depot") }
                    Choice { text: "Mailbox"; checked: dialog.selectedType === "mailbox"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("mailbox") }
                    Choice { text: "Trash Holder"; checked: dialog.selectedType === "trashholder"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("trashholder") }
                    Choice { text: "Container"; checked: dialog.selectedType === "container"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("container") }
                    Choice { text: "Door"; checked: dialog.selectedType === "door"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("door") }
                    Choice { text: "Magic Field"; checked: dialog.selectedType === "magicfield"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("magicfield") }
                    Choice { text: "Teleport"; checked: dialog.selectedType === "teleport"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("teleport") }
                    Choice { text: "Bed"; checked: dialog.selectedType === "bed"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("bed") }
                    Choice { text: "Key"; checked: dialog.selectedType === "key"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("key") }
                    Choice { text: "Podium"; checked: dialog.selectedType === "podium"; enabled: dialog.searchMode === "type"; onClicked: dialog.chooseType("podium") }
                }
            }

            DmePanel {
                width: 175
                height: 500

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7

                    SectionLabel { text: "Properties" }
                    PropertyCheck { id: unpassable; text: "Unpassable" }
                    PropertyCheck { id: unmovable; text: "Unmovable" }
                    PropertyCheck { id: blockMissiles; text: "Block Missiles" }
                    PropertyCheck { id: blockPathfinder; text: "Block Pathfinder" }
                    PropertyCheck { id: readable; text: "Readable" }
                    PropertyCheck { id: writeable; text: "Writeable" }
                    PropertyCheck { id: pickupable; text: "Pickupable" }
                    PropertyCheck { id: stackable; text: "Stackable" }
                    PropertyCheck { id: rotatable; text: "Rotatable" }
                    PropertyCheck { id: hangable; text: "Hangable" }
                    PropertyCheck { id: hookEast; text: "Hook East" }
                    PropertyCheck { id: hookSouth; text: "Hook South" }
                    PropertyCheck { id: hasElevation; text: "Has Elevation" }
                    PropertyCheck { id: ignoreLook; text: "Ignore Look" }
                    PropertyCheck { id: floorChange; text: "Floor Change" }
                }
            }

            DmePanel {
                width: 300
                height: 500

                Column {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 5

                    Row {
                        width: parent.width

                        SectionLabel {
                            text: "Result"
                        }
                        Text {
                            width: parent.width - 45
                            horizontalAlignment: Text.AlignRight
                            text: dialog.total + (dialog.truncated ? "+ matches" : " match(es)")
                            color: dialog.truncated ? "#e3b341" : "#8b949e"
                            font.pixelSize: 10
                        }
                    }

                    ListView {
                        id: resultList
                        width: parent.width
                        height: parent.height - 28
                        clip: true
                        model: dialog.results
                        highlightMoveDuration: 0

                        delegate: Rectangle {
                            id: resultRow
                            required property var modelData
                            required property int index

                            width: resultList.width - 12
                            height: 54
                            color: resultList.currentIndex === index
                                   ? (dialog.githubUi ? (dialog.grayUi ? "#4A3A1F" : "#163B2C") : "#505050")
                                   : (resultMouse.containsMouse
                                      ? (dialog.githubUi ? (dialog.grayUi ? "#303030" : "#161E27") : "#383838")
                                      : "transparent")

                            Image {
                                anchors {
                                    left: parent.left
                                    leftMargin: 5
                                    verticalCenter: parent.verticalCenter
                                }
                                width: 42
                                height: 42
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: dialog.spriteSource(resultRow.modelData)
                            }

                            Column {
                                anchors {
                                    left: parent.left
                                    leftMargin: 53
                                    right: parent.right
                                    rightMargin: 5
                                    verticalCenter: parent.verticalCenter
                                }
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: resultRow.modelData.name || "Unknown item"
                                    color: "#d0d0d0"
                                    font.pixelSize: 11
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: "Server " + resultRow.modelData.serverId
                                          + "   Client " + resultRow.modelData.clientId
                                    color: "#8b949e"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: resultRow.modelData.group
                                    color: "#6e7681"
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: resultMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: resultList.currentIndex = resultRow.index
                                onDoubleClicked: {
                                    resultList.currentIndex = resultRow.index;
                                    dialog.acceptSelection();
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: dialog.results.length === 0
                            text: "No matches for your search."
                            color: "#8b949e"
                            font.pixelSize: 11
                        }
                    }

                    DmeScrollBar {
                        anchors {
                            right: parent.right
                            top: parent.top
                            topMargin: 22
                            bottom: parent.bottom
                        }
                        flickable: resultList
                    }
                }
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "OK"
                width: 90
                enabled: resultList.currentIndex >= 0
                variant: "primary"
                onClicked: dialog.acceptSelection()
            }
            DmeButton {
                text: "Cancel"
                width: 90
                onClicked: dialog.close()
            }
        }
    }

    function chooseType(type) {
        selectedType = type;
        runSearch();
    }

    component SectionLabel: Text {
        color: "#8b949e"
        font.pixelSize: 10
        font.bold: true
    }

    component Choice: Item {
        id: choice
        property string text: ""
        property bool checked: false
        signal clicked
        implicitWidth: 145
        implicitHeight: 18
        opacity: enabled ? 1 : 0.42

        Rectangle {
            width: 12
            height: 12
            radius: 6
            anchors.verticalCenter: parent.verticalCenter
            color: "transparent"
            border.width: 1
            border.color: choice.checked ? "#58a6ff" : "#777"

            Rectangle {
                anchors.centerIn: parent
                width: 6
                height: 6
                radius: 3
                visible: choice.checked
                color: "#58a6ff"
            }
        }

        Text {
            anchors {
                left: parent.left
                leftMargin: 19
                verticalCenter: parent.verticalCenter
            }
            text: choice.text
            color: "#c9d1d9"
            font.pixelSize: 11
        }

        MouseArea {
            anchors.fill: parent
            enabled: choice.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.clicked()
        }
    }

    component PropertyCheck: DmeCheckBox {
        enabled: dialog.propertyMode
        opacity: enabled ? 1 : 0.42
        onClicked: {
            checked = !checked;
            dialog.runSearch();
        }
    }
}

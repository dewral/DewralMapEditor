pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as FluentColors

DmeDialog {
    id: dialog
    objectName: "findItemDialog"

    required property var paletteNavigator
    property var itemCatalog: Backend.otbReader
    property string searchMode: "name"
    property string selectedType: "depot"
    property var results: []
    property int total: 0
    property bool truncated: false
    readonly property bool propertyMode: searchMode === "properties"
    readonly property bool fluentUi: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool neutralUi: fluentUi || Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property color foreground: windowsClassic ? "#202020"
        : fluentUi ? FluentColors.c("text") : "#f0f0f0"
    readonly property color secondary: windowsClassic ? "#595959"
        : fluentUi ? FluentColors.c("muted") : neutralUi ? "#b1b1b1" : "#a0a8b1"
    readonly property color accent: windowsClassic ? "#0067c0"
        : fluentUi ? FluentColors.c("accent") : "#4a9fc2"
    readonly property color panelColor: windowsClassic ? "#f4f4f4"
        : fluentUi ? FluentColors.c("surface") : neutralUi ? "#232323" : "#161b22"
    readonly property color lineColor: windowsClassic ? "#ababab"
        : fluentUi ? FluentColors.c("border") : neutralUi ? "#414141" : "#30363d"
    readonly property color selectedColor: Qt.tint(panelColor,
        Qt.rgba(accent.r, accent.g, accent.b, 0.22))
    readonly property real viewportWidth: Controls.Overlay.overlay ? Controls.Overlay.overlay.width : 1024
    readonly property real viewportHeight: Controls.Overlay.overlay ? Controls.Overlay.overlay.height : 800

    title: "Search for Item"
    width: Math.max(700, Math.min(920, viewportWidth - 32))
    height: Math.max(420, Math.min(680, viewportHeight - 32))

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
        searchTimer.stop();
        const response = itemCatalog.findItems({
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

    function scheduleSearch() { searchTimer.restart(); }

    function focusName() {
        if (visible && searchMode === "name")
            nameField.focusEditor();
    }

    function chooseMode(mode) {
        searchMode = mode;
        runSearch();
        if (mode === "name") Qt.callLater(focusName);
    }

    function chooseType(type) {
        selectedType = type;
        runSearch();
    }

    function acceptSelection() {
        if (resultList.currentIndex < 0 || resultList.currentIndex >= results.length)
            return;
        paletteNavigator.selectRaw(results[resultList.currentIndex].serverId);
        close();
    }

    onOpened: {
        searchMode = "name";
        serverIdField.value = 100;
        clientIdField.value = 100;
        nameField.text = "";
        selectedType = "depot";
        runSearch();
        Qt.callLater(focusName);
    }
    onClosed: searchTimer.stop()

    Timer {
        id: searchTimer
        interval: 180
        repeat: false
        onTriggered: dialog.runSearch()
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            objectName: "findItemModes"
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [
                    {mode: "server", label: "Server ID"},
                    {mode: "client", label: "Client ID"},
                    {mode: "name", label: "Name"},
                    {mode: "type", label: "Types"},
                    {mode: "properties", label: "Properties"}
                ]
                delegate: ModeButton {
                    required property var modelData
                    mode: modelData.mode
                    labelText: modelData.label
                    objectName: "findItemMode_" + mode
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 46
                }
            }
        }

        RowLayout {
            objectName: "findItemFields"
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6
                FieldLabel { text: "Server ID" }
                DmeSpinBox {
                    id: serverIdField
                    objectName: "findItemServerId"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    from: 1; to: 65535
                    enabled: dialog.searchMode === "server"
                    onValueChanged: dialog.scheduleSearch()
                }
                DmeCheckBox {
                    id: forceSelect
                    objectName: "findItemForceSelect"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    text: "Force select"
                    enabled: dialog.searchMode === "server"
                    opacity: enabled ? 1 : 0.6
                    onClicked: { checked = !checked; dialog.runSearch(); }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6
                FieldLabel { text: "Client ID" }
                DmeSpinBox {
                    id: clientIdField
                    objectName: "findItemClientId"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    from: 1; to: 65535
                    enabled: dialog.searchMode === "client"
                    onValueChanged: dialog.scheduleSearch()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1.45
                Layout.alignment: Qt.AlignTop
                spacing: 6
                FieldLabel { text: "Name" }
                DmeTextField {
                    id: nameField
                    objectName: "findItemName"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    enabled: dialog.searchMode === "name"
                    placeholderText: "At least 2 characters"
                    onTextChanged: dialog.scheduleSearch()
                }
            }
        }

        RowLayout {
            objectName: "findItemSections"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            SectionPanel {
                objectName: "findItemTypesPanel"
                Layout.preferredWidth: dialog.width < 820 ? 132 : 146
                Layout.fillHeight: true
                ColumnLayout {
                    anchors { fill: parent; margins: 11 }
                    spacing: 9
                    SectionLabel { text: "Types" }
                    Separator {}
                    Flickable {
                        id: typesScroll
                        objectName: "findItemTypesScroll"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: typesColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        Column {
                            id: typesColumn
                            width: typesScroll.width - 8
                            spacing: 2
                            Repeater {
                                model: [
                                    {key: "depot", label: "Depot"},
                                    {key: "mailbox", label: "Mailbox"},
                                    {key: "trashholder", label: "Trash Holder"},
                                    {key: "container", label: "Container"},
                                    {key: "door", label: "Door"},
                                    {key: "magicfield", label: "Magic Field"},
                                    {key: "teleport", label: "Teleport"},
                                    {key: "bed", label: "Bed"},
                                    {key: "key", label: "Key"},
                                    {key: "podium", label: "Podium"}
                                ]
                                delegate: TypeChoice {
                                    required property var modelData
                                    objectName: "findItemType_" + modelData.key
                                    typeValue: modelData.key
                                    text: modelData.label
                                    width: typesColumn.width
                                }
                            }
                        }
                        DmeScrollBar { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } flickable: typesScroll }
                    }
                }
            }

            SectionPanel {
                objectName: "findItemPropertiesPanel"
                Layout.preferredWidth: dialog.width < 820 ? 228 : 256
                Layout.fillHeight: true
                ColumnLayout {
                    anchors { fill: parent; margins: 11 }
                    spacing: 9
                    SectionLabel { text: "Properties" }
                    Separator {}
                    Flickable {
                        id: propertiesScroll
                        objectName: "findItemPropertiesScroll"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: propertyGrid.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        Grid {
                            id: propertyGrid
                            width: propertiesScroll.width - 8
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 5
                            PropertyCheck { id: unpassable; propertyKey: "unpassable"; text: "Unpassable" }
                            PropertyCheck { id: unmovable; propertyKey: "unmovable"; text: "Unmovable" }
                            PropertyCheck { id: blockMissiles; propertyKey: "blockmissiles"; text: "Block Missiles" }
                            PropertyCheck { id: blockPathfinder; propertyKey: "blockpathfinder"; text: "Block Pathfinder" }
                            PropertyCheck { id: readable; propertyKey: "readable"; text: "Readable" }
                            PropertyCheck { id: writeable; propertyKey: "writeable"; text: "Writeable" }
                            PropertyCheck { id: pickupable; propertyKey: "pickupable"; text: "Pickupable" }
                            PropertyCheck { id: stackable; propertyKey: "stackable"; text: "Stackable" }
                            PropertyCheck { id: rotatable; propertyKey: "rotatable"; text: "Rotatable" }
                            PropertyCheck { id: hangable; propertyKey: "hangable"; text: "Hangable" }
                            PropertyCheck { id: hookEast; propertyKey: "hookeast"; text: "Hook East" }
                            PropertyCheck { id: hookSouth; propertyKey: "hooksouth"; text: "Hook South" }
                            PropertyCheck { id: hasElevation; propertyKey: "haselevation"; text: "Has Elevation" }
                            PropertyCheck { id: ignoreLook; propertyKey: "ignorelook"; text: "Ignore Look" }
                            PropertyCheck { id: floorChange; propertyKey: "floorchange"; text: "Floor Change" }
                        }
                        DmeScrollBar { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } flickable: propertiesScroll }
                    }
                }
            }

            SectionPanel {
                objectName: "findItemResultsPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                ColumnLayout {
                    anchors { fill: parent; margins: 7 }
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4; Layout.rightMargin: 4; Layout.topMargin: 4
                        SectionLabel { text: "Result"; Layout.fillWidth: true }
                        Text {
                            objectName: "findItemResultCount"
                            text: dialog.total + (dialog.truncated ? "+ matches" : " match(es)")
                            color: dialog.truncated ? "#e3b341" : dialog.secondary
                            font.family: "Segoe UI"; font.pixelSize: 11
                        }
                    }
                    Separator {}
                    ListView {
                        id: resultList
                        objectName: "findItemResults"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: dialog.results
                        clip: true
                        spacing: 4
                        highlightMoveDuration: 0
                        delegate: Rectangle {
                            id: resultRow
                            required property var modelData
                            required property int index
                            objectName: "findItemResult_" + modelData.serverId
                            width: resultList.width - 10
                            height: 62
                            radius: dialog.modernTheme ? 3 : 0
                            color: resultList.currentIndex === index ? dialog.selectedColor
                                : resultMouse.containsMouse ? Qt.tint(dialog.panelColor, "#16ffffff") : "transparent"
                            border.width: resultList.currentIndex === index ? 1 : 0
                            border.color: dialog.accent
                            Image {
                                anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                                width: 43; height: 43
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: dialog.spriteSource(resultRow.modelData)
                            }
                            Column {
                                anchors { left: parent.left; leftMargin: 58; right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                                spacing: 2
                                Text {
                                    width: parent.width
                                    text: resultRow.modelData.name || "Unknown item"
                                    color: dialog.foreground
                                    font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: "Server " + resultRow.modelData.serverId + "   Client " + resultRow.modelData.clientId
                                    color: dialog.secondary
                                    font.family: "Segoe UI"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: resultRow.modelData.group
                                    color: dialog.secondary
                                    font.family: "Segoe UI"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                id: resultMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: resultList.currentIndex = resultRow.index
                                onDoubleClicked: { resultList.currentIndex = resultRow.index; dialog.acceptSelection(); }
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 20
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            visible: dialog.results.length === 0
                            text: "No matches for your search."
                            color: dialog.secondary
                            font.family: "Segoe UI"; font.pixelSize: 12
                        }
                        DmeScrollBar { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } flickable: resultList }
                    }
                }
            }
        }

        Separator {}
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            DmeButton {
                objectName: "findItemAccept"
                text: "OK"
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                enabled: resultList.currentIndex >= 0
                variant: "primary"
                onClicked: dialog.acceptSelection()
            }
            DmeButton {
                objectName: "findItemCancel"
                text: "Cancel"
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                onClicked: dialog.close()
            }
        }
    }

    component FieldLabel: Text {
        color: dialog.secondary
        font.family: "Segoe UI"; font.pixelSize: 12
    }
    component SectionLabel: Text {
        color: dialog.foreground
        font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.DemiBold
    }
    component Separator: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: dialog.lineColor
    }
    component SectionPanel: Rectangle {
        radius: dialog.modernTheme ? 5 : 0
        color: dialog.panelColor
        border.color: dialog.lineColor
    }
    component ModeButton: Controls.Button {
        id: modeButton
        property string mode: ""
        property string labelText: ""
        text: "Find by " + labelText
        checked: dialog.searchMode === mode
        Accessible.name: text
        onClicked: dialog.chooseMode(mode)
        padding: 10
        background: Rectangle {
            radius: dialog.modernTheme ? 4 : 0
            color: modeButton.checked ? dialog.selectedColor : dialog.panelColor
            border.color: modeButton.checked ? dialog.accent : modeButton.hovered ? dialog.secondary : dialog.lineColor
        }
        contentItem: RowLayout {
            spacing: 7
            Rectangle {
                width: 12; height: 12; radius: 6
                color: "transparent"
                border.color: modeButton.checked ? dialog.accent : dialog.secondary
                Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; visible: modeButton.checked; color: dialog.accent }
            }
            Column {
                Layout.fillWidth: true
                Text { text: "Find by"; color: dialog.secondary; font.family: "Segoe UI"; font.pixelSize: 11 }
                Text { width: parent.width; text: modeButton.labelText; color: dialog.foreground; font.family: "Segoe UI"; font.pixelSize: 12; elide: Text.ElideRight }
            }
        }
    }
    component TypeChoice: Controls.AbstractButton {
        id: choice
        property string typeValue: ""
        enabled: dialog.searchMode === "type"
        checked: dialog.selectedType === typeValue
        height: 26
        opacity: enabled ? 1 : 0.6
        onClicked: dialog.chooseType(typeValue)
        contentItem: RowLayout {
            spacing: 7
            Rectangle {
                width: 12; height: 12; radius: 6
                color: "transparent"
                border.color: choice.checked ? dialog.accent : dialog.secondary
                Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; visible: choice.checked; color: dialog.accent }
            }
            Text { Layout.fillWidth: true; text: choice.text; color: dialog.foreground; font.family: "Segoe UI"; font.pixelSize: 11; wrapMode: Text.WordWrap }
        }
    }
    component PropertyCheck: Controls.AbstractButton {
        id: propertyCheck
        property string propertyKey: ""
        objectName: "findItemProperty_" + propertyKey
        width: (propertyGrid.width - propertyGrid.columnSpacing) / 2
        height: 32
        checkable: true
        enabled: dialog.propertyMode
        opacity: enabled ? 1 : 0.6
        onClicked: dialog.runSearch()
        contentItem: RowLayout {
            spacing: 7
            Rectangle {
                width: 13; height: 13; radius: dialog.modernTheme ? 2 : 0
                color: propertyCheck.checked ? dialog.accent : "transparent"
                border.color: propertyCheck.checked ? dialog.accent : dialog.secondary
                Text { anchors.centerIn: parent; text: "\u2713"; visible: propertyCheck.checked; color: dialog.windowsClassic ? "white" : "#10202a"; font.pixelSize: 11 }
            }
            Text { Layout.fillWidth: true; text: propertyCheck.text; color: dialog.foreground; font.family: "Segoe UI"; font.pixelSize: 11; wrapMode: Text.WordWrap }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    property int selectedRow: -1
    property string originalName: ""
    property string statusText: ""
    property bool statusError: false

    title: "Monster and NPC Manager"
    width: 700

    function clearEditor() {
        selectedRow = -1;
        originalName = "";
        creatureList.currentIndex = -1;
        nameField.text = "";
        npcCheck.checked = false;
        lookTypeField.value = 0;
        lookItemField.value = 0;
        headField.value = 0;
        bodyField.value = 0;
        legsField.value = 0;
        feetField.value = 0;
    }

    function selectRow(row) {
        const creature = Backend.creatureStore.creatureAt(row);
        if (!creature.name)
            return;
        selectedRow = row;
        creatureList.currentIndex = row;
        originalName = creature.name;
        nameField.text = creature.name;
        npcCheck.checked = creature.isNpc;
        lookTypeField.value = creature.lookType;
        lookItemField.value = creature.lookItem;
        headField.value = creature.lookHead;
        bodyField.value = creature.lookBody;
        legsField.value = creature.lookLegs;
        feetField.value = creature.lookFeet;
    }

    function saveCurrent() {
        if (nameField.text.trim().length === 0) {
            statusError = true;
            statusText = "Enter a creature name.";
            return;
        }
        const saved = Backend.creatureStore.saveCreature(
                    originalName, nameField.text, npcCheck.checked,
                    lookTypeField.value, lookItemField.value,
                    headField.value, bodyField.value,
                    legsField.value, feetField.value);
        statusError = !saved;
        statusText = saved ? "Creature saved."
                           : (Backend.creatureStore.errorString || "Could not save the creature.");
        if (saved) {
            originalName = nameField.text.trim();
            const row = Backend.creatureStore.rowForName(originalName);
            selectRow(row);
        }
    }

    onOpened: {
        statusText = "";
        clearEditor();
        if (Backend.creatureStore.count > 0)
            selectRow(0);
    }

    FileDialog {
        id: importDialog
        title: "Import monsters.xml or a monster/NPC XML file"
        fileMode: FileDialog.OpenFile
        nameFilters: ["XML files (*.xml)", "All files (*)"]
        onAccepted: {
            const result = Backend.creatureStore.importOtFile(selectedFile);
            dialog.statusError = result.success !== true;
            dialog.statusText = result.success
                    ? "Imported " + result.imported + " creature(s)"
                      + (result.failed > 0 ? "; " + result.failed + " file(s) failed." : ".")
                    : (result.error || "Import failed.");
            if (result.success && Backend.creatureStore.count > 0)
                dialog.selectRow(0);
        }
    }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 12

            Column {
                width: 280
                spacing: 6

                DmePanel {
                    width: parent.width
                    height: 350

                    ListView {
                        id: creatureList
                        anchors.fill: parent
                        anchors.margins: 2
                        anchors.rightMargin: 14
                        clip: true
                        model: Backend.creatureStore
                        highlightMoveDuration: 0

                        delegate: Rectangle {
                            id: creatureRow
                            required property int index
                            required property string name
                            required property bool isNpc
                            required property int lookType

                            width: creatureList.width
                            height: 30
                            color: creatureList.currentIndex === index
                                   ? "#505050"
                                   : (rowMouse.containsMouse ? "#383838" : "transparent")

                            Image {
                                anchors {
                                    left: parent.left
                                    leftMargin: 4
                                    verticalCenter: parent.verticalCenter
                                }
                                width: 26
                                height: 26
                                smooth: false
                                fillMode: Image.PreserveAspectFit
                                source: {
                                    const preview = Backend.datReader.outfitPreview(creatureRow.lookType);
                                    return preview.ids !== undefined && preview.ids.length > 0
                                            ? Backend.sprReader.itemImageSource(
                                                  preview.ids, preview.width, preview.height, 1)
                                            : "";
                                }
                            }

                            Text {
                                anchors {
                                    left: parent.left
                                    leftMargin: 36
                                    right: parent.right
                                    rightMargin: 6
                                    verticalCenter: parent.verticalCenter
                                }
                                text: creatureRow.name + (creatureRow.isNpc ? "  (NPC)" : "")
                                color: "#c0c0c0"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: dialog.selectRow(creatureRow.index)
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
                        flickable: creatureList
                    }
                }

                Row {
                    spacing: 6
                    DmeButton {
                        text: "New"
                        width: 86
                        onClicked: dialog.clearEditor()
                    }
                    DmeButton {
                        text: "Delete"
                        width: 86
                        enabled: dialog.originalName.length > 0
                        variant: "danger"
                        onClicked: {
                            const removed = Backend.creatureStore.removeCreature(
                                              dialog.originalName);
                            dialog.statusError = !removed;
                            dialog.statusText = removed ? "Creature removed."
                                                        : "Could not remove the creature.";
                            dialog.clearEditor();
                            if (Backend.creatureStore.count > 0)
                                dialog.selectRow(0);
                        }
                    }
                    DmeButton {
                        text: "Import..."
                        width: 96
                        onClicked: importDialog.open()
                    }
                }
            }

            Column {
                width: 370
                spacing: 6

                Text { text: "Name"; color: "#999"; font.pixelSize: 11 }
                DmeTextField {
                    id: nameField
                    width: parent.width
                    placeholderText: "Creature name"
                }

                DmeCheckBox {
                    id: npcCheck
                    text: "NPC"
                    onClicked: checked = !checked
                }

                Text {
                    text: "Outfit (use Look Type, or Look Item for an item outfit)"
                    color: "#999"
                    font.pixelSize: 11
                }
                Row {
                    spacing: 8
                    Column {
                        spacing: 3
                        Text { text: "Look Type"; color: "#777"; font.pixelSize: 10 }
                        DmeSpinBox {
                            id: lookTypeField
                            width: 130
                            from: 0
                            to: 65535
                        }
                    }
                    Column {
                        spacing: 3
                        Text { text: "Look Item"; color: "#777"; font.pixelSize: 10 }
                        DmeSpinBox {
                            id: lookItemField
                            width: 130
                            from: 0
                            to: 65535
                        }
                    }
                }

                Text { text: "Outfit colors"; color: "#999"; font.pixelSize: 11 }
                Grid {
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 5

                    Text { text: "Head"; color: "#777"; width: 70; font.pixelSize: 11 }
                    DmeSpinBox { id: headField; width: 110; from: 0; to: 255 }
                    Text { text: "Body"; color: "#777"; width: 70; font.pixelSize: 11 }
                    DmeSpinBox { id: bodyField; width: 110; from: 0; to: 255 }
                    Text { text: "Legs"; color: "#777"; width: 70; font.pixelSize: 11 }
                    DmeSpinBox { id: legsField; width: 110; from: 0; to: 255 }
                    Text { text: "Feet"; color: "#777"; width: 70; font.pixelSize: 11 }
                    DmeSpinBox { id: feetField; width: 110; from: 0; to: 255 }
                }

                Item { width: 1; height: 10 }

                DmeButton {
                    text: dialog.originalName.length > 0 ? "Save Changes" : "Add Creature"
                    width: 130
                    onClicked: dialog.saveCurrent()
                }
            }
        }

        Text {
            width: parent.width
            visible: dialog.statusText.length > 0
            text: dialog.statusText
            color: dialog.statusError ? "#f85149" : "#7ee787"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        DmeButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Close"
            width: 90
            onClicked: dialog.close()
        }
    }
}

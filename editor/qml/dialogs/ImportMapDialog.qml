import QtQuick
import QtQuick.Dialogs
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property string sourcePath: ""
    property string resultText: ""
    property bool resultError: false

    title: "Import Map"
    width: 470

    onOpened: {
        sourcePath = "";
        resultText = "";
        resultError = false;
        xOffset.value = 0;
        yOffset.value = 0;
        zOffset.value = 0;
        collisionMode.currentIndex = 1;
    }

    FileDialog {
        id: sourceFileDialog
        title: "Select map to import"
        fileMode: FileDialog.OpenFile
        nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
        onAccepted: dialog.sourcePath = Backend.fileTools.toLocalFile(selectedFile)
    }

    contentItem: Column {
        spacing: 10

        Text {
            text: "Map file"
            color: "#999"
            font.pixelSize: 11
        }

        Row {
            spacing: 6

            DmeTextField {
                width: 340
                text: dialog.sourcePath
                placeholderText: "Select an .otbm file"
                onEditingFinished: dialog.sourcePath = text
            }

            DmeButton {
                text: "Browse..."
                width: 90
                onClicked: sourceFileDialog.open()
            }
        }

        Text {
            text: "Import offset"
            color: "#999"
            font.pixelSize: 11
        }

        Row {
            spacing: 6

            Text {
                text: "X"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: xOffset
                width: 120
                from: -65535
                to: 65535
            }

            Text {
                text: "Y"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: yOffset
                width: 120
                from: -65535
                to: 65535
            }

            Text {
                text: "Z"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: zOffset
                width: 70
                from: -15
                to: 15
            }
        }

        Text {
            text: "Import options"
            color: "#999"
            font.pixelSize: 11
        }

        DmeCheckBox {
            id: housesCheck
            text: "Import houses and towns"
            checked: true
            onClicked: checked = !checked
        }

        DmeCheckBox {
            id: spawnsCheck
            text: "Import creatures and spawns"
            checked: true
            onClicked: checked = !checked
        }

        Row {
            spacing: 8
            Text {
                text: "On collision"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeComboBox {
                id: collisionMode
                width: 190
                model: ["Skip existing tile", "Replace existing tile", "Merge tile contents"]
                currentIndex: 1
            }
        }

        Text {
            width: parent.width
            visible: dialog.resultText.length > 0
            text: dialog.resultText
            color: dialog.resultError ? "#f85149" : "#7ee787"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Import"
                width: 100
                variant: "primary"
                enabled: dialog.sourcePath.length > 0
                onClicked: {
                    const result = dialog.mapCtrl.importMap(
                                     dialog.sourcePath,
                                     xOffset.value,
                                     yOffset.value,
                                     zOffset.value,
                                     housesCheck.checked,
                                     spawnsCheck.checked,
                                     collisionMode.currentIndex);
                    dialog.resultError = result.success !== true;
                    if (dialog.resultError) {
                        dialog.resultText = result.error || "Unable to import map";
                    } else {
                        dialog.resultText = "Imported " + result.importedTiles
                                          + " tile(s), merged " + result.mergedTiles
                                          + ", discarded " + result.discardedTiles + ".";
                    }
                }
            }

            DmeButton {
                text: "Close"
                width: 90
                onClicked: dialog.close()
            }
        }
    }
}

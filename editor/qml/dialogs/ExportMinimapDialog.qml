pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property string outputPath: ""
    property string resultText: ""
    property bool resultError: false

    title: "Export Minimap"
    width: 490

    function modeName() {
        const modes = ["all", "ground", "current", "specific", "selection"];
        return modes[areaMode.currentIndex];
    }

    onOpened: {
        outputPath = "";
        resultText = "";
        resultError = false;
        areaMode.currentIndex = 2;
        floorNumber.value = mapCtrl.floor;
    }

    FileDialog {
        id: outputDialog
        title: "Export minimap as PNG"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG images (*.png)"]
        defaultSuffix: "png"
        onAccepted: dialog.outputPath = Backend.fileTools.toLocalFile(selectedFile)
    }

    contentItem: Column {
        spacing: 10

        Text {
            text: "Output file"
            color: "#999"
            font.pixelSize: 11
        }

        Row {
            spacing: 6

            DmeTextField {
                width: 360
                text: dialog.outputPath
                placeholderText: "Choose the destination .png file"
                onEditingFinished: dialog.outputPath = text
            }

            DmeButton {
                text: "Browse..."
                width: 90
                onClicked: outputDialog.open()
            }
        }

        Text {
            text: "Area"
            color: "#999"
            font.pixelSize: 11
        }

        Row {
            spacing: 8

            DmeComboBox {
                id: areaMode
                width: 260
                model: [
                    "All Floors",
                    "Ground Floor (7)",
                    "Current Floor",
                    "Specific Floor",
                    "Selected Area"
                ]
                currentIndex: 2
            }

            DmeSpinBox {
                id: floorNumber
                width: 90
                from: 0
                to: 15
                enabled: areaMode.currentIndex === 3
            }
        }

        Text {
            width: parent.width
            text: areaMode.currentIndex === 0
                  ? "One aligned PNG file will be created for every non-empty floor."
                  : areaMode.currentIndex === 4
                    ? "Only selected tiles are exported. Selection: "
                      + dialog.mapCtrl.selectionCount + " tile(s)."
                    : "The minimap uses one pixel per map tile."
            color: "#777"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
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
                text: "Export"
                width: 100
                enabled: dialog.outputPath.length > 0
                         && (areaMode.currentIndex !== 4
                             || dialog.mapCtrl.selectionCount > 0)
                onClicked: {
                    const result = dialog.mapCtrl.exportMinimap(
                                     dialog.outputPath,
                                     dialog.modeName(),
                                     floorNumber.value);
                    dialog.resultError = result.success !== true;
                    if (dialog.resultError) {
                        dialog.resultText = result.error || "Minimap export failed.";
                    } else {
                        dialog.resultText = "Exported " + result.count
                                + " file(s), " + result.width + " x "
                                + result.height + " pixels.\n"
                                + result.files.join("\n");
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

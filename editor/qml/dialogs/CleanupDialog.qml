import QtQuick
import Tibia 1.0
import "../style"

TibiaDialog {
    id: dialog

    required property var mapCtrl
    property string resultText: ""
    property bool resultError: false

    title: "Cleanup Map"
    width: 410

    onOpened: {
        resultText = "";
        resultError = false;
    }

    contentItem: Column {
        spacing: 10

        Text {
            width: parent.width
            text: "Choose the data that should be removed or repaired."
            color: "#c0c0c0"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        TibiaCheckBox {
            id: invalidItems
            text: "Remove items missing from the loaded OTB"
            checked: true
            onClicked: checked = !checked
        }

        TibiaCheckBox {
            id: emptyTiles
            text: "Remove empty tiles"
            checked: true
            onClicked: checked = !checked
        }

        TibiaCheckBox {
            id: invalidHouses
            text: "Clear tiles assigned to missing houses"
            checked: true
            onClicked: checked = !checked
        }

        TibiaCheckBox {
            id: duplicateUniqueIds
            text: "Clear duplicate unique IDs (keep the first)"
            checked: true
            onClicked: checked = !checked
        }

        TibiaCheckBox {
            id: unusedHouses
            text: "Remove house definitions without tiles"
            checked: false
            onClicked: checked = !checked
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

            TibiaButton {
                text: "Run Cleanup"
                width: 110
                variant: "danger"
                enabled: invalidItems.checked || emptyTiles.checked
                         || invalidHouses.checked || duplicateUniqueIds.checked
                         || unusedHouses.checked
                onClicked: {
                    const result = dialog.mapCtrl.cleanupMap(
                                     invalidItems.checked,
                                     emptyTiles.checked,
                                     invalidHouses.checked,
                                     duplicateUniqueIds.checked,
                                     unusedHouses.checked);
                    dialog.resultError = result.success !== true;
                    dialog.resultText = dialog.resultError
                            ? (result.error || "Cleanup failed")
                            : "Removed " + result.removedItems + " invalid item(s), "
                              + result.removedTiles + " empty tile(s), and cleared "
                              + result.clearedHouseTiles + " invalid house tile(s). "
                              + "Cleared " + result.clearedUniqueIds
                              + " duplicate unique ID(s) and removed "
                              + result.removedHouses + " unused house(s).";
                }
            }

            TibiaButton {
                text: "Close"
                width: 90
                onClicked: dialog.close()
            }
        }
    }
}

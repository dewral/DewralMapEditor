pragma ComponentBehavior: Bound
import QtQuick
import "../style"

DmeDialog {
    id: dialog
    objectName: "numberedZonesDialog"
    required property var mapView
    required property var mapReader
    property var results: []
    property int total: 0
    readonly property color fg: windowsClassic ? "#202020" : "#e0e0e0"
    readonly property color muted: windowsClassic ? "#595959" : "#b1b1b1"
    title: "Numbered zones"
    width: 580
    onAboutToShow: {
        zoneId.value = mapView.numberedZoneId;
        results = []; total = 0;
    }
    function search() {
        const response = mapView.findNumberedZones(zoneId.value, selectionOnly.checked);
        results = response.results; total = response.total;
    }
    function paint() {
        mapView.numberedZoneId = zoneId.value;
        mapView.eraseMode = false;
        mapView.activeZone = 64;
        mapView.showNumberedZones = true;
        close();
    }
    contentItem: Column {
        width: 540
        spacing: 12
        Text { text: "RME-ZONES"; color: dialog.fg; font { pixelSize: 14; bold: true } }
        DmeCheckBox {
            objectName: "zonesEnabled"
            text: "Save numbered zones in this map"
            checked: dialog.mapReader.numberedZonesEnabled
            onClicked: dialog.mapReader.numberedZonesEnabled = !checked
        }
        Text {
            width: parent.width
            text: dialog.mapReader.numberedZonesEnabled
                ? "Enabled for this map. Your server must support the RME-ZONES extension."
                : "Disabled: normal TFS output. Numbered zones are not written to the map file."
            color: "#dfa65a"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        Row {
            spacing: 12
            Text { anchors.verticalCenter: parent.verticalCenter; text: "Zone ID"; color: dialog.fg }
            DmeSpinBox { id: zoneId; objectName: "zoneId"; width: 125; from: 0; to: 65535; value: 1 }
            DmeButton {
                objectName: "paintZone"; text: zoneId.value === 0 ? "Clear zones brush" : "Paint zone"
                width: 160; variant: "primary"
                enabled: dialog.mapReader.numberedZonesEnabled
                onClicked: dialog.paint()
            }
        }
        Text {
            width: parent.width
            text: "Paint adds this ID without replacing other zones. Ctrl + drag removes only this ID. ID 0 clears all zones on a tile."
            color: dialog.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        DmeCheckBox {
            text: "Show numbered zones on the map"
            checked: dialog.mapView.showNumberedZones
            onClicked: dialog.mapView.showNumberedZones = !checked
        }
        Rectangle { width: parent.width; height: 1; color: "#414141" }
        Row {
            spacing: 12
            DmeCheckBox {
                id: selectionOnly; objectName: "zonesSelectionOnly"
                text: "Selection only"; onClicked: checked = !checked
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeButton { objectName: "findZones"; text: "Find zones"; width: 110; onClicked: dialog.search() }
        }
        Text {
            width: parent.width
            text: "ID 0 finds all zones. " + dialog.total + " tile(s)"
                  + (dialog.total > dialog.results.length ? " · showing first 1000" : "")
            color: dialog.muted; font.pixelSize: 12
        }
        Rectangle {
            width: parent.width; height: 188
            color: dialog.windowsClassic ? "#f4f4f4" : "#1b1c1e"
            border.color: "#414141"; radius: 4
            ListView {
                id: list; anchors.fill: parent; anchors.margins: 4; clip: true
                model: dialog.results
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    width: list.width; height: 34
                    color: hover.containsMouse ? "#344752" : "transparent"
                    Text {
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: "Zone " + row.modelData.zones + "    ·    " + row.modelData.x + ", " + row.modelData.y + ", " + row.modelData.z
                        color: dialog.fg; font.pixelSize: 12
                    }
                    MouseArea {
                        id: hover; anchors.fill: parent; hoverEnabled: true
                        onClicked: { dialog.mapView.centerOnPosition(row.modelData.x, row.modelData.y, row.modelData.z); dialog.close(); }
                    }
                }
                Text { anchors.centerIn: parent; visible: list.count === 0; text: "Find zones to jump to a tile"; color: dialog.muted; font.pixelSize: 12 }
            }
            DmeScrollBar { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } flickable: list }
        }
        DmeButton { text: "Close"; width: 100; anchors.right: parent.right; onClicked: dialog.close() }
    }
}

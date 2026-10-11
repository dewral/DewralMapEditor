import "../../../components"
import "../Colors.js" as Colors
import QtQuick
import QtQuick.Controls
import QtQuick.Window
Item {
    id: root
    property var model: []
    property int currentIndex: -1
    property bool paletteCategories: false
    readonly property string currentText: currentIndex >= 0 && currentIndex < model.length ? model[currentIndex] : ""
    readonly property bool open: popup.visible
    signal activated(int index)
    implicitWidth: 140
    implicitHeight: 23
    function categoryIcon(label) {
        const icons = {
            "Recent": "palette-recent", "Favorites": "palette-favorites",
            "Terrain Palette": "palette-terrain", "Doodad Palette": "palette-doodad",
            "Collection Palette": "palette-collection", "Door Palette": "Doors",
            "Item Palette": "palette-item", "RAW Palette": "palette-raw",
            "Creature Palette": "palette-creature", "House Palette": "palette-house",
            "My Palettes": "palette-custom"
        };
        return icons[label] || "palette-raw";
    }
    function iconSource(label, selected) {
        return "image://tibiaui/fluent-icon/" + categoryIcon(label) + "/"
                + String(Colors.c(selected ? "accent" : "muted")).replace("#", "");
    }
    Rectangle {
        anchors.fill: parent
        radius: root.paletteCategories ? 1 : 4
        color: Colors.c("base")
        border.width: 1
        border.color: root.open || mouse.containsMouse ? "#737d84" : Colors.c("border")
    }
    Image {
        visible: root.paletteCategories
        x: 10; width: 18; height: 18
        anchors.verticalCenter: parent.verticalCenter
        source: visible ? root.iconSource(root.currentText, true) : ""
        sourceSize: Qt.size(18, 18)
    }
    Text {
        anchors.left: parent.left; anchors.leftMargin: root.paletteCategories ? 36 : 6
        anchors.right: arrow.left; anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.currentText; color: Colors.c("text")
        font.family: "Segoe UI"; font.pixelSize: root.paletteCategories ? 13 : 12
        elide: Text.ElideRight
    }
    Text {
        id: arrow
        anchors.right: parent.right; anchors.rightMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        text: "\u25BE"; color: Colors.c("placeholder"); font.pixelSize: 12
    }
    MouseArea {
        id: mouse; anchors.fill: parent; hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: popup.visible = !popup.visible
    }
    Popup {
        id: popup
        objectName: "fluentComboPopup"
        y: root.height + (root.paletteCategories ? 3 : 0)
        width: root.width
        height: Math.min(root.paletteCategories ? 420 : 200, list.contentHeight + 2,
                         root.Window.window ? Math.max(80, root.Window.window.height - 90) : 420)
        padding: 1
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        modal: true; dim: false; focus: true
        onOpened: list.positionViewAtIndex(root.currentIndex, ListView.Contain)
        transformOrigin: Item.TopLeft
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.paletteCategories ? 130 : 0; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: root.paletteCategories ? 130 : 0; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: root.paletteCategories ? 90 : 0; easing.type: Easing.InQuad }
        }
        background: Rectangle {
            radius: root.paletteCategories ? 1 : 4
            color: root.paletteCategories ? Colors.c("popup") : Colors.c("surface")
            border.width: 1
            border.color: root.paletteCategories ? Colors.c("scrollThumb") : Colors.c("border")
        }
        contentItem: ListView {
            id: list; model: root.model; clip: true; focus: true
            currentIndex: root.currentIndex; keyNavigationEnabled: false
            Keys.forwardTo: [typeAhead]
            ScrollIndicator.vertical: ScrollIndicator {}
            ComboBoxTypeAhead { id: typeAhead; comboBox: root; popup: popup; view: list }
            delegate: Item {
                id: row
                required property int index
                required property string modelData
                objectName: "paletteCategoryRow" + index
                readonly property bool selected: root.currentIndex === index
                readonly property bool divider: root.paletteCategories && index > 0
                    && (modelData === "Terrain Palette" || modelData === "RAW Palette" || modelData === "My Palettes")
                width: list.width
                height: (root.paletteCategories ? 34 : 22) + (divider ? 9 : 0)
                Rectangle {
                    visible: row.divider
                    x: 8; y: 4; width: parent.width - 16; height: 1; color: Colors.c("separator")
                }
                Rectangle {
                    id: entryBackground
                    y: row.divider ? 9 : 0
                    width: parent.width; height: root.paletteCategories ? 34 : 22
                    radius: root.paletteCategories ? 0 : 4
                    color: row.selected && root.paletteCategories ? Qt.tint(Colors.c("popup"), "#244a9ec7")
                         : entry.containsMouse || list.currentIndex === row.index ? Colors.c("hover") : "transparent"
                    Rectangle {
                        visible: root.paletteCategories && row.selected
                        width: 3; height: parent.height - 10
                        anchors.verticalCenter: parent.verticalCenter; color: Colors.c("accent")
                    }
                    Image {
                        visible: root.paletteCategories
                        x: 10; width: 18; height: 18; anchors.verticalCenter: parent.verticalCenter
                        source: visible ? root.iconSource(row.modelData, row.selected) : ""
                        sourceSize: Qt.size(18, 18)
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: root.paletteCategories ? 38 : 6
                        anchors.right: parent.right; anchors.rightMargin: root.paletteCategories ? 30 : 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData; color: Colors.c("text"); elide: Text.ElideRight
                        font.family: "Segoe UI"; font.pixelSize: root.paletteCategories ? 13 : 12
                        font.bold: root.paletteCategories && row.selected
                    }
                    Text {
                        visible: root.paletteCategories && row.selected
                        anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                        text: "\u2713"; color: Colors.c("accent"); font.pixelSize: 17
                    }
                    MouseArea {
                        id: entry; anchors.fill: parent; hoverEnabled: true
                        onClicked: { root.currentIndex = row.index; root.activated(row.index); popup.close(); }
                    }
                }
            }
        }
    }
    ColorHighlight { targetItem: root; colorKeys: ["base", "border", "text", "placeholder", "surface", "hover", "accent", "muted", "separator", "popup", "scrollThumb"] }
}
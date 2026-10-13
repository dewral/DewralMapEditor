pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

ComboBox {
    id: control
    textRole: "label"; valueRole: "name"
    readonly property string currentName: currentIndex >= 0 && currentIndex < count ? String(currentValue || "") : ""
    readonly property bool currentUsable: currentIndex >= 0 && currentIndex < count && !!model[currentIndex].usable
    readonly property bool lightTheme: Backend.uiTheme.style === "windows-classic"
    readonly property color labelColor: lightTheme ? "#202020" : Colors.c("text")
    implicitHeight: 38; implicitWidth: 240
    function spriteSource(entry) {
        if (!entry || !entry.clientId || !Backend.sprReader.loaded) return "";
        const revision = Backend.sprReader.itemImagesRevision;
        return Backend.sprReader.itemImageSourceForClientId(entry.clientId);
    }
    background: Rectangle {
        radius: control.lightTheme ? 0 : 4
        color: control.lightTheme ? "#ffffff" : Colors.c("field")
        border.color: control.activeFocus ? Colors.c("accent") : control.lightTheme ? "#808080" : Colors.c("border")
    }
    indicator: Text {
        x: control.width - width - 9; y: (control.height - height) / 2
        text: "\u25be"; color: control.labelColor; font.pixelSize: 12
    }
    contentItem: Item {
        Image {
            id: selectedSprite; objectName: control.objectName + "SelectedSprite"
            x: 5; width: 32; height: 32; anchors.verticalCenter: parent.verticalCenter
            smooth: false; fillMode: Image.PreserveAspectFit
            source: control.spriteSource(control.currentIndex >= 0 ? control.model[control.currentIndex] : null)
        }
        Text {
            anchors.left: selectedSprite.right; anchors.leftMargin: 6
            anchors.right: parent.right; anchors.rightMargin: 22; anchors.verticalCenter: parent.verticalCenter
            text: control.displayText; color: control.labelColor; font.pixelSize: 12
            elide: Text.ElideRight; opacity: control.enabled ? 1 : 0.5
        }
    }
    delegate: ItemDelegate {
        id: entry
        required property int index
        required property var modelData
        objectName: control.objectName + "Row" + index
        width: control.popup.width - 2; height: 40
        highlighted: control.highlightedIndex === index
        background: Rectangle { color: entry.highlighted ? (control.lightTheme ? "#d9e8fb" : Colors.c("hover")) : "transparent" }
        contentItem: Item {
            Image {
                objectName: control.objectName + "Sprite" + entry.index
                x: 4; width: 32; height: 32; anchors.verticalCenter: parent.verticalCenter
                smooth: false; fillMode: Image.PreserveAspectFit; source: control.spriteSource(entry.modelData)
            }
            Text {
                x: 44; width: parent.width - 48; anchors.verticalCenter: parent.verticalCenter
                text: entry.modelData.label; color: control.labelColor; font.pixelSize: 12; elide: Text.ElideRight
            }
        }
    }
    popup: Popup {
        y: control.height; width: Math.max(260, control.width)
        height: Math.min(280, brushList.contentHeight + 2); padding: 1
        contentItem: ListView {
            id: brushList; clip: true; implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: control.lightTheme ? "#ffffff" : Colors.c("popup")
            border.color: control.lightTheme ? "#808080" : Colors.c("border")
            radius: control.lightTheme ? 0 : 4
        }
    }
}

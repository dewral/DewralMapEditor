import "../../../components"
import "../Colors.js" as Colors
import QtQuick
Item {
    id: root; property string text: ""; property string placeholderText: ""; signal accepted; signal editingFinished; signal userTextChanged(string value)
    implicitWidth: 140; implicitHeight: 22
    Rectangle { anchors.fill: parent; radius: 4; color: Colors.c("base"); border.width: 1; border.color: input.activeFocus ? Colors.c("accent") : Colors.c("border") }
    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: root.placeholderText; color: Colors.c("placeholder"); font.family: "Segoe UI"; font.pixelSize: 12; visible: input.text.length === 0 }
    TextInput { id: input; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; verticalAlignment: TextInput.AlignVCenter; color: Colors.c("text"); font.family: "Segoe UI"; font.pixelSize: 12; clip: true; selectByMouse: true; text: root.text; onTextEdited: root.userTextChanged(text); onAccepted: root.accepted(); onEditingFinished: root.editingFinished() }
    Binding { target: input; property: "text"; value: root.text; when: !input.activeFocus }
    MouseArea { anchors.fill: parent; onClicked: input.forceActiveFocus() }
    ColorHighlight { targetItem: root; colorKeys: ["base", "accent", "border", "placeholder", "text"] }
}

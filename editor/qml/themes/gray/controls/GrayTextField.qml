import QtQuick
Item {
    id: root; property string text: ""; property string placeholderText: ""; signal accepted; signal editingFinished; signal userTextChanged(string value)
    implicitWidth: 140; implicitHeight: 22
    Rectangle { anchors.fill: parent; radius: 6; color: "#111111"; border.width: 1; border.color: input.activeFocus ? "#C79A3B" : "#3A3A3A" }
    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: root.placeholderText; color: "#858585"; font.pixelSize: 12; visible: input.text.length === 0 }
    TextInput { id: input; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; verticalAlignment: TextInput.AlignVCenter; color: "#E5E5E5"; font.pixelSize: 12; clip: true; selectByMouse: true; text: root.text; onTextEdited: root.userTextChanged(text); onAccepted: root.accepted(); onEditingFinished: root.editingFinished() }
    Binding { target: input; property: "text"; value: root.text; when: !input.activeFocus }
    MouseArea { anchors.fill: parent; onClicked: input.forceActiveFocus() }
}

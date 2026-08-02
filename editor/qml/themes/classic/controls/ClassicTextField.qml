import QtQuick
import Tibia 1.0

Item {
    id: root
    property string text: ""
    property string placeholderText: ""
    signal accepted
    signal editingFinished
    signal userTextChanged(string value)
    implicitWidth: 140
    implicitHeight: 22

    BorderImage {
        anchors.fill: parent
        source: Backend.uiTheme.tex + "textedit.png"
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.placeholderText
        color: "#777"
        font.pixelSize: 12
        visible: input.text.length === 0
    }
    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        color: "#c0c0c0"
        font.pixelSize: 12
        clip: true
        selectByMouse: true
        text: root.text
        onTextEdited: root.userTextChanged(text)
        onAccepted: root.accepted()
        onEditingFinished: root.editingFinished()
    }
    Binding {
        target: input
        property: "text"
        value: root.text
        when: !input.activeFocus
    }
    MouseArea { anchors.fill: parent; onClicked: input.forceActiveFocus() }
}

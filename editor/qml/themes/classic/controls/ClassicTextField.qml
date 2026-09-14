import QtQuick
import Tibia 1.0

Item {
    id: root
    property string text: ""
    property string placeholderText: ""
    signal accepted
    signal editingFinished
    signal userTextChanged(string value)
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
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
        color: root.windowsClassic ? "#6d6d6d" : "#777"
        font.pixelSize: 12
        visible: input.text.length === 0
    }
    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        color: root.windowsClassic ? "#202020" : "#c0c0c0"
        selectionColor: root.windowsClassic ? "#0a64ad" : "#507050"
        selectedTextColor: "#ffffff"
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

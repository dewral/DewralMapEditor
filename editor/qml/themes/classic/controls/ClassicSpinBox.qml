import QtQuick
import Tibia 1.0

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    property var nextTabItem: null
    property var previousTabItem: null
    property var pasteHandler: null
    signal valueModified(int value)
    implicitWidth: 96
    implicitHeight: 22

    function focusEditor() { input.forceActiveFocus(); input.selectAll(); }

    function setValue(nextValue) {
        var clamped = Math.max(from, Math.min(to, nextValue));
        if (clamped !== value) {
            value = clamped;
            valueModified(value);
        }
    }

    BorderImage {
        anchors.fill: parent
        anchors.rightMargin: 12
        source: Backend.uiTheme.tex + "textedit.png"
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }
    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: "#c0c0c0"
        font.pixelSize: 12
        readOnly: !root.editable
        selectByMouse: true
        text: root.value
        validator: IntValidator { bottom: root.from; top: root.to }
        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) {
            if (event.matches(StandardKey.Paste) && root.pasteHandler) {
                event.accepted = root.pasteHandler();
            } else if (event.key === Qt.Key_Tab && root.nextTabItem) {
                root.nextTabItem.focusEditor(); event.accepted = true;
            } else if (event.key === Qt.Key_Backtab && root.previousTabItem) {
                root.previousTabItem.focusEditor(); event.accepted = true;
            }
        }
    }
    Binding { target: input; property: "text"; value: String(root.value); when: !input.activeFocus }
    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        width: 12
        Image {
            width: 10; height: 11; smooth: false
            source: upArea.pressed ? (Backend.uiTheme.tex + "spinbox_up_pressed.png")
                                   : (upArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_up_hover.png")
                                                           : (Backend.uiTheme.tex + "spinbox_up_idle.png"))
            MouseArea { id: upArea; anchors.fill: parent; hoverEnabled: true; onClicked: root.setValue(root.value + root.stepSize) }
        }
        Image {
            width: 10; height: 11; smooth: false
            source: downArea.pressed ? (Backend.uiTheme.tex + "spinbox_down_pressed.png")
                                     : (downArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_down_hover.png")
                                                               : (Backend.uiTheme.tex + "spinbox_down_idle.png"))
            MouseArea { id: downArea; anchors.fill: parent; hoverEnabled: true; onClicked: root.setValue(root.value - root.stepSize) }
        }
    }
}

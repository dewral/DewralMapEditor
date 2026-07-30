import Tibia 1.0
import QtQuick

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    signal valueModified
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    implicitWidth: 96
    implicitHeight: 22

    function clamp(v) {
        return Math.max(root.from, Math.min(root.to, v));
    }
    function setValue(v) {
        const c = clamp(v);
        if (c !== root.value) {
            root.value = c;
            root.valueModified();
        }
    }

    BorderImage {
        anchors.fill: parent
        anchors.rightMargin: 12
        visible: !root.githubUi
        source: (Backend.uiTheme.tex + "textedit.png")
        smooth: false
        border {
            left: 1
            right: 1
            top: 1
            bottom: 1
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.githubUi
        radius: 6
        color: "#343434"
        border {
            width: input.activeFocus ? 2 : 1
            color: input.activeFocus ? "#B8B8B8" : "#646464"
        }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: Backend.uiTheme.style === "github-dark" ? "#D6D6D6" : "#c0c0c0"
        font.pixelSize: 12
        readOnly: !root.editable
        selectByMouse: true
        text: root.value
        validator: IntValidator {
            bottom: root.from
            top: root.to
        }

        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
    }

    Binding {
        target: input
        property: "text"
        value: String(root.value)
        when: !input.activeFocus
    }

    Column {
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: 12
        Image {
            width: 10
            height: 11
            visible: !root.githubUi
            source: upArea.pressed ? (Backend.uiTheme.tex + "spinbox_up_pressed.png") : (upArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_up_hover.png") : (Backend.uiTheme.tex + "spinbox_up_idle.png"))
            smooth: false
            MouseArea {
                id: upArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value + root.stepSize)
            }
        }
        Item {
            width: 12
            height: 11
            visible: root.githubUi
            Rectangle {
                anchors.fill: parent
                radius: 3
                color: githubUpArea.containsMouse ? "#505050" : "transparent"
            }
            Text {
                anchors.centerIn: parent
                text: "\u2303"
                color: "#8A8A8A"
                font.pixelSize: 10
            }
            MouseArea {
                id: githubUpArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value + root.stepSize)
            }
        }
        Image {
            width: 10
            height: 11
            visible: !root.githubUi
            source: downArea.pressed ? (Backend.uiTheme.tex + "spinbox_down_pressed.png") : (downArea.containsMouse ? (Backend.uiTheme.tex + "spinbox_down_hover.png") : (Backend.uiTheme.tex + "spinbox_down_idle.png"))
            smooth: false
            MouseArea {
                id: downArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value - root.stepSize)
            }
        }
        Item {
            width: 12
            height: 11
            visible: root.githubUi
            Rectangle {
                anchors.fill: parent
                radius: 3
                color: githubDownArea.containsMouse ? "#505050" : "transparent"
            }
            Text {
                anchors.centerIn: parent
                text: "\u2304"
                color: "#8A8A8A"
                font.pixelSize: 10
            }
            MouseArea {
                id: githubDownArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.setValue(root.value - root.stepSize)
            }
        }
    }
}

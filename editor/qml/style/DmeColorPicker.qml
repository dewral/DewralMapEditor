import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: root
    property color selectedColor: "#4a9ec7"
    property real hue: selectedColor.hsvHue < 0 ? 0 : selectedColor.hsvHue
    width: 400
    title: "Choose color"
    onOpened: { hex.text = String(selectedColor).toUpperCase(); if (selectedColor.hsvHue >= 0) hue = selectedColor.hsvHue; }
    function choose(color) { selectedColor = color; if (selectedColor.hsvHue >= 0) hue = selectedColor.hsvHue; hex.text = String(selectedColor).toUpperCase(); }
    contentItem: Column {
        spacing: 14
        Rectangle {
            id: field
            objectName: "colorSaturationField"
            width: parent.width; height: 190
            radius: 4
            color: Qt.hsva(root.hue, 1, 1, 1)
            Rectangle {
                anchors.fill: parent
                radius: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#ffffff" }
                    GradientStop { position: 1; color: "#00ffffff" }
                }
            }
            Rectangle {
                anchors.fill: parent
                radius: 4
                gradient: Gradient {
                    GradientStop { position: 0; color: "#00000000" }
                    GradientStop { position: 1; color: "#ff000000" }
                }
            }
            Rectangle {
                width: 12; height: 12; radius: 6
                x: root.selectedColor.hsvSaturation * (field.width - 1) - width / 2
                y: (1 - root.selectedColor.hsvValue) * (field.height - 1) - height / 2
                color: "transparent"; border.width: 2; border.color: "white"
            }
            MouseArea {
                anchors.fill: parent
                function pick(mouse) {
                    root.choose(Qt.hsva(root.hue, Math.max(0, Math.min(1, mouse.x / width)),
                                        1 - Math.max(0, Math.min(1, mouse.y / height)), 1));
                }
                onPressed: mouse => pick(mouse)
                onPositionChanged: mouse => { if (pressed) pick(mouse); }
            }
        }
        Slider {
            objectName: "colorHueSlider"
            width: parent.width
            from: 0; to: 1
            value: root.hue
            background: Rectangle {
                x: parent.leftPadding; y: parent.topPadding + parent.availableHeight / 2 - height / 2
                width: parent.availableWidth; height: 8; radius: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#ff0000" }
                    GradientStop { position: 0.1667; color: "#ffff00" }
                    GradientStop { position: 0.3333; color: "#00ff00" }
                    GradientStop { position: 0.5; color: "#00ffff" }
                    GradientStop { position: 0.6667; color: "#0000ff" }
                    GradientStop { position: 0.8333; color: "#ff00ff" }
                    GradientStop { position: 1; color: "#ff0000" }
                }
            }
            handle: Rectangle {
                width: 16; height: 16; radius: 8
                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                color: Colors.c("surface"); border.color: "white"; border.width: 2
            }
            onMoved: {
                root.hue = value;
                root.choose(Qt.hsva(value, root.selectedColor.hsvSaturation, root.selectedColor.hsvValue, 1));
            }
        }
        Flow {
            width: parent.width; spacing: 6
            Repeater {
                model: ["#399ee8", "#48b883", "#dfa65a", "#d46b79", "#9173be", "#4a9ec7", "#ffffff", "#cccccc", "#888888", "#202020", "#ff4444", "#ff8800", "#ffee44", "#88dd44", "#44dddd", "#cc66ee"]
                Rectangle {
                    required property string modelData
                    width: 34; height: 24; radius: 4
                    color: modelData; border.color: Colors.c("border")
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { root.choose(parent.modelData); } }
                }
            }
        }
        Row {
            width: parent.width; spacing: 10
            Rectangle { width: 42; height: 32; radius: 4; color: root.selectedColor; border.color: Colors.c("border") }
            TextField {
                id: hex
                objectName: "colorHexField"
                width: parent.width - 52; height: 32
                text: String(root.selectedColor).toUpperCase()
                color: Colors.c("text")
                font.family: Colors.fontFamily
                validator: RegularExpressionValidator { regularExpression: /#[0-9a-fA-F]{6}/ }
                selectByMouse: true
                background: Rectangle { radius: 4; color: Colors.c("field"); border.color: hex.activeFocus ? Colors.c("accent") : Colors.c("border") }
                onTextEdited: {
                    if (acceptableInput) {
                        root.selectedColor = text;
                        if (root.selectedColor.hsvHue >= 0) root.hue = root.selectedColor.hsvHue;
                    }
                }
            }
        }
        Row {
            anchors.right: parent.right; spacing: 8
            DmeButton { width: 100; text: "Cancel"; onClicked: root.reject() }
            DmeButton { objectName: "colorApplyButton"; width: 100; text: "Apply"; enabled: hex.acceptableInput; variant: "primary"; onClicked: root.accept() }
        }
    }
}

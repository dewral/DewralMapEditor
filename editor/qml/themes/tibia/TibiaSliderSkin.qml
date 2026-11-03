import QtQuick
import QtQuick.Controls
import Tibia 1.0

// A shared native skin; the existing Slider continues owning values and input.
Item {
    id: skin
    objectName: "tibiaSliderSkin"
    required property Slider control
    readonly property bool active: Backend.uiTheme.style === "tibia-ui"
    anchors.fill: parent
    visible: active
    z: 2

    Binding { target: skin.control; property: "leftPadding"; value: 12; when: skin.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: skin.control; property: "rightPadding"; value: 12; when: skin.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: skin.control.background; property: "opacity"; value: 0; when: skin.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: skin.control.handle; property: "opacity"; value: 0; when: skin.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: skin.control.handle; property: "width"; value: Math.min(28,Math.max(0,skin.control.availableWidth)); when: skin.active; restoreMode: Binding.RestoreBindingOrValue }
    Binding { target: skin.control.handle; property: "height"; value: 12; when: skin.active; restoreMode: Binding.RestoreBindingOrValue }

    Image {
        objectName: "nativeSliderTrack"
        x: skin.control.leftPadding; y: Math.floor((skin.height-12)/2)
        width: Math.max(0,skin.control.availableWidth); height: 12
        source: Qt.resolvedUrl("images/scrollbar-texture-horizontal.png")
        fillMode: Image.Tile; smooth: false
    }
    BorderImage {
        objectName: "nativeSliderThumb"
        x: skin.control.leftPadding+skin.control.visualPosition*(skin.control.availableWidth-width)
        y: Math.floor((skin.height-12)/2)
        width: skin.control.handle.width; height: 12
        source: Qt.resolvedUrl("images/scrollbar-handle-horizontal.png")
        border { left: 6; right: 6; top: 0; bottom: 0 }
        horizontalTileMode: BorderImage.Repeat; smooth: false
    }
    Repeater {
        model: [-1,1]
        ToolButton {
            id: arrow
            required property int modelData
            objectName: modelData<0 ? "nativeSliderDecrease" : "nativeSliderIncrease"
            readonly property bool atStart: (modelData<0)!==skin.control.mirrored
            x: atStart ? 0 : skin.width-12
            y: Math.floor((skin.height-12)/2)
            width: 12; height: 12; padding: 0
            focusPolicy: Qt.NoFocus
            autoRepeat: true
            enabled: skin.control.enabled
            background: Image {
                source: Qt.resolvedUrl("images/scrollbar-button"+(arrow.atStart ? "left" : "right")+"-"+(arrow.down ? "down" : "up")+".png")
                smooth: false
            }
            contentItem: Item { }
            onClicked: {
                const previous=skin.control.value;
                if(modelData<0)skin.control.decrease();else skin.control.increase();
                if(skin.control.value!==previous)skin.control.moved();
            }
            Accessible.name: modelData<0 ? "Decrease value" : "Increase value"
        }
    }
}

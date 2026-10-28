import QtQuick
import Tibia 1.0

// Instantiate texture nodes only when this skin is selected.
Loader {
    id: skin
    property string kind: "panel"
    property bool pressed: false
    property bool hovered: false
    property bool selected: false
    property bool frameOnly: false
    active: Backend.uiTheme.style === "tibia-ui"
    visible: active
    sourceComponent: TibiaSurface {
        frameOnly: skin.frameOnly
        kind: skin.kind
        pressed: skin.pressed
        hovered: skin.hovered
        selected: skin.selected
    }
}

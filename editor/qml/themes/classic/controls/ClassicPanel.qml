import QtQuick
import Tibia 1.0

BorderImage {
    source: Backend.uiTheme.tex + "panel_flat.png"
    smooth: false
    border { left: 1; right: 1; top: 1; bottom: 1 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}

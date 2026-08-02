import QtQuick
import Tibia 1.0

BorderImage {
    property int topBorder: 27
    property url frameSource: Backend.uiTheme.tex + "popupwindow.png"
    source: frameSource
    smooth: false
    border { left: 6; right: 6; top: topBorder; bottom: 6 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}

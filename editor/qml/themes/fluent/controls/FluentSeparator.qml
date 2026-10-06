import "../../../components"
import "../Colors.js" as Colors
import QtQuick
Rectangle { id: root; property int orientation: Qt.Horizontal; implicitWidth: orientation === Qt.Horizontal ? 80 : 1; implicitHeight: orientation === Qt.Horizontal ? 1 : 80; color: Colors.c("border");
    ColorHighlight { targetItem: root; colorKeys: ["border"] }
}

import "../../../components"
import "../Colors.js" as Colors
import QtQuick
import "../../tibia" as TibiaSkin
Rectangle { id: root; radius: 6; color: Colors.c("surface"); border.width: 1; border.color: Colors.c("border");
    TibiaSkin.TibiaSkin { anchors.fill: parent }
    ColorHighlight { targetItem: root; colorKeys: ["surface", "border"] }
}

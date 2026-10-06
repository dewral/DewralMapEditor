import "../../../components"
import "../Colors.js" as Colors
import QtQuick
Rectangle { id: root; radius: 6; color: Colors.c("surface"); border.width: 1; border.color: Colors.c("border");
    ColorHighlight { targetItem: root; colorKeys: ["surface", "border"] }
}

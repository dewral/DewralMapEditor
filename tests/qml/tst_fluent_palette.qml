import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
Item {
    width: 400; height: 500
    QtObject { id: prefs; property string paletteViewMode: "grid"; property int iconSize: 66 }
    QtObject { id: app; property var settings: prefs; property int iconSizePx: prefs.iconSize }
    QtObject { id: map; property int brushServerId: 0; property string doodadBrush: ""; function doodadPreviewSource(id) { return "" } }
    Components.PaletteItemGrid { id: items; width: 300; height: 200; app: app; mapCtrl: map; filterModel: []; currentKind: "Terrain Palette"; githubUi: false }
    Components.DoodadPaletteGrid { id: doodads; y: 210; width: 300; height: 200; app: app; mapCtrl: map; itemIds: []; categoryName: ""; searchText: ""; githubUi: false }
    TestCase {
        name: "FluentPaletteLayout"
        when: windowShown
        function test_modesAndScale() {
            Backend.uiTheme.style = "fluent-dark";
            for (const panel of [items, doodads]) {
                const grid = findChild(panel, "paletteGrid");
                prefs.paletteViewMode = "grid";
                prefs.iconSize = 50;
                const small = grid.cellWidth;
                prefs.iconSize = 88;
                verify(grid.cellWidth > small, "Item scale must resize grid cells");
                prefs.paletteViewMode = "list";
                compare(grid.cellWidth, grid.width, "List rows must span the palette");
                panel.width = 220;
                compare(grid.cellWidth, grid.width, "List must follow palette resizing");
                verify(grid.cellHeight >= 48);
                prefs.paletteViewMode = "grid";
                verify(grid.cellWidth < grid.width);
            }
        }
    }
}

import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
Item {
    id: root
    width: 900; height: 720
    QtObject {
        id: prefs
        property int tooltipMinimumZoom: 25
        property int lightingMinimumZoom: 25
        property int creatureNamesMinimumZoom: 25
        property string paletteViewMode: "grid"
        property int iconSize: 66
        property int renderMaxFps: 60
        property bool vsyncEnabled: true
        property int undoLimit: 1000
        property bool autosaveEnabled: true
        property int autosaveIntervalMinutes: 3
    }
    QtObject {
        id: map
        property int visibleZoneMask: 29
        property color selectionColor: "#4a9ec7"
        property real selectionOpacity: 0.25
        property var zoneColors: ["#399ee8", "#48b883", "#dfa65a", "#d46b79", "#9173be"]
        property var zoneOpacities: [0.25,0.25,0.25,0.25]
        property real houseOpacity: 0.25
        property real tilesOpacity: 1
        property real itemsOpacity: 1
        property bool showHouses: true
        property bool showZones: true
        property bool showZonesAlways: true
        property bool showSpawns: true
        property bool showCreatures: true
        property bool showGrid: false
    }
    Component {
        id: dialogComponent
        Dialogs.PreferencesDialog { settings: prefs; mapView: map; mapRenderer: map }
    }
    TestCase {
        name: "PreferencesLayout"
        when: windowShown
        function test_interfaceScale_data() {
            return ["fluent-dark", "tibia-ui"].map(theme => ({tag:theme,theme:theme}));
        }
        function test_interfaceScale(data) {
            Backend.uiTheme.style=data.theme;
            Backend.uiTheme.uiScalePercent=100;
            Backend.uiTheme.activeUiScalePercent=100;
            const dialog=createTemporaryObject(dialogComponent,root);
            dialog.page=1; dialog.open();
            tryCompare(dialog,"opened",true);
            const combo=findChild(dialog,"interfaceScaleCombo");
            const notice=findChild(dialog,"interfaceScaleRestartNotice");
            verify(combo!==null && notice!==null);
            compare(combo.currentIndex,3);
            combo.currentIndex=1; combo.activated(1);
            compare(Backend.uiTheme.uiScalePercent,80);
            verify(notice.text.indexOf("Restart DME to apply 80%")>=0);
            compare(Backend.uiTheme.activeUiScalePercent,100);
            Backend.uiTheme.uiScalePercent=125;
            compare(combo.currentIndex,5);
            combo.currentIndex=3; combo.activated(3);
            verify(!Backend.uiTheme.uiScaleRestartRequired);
            verify(notice.text.indexOf("Restart DME to apply")<0);
            dialog.close();
        }
        function test_pages_stay_above_footer_data() {
            return ["fluent-dark", "tibia-ui"].map(theme => ({tag: theme, theme: theme}));
        }
        function test_pages_stay_above_footer(data) {
            Backend.uiTheme.style = data.theme;
            prefs.lightingMinimumZoom = 25;
            prefs.creatureNamesMinimumZoom = 25;
            map.selectionOpacity = 0.25;
            const dialog = createTemporaryObject(dialogComponent, root);
            verify(dialog !== null);
            dialog.height = 440;
            dialog.open();
            tryCompare(dialog, "visible", true);
            for (let page = 0; page < 6; ++page) {
                dialog.page = page;
                wait(20);
                verify(dialog.contentItem.clip);
                verify(dialog.contentItem.height <= dialog.height - dialog.footer.height - dialog.header.height);
                const zones = findChild(dialog, "preferencesZoneDisplay");
                if (page === 4) verify(zones.height <= dialog.contentItem.height);
                if (page === 5) {
                    const hotkeys = findChild(dialog, "preferencesHotkeys");
                    verify(hotkeys.visible);
                    verify(hotkeys.height <= dialog.contentItem.height);
                }
            }
            const slider = findChild(dialog, "tooltipMinimumZoomSlider");
            verify(slider !== null);
            compare(slider.handle.width, data.theme === "tibia-ui" ? 28 : 16);
            const opacity = findChild(dialog, "selectionOpacitySlider");
            verify(opacity !== null);
            opacity.value = 70;
            opacity.moved();
            compare(map.selectionOpacity, 0.7);
            const lighting = findChild(dialog, "lightingMinimumZoomSlider");
            verify(lighting !== null);
            compare(lighting.value, 25);
            prefs.lightingMinimumZoom = 0;
            tryCompare(lighting, "value", 0);
            const names = findChild(dialog,"creatureNamesMinimumZoomSlider");
            verify(names !== null); compare(names.value,25);
            prefs.creatureNamesMinimumZoom = 0;
            tryCompare(names,"value",0);
            names.value = 50; names.moved();
            compare(prefs.creatureNamesMinimumZoom,50);
            dialog.close();
        }
    }
}

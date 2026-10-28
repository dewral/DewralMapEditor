import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/style" as Styles
import "../../editor/qml/components" as Components
import "../../editor/qml/themes/tibia" as Skin
import "../../editor/qml/themes/fluent/Colors.js" as Colors

Item {
    id: scene
    width: 1200; height: 760
    property int backgroundClicks: 0
    property bool menuFpsVisible: false
    property int menuActionCalls: 0
    MouseArea { anchors.fill: parent; onClicked: scene.backgroundClicks++ }
    Skin.TibiaSkin { id: shell; anchors.fill: parent; kind: "window" }
    QtObject {
        id: map
        property bool torchOn: false
        property bool selectionMode: false
        property bool lassoMode: false
        property bool eraseMode: false
        property bool optionalBorderMode: false
        property bool automagic: true
        property bool showAnimations: false
        property int activeZone: 0
        property int selectionCount: 0
        property bool hasClipboard: false
        property string brushShape: "square"
        property int brushSize: 0
        property int lightAmbient: 143
        property int lightAmbientColor: 215
        function forceActiveFocus() {}
    }
    QtObject { id: prefs; property bool showIngamePreviewWindow: false; property string toolsDock: "palette"; property string brushSizeDock: "palette" }
    Text { x: 14; y: 7; text: "Dewral Map Editor Ă‚Â· Tibia-UI"; color: Colors.c("text"); font.family: "Consolas"; font.bold: true; font.pixelSize: 15 }
    Styles.DmeMenuBar {
        id: testMenuBar; x: 6; y: 32
        Styles.DmeMenu {
            id: testFileMenu; title: "File"
            Styles.DmeMenuItem { id: testOpenEntry; text: "Open map..." }
            Styles.DmeMenuItem { text: "Disabled"; enabled: false }
        }
        Styles.DmeMenu {
            id: testViewMenu; title: "View"
            Styles.DmeMenuItem { text: "Minimap" }
            Action {
                text: "Show FPS"; checkable: true; checked: scene.menuFpsVisible; shortcut: "Ctrl+Shift+F"
                onTriggered: { scene.menuFpsVisible=!scene.menuFpsVisible; scene.menuActionCalls++; }
            }
            Action { text: "In-game preview window"; shortcut: "Ctrl+Shift+P" }
            Styles.DmeMenu { title: "Options"; Styles.DmeMenuItem { text: "Details" } }
            Action { text: "Unavailable action"; enabled: false; onTriggered: scene.menuActionCalls++ }
        }
        Styles.DmeMenu { id: testToolsMenu; title: "Tools"; Styles.DmeMenuItem { text: "Brush manager" } }
    }
    Components.FluentFileBar { id: toolbar; y: 65; width: scene.width; mapView: map; settings: prefs }
    Styles.DmePanel {
        x: 8; y: 116; width: 320; height: 630
        Text { x: 12; y: 8; text: "Palette"; color: Colors.c("text"); font.family: "Consolas"; font.bold: true; font.pixelSize: 14 }
        Styles.DmeComboBox { id: category; x: 12; y: 38; width: 296; height: 30; paletteCategories: true; model: ["RAW Palette", "Terrain Palette", "Creature Palette"]; currentIndex: 0 }
        Text { x: 12; y: 78; text: "Tileset"; color: Colors.c("text") }
        Styles.DmeComboBox { x: 12; y: 98; width: 296; height: 28; model: ["Others", "Architecture"]; currentIndex: 0 }
        Styles.DmeTextField { id: search; x: 12; y: 134; width: 296; height: 28; placeholderText: "Search items or ID..." }
        Skin.TibiaSurface { x: 12; y: 176; width: 296; height: 230; kind: "field" }
        Components.FluentTools { id: tools; x: 12; y: 420; width: 296; mapView: map }
        Components.PaletteBrushSizeSelector { id: brush; x: 12; y: 550; width: 296; mapCtrl: map; githubUi: false }
    }
    Rectangle { x: 340; y: 150; width: scene.width-350; height: 596; color: "#0f141b" }
    Skin.TibiaSkin { x: 340; y: 150; width: scene.width-350; height: 596; frameOnly: true; kind: "window" }
    Styles.DmeButton { id: tab; x: 340; y: 118; width: 180; height: 30; text: "woodenCity.otbm  *"; checked: true }
    Column {
        x: 380; y: 200; spacing: 18
        Text { text: "Tibia-UI controls"; color: Colors.c("heading"); font.pixelSize: 16 }
        Styles.DmeButton { id: action; width: 160; text: "Apply"; onClicked: action.checked=!action.checked }
        Styles.DmeCheckBox { id: check; text: "Show lower floors"; onClicked: checked=!checked }
        Styles.DmeSpinBox { id: number; from: 0; to: 255; value: 143 }
    }
    Skin.TibiaWindowFrame { id: floating; x: 750; y: 180; width: 300; height: 400; visible: false; caption: "Palette" }
    SignalSpy { id: dragSpy; target: floating; signalName: "dragStarted" }
    SignalSpy { id: closeSpy; target: floating; signalName: "closeRequested" }
    QtObject {
        id: tabDocuments
        property int currentIndex: 0
        property var tabs: [{title:"city.otbm",dirty:true},{title:"cave.otbm",dirty:false}]
    }
    QtObject { id: tabApp; property int closedTab: -1; function closeTab(index) { closedTab=index; } }
    Components.DocumentTabs { id: documentTabs; x:340; y:116; width:500; height:30; visible:false; app:tabApp; documents:tabDocuments }
    Skin.TibiaClassicFrame { id: applicationFrame; visible: false; kind: "application"; width: 240; height: 100 }
    Item {
        id: navigation
        width: 900; height: 600; visible: false
        property int tileSize: 32
        property int floor: 7
        property real ox: 30
        property real oy: 40
        property QtObject otbm: QtObject { property int width: 2048; property int height: 2048 }
        signal contentUpdated()
        function renderOriginX() { return ox; }
        function renderOriginY() { return oy; }
        function centerOnTile(x,y,z) { ox=x-width/(2*tileSize); oy=y-height/(2*tileSize); contentUpdated(); }
    }
    Skin.TibiaMapScrollBar { id: horizontalScroll; x: 750; y: 620; width: 300; height: 12; horizontal: true; mapView: navigation }
    Skin.TibiaMapScrollBar { id: verticalScroll; x: 1100; y: 200; width: 12; height: 300; mapView: navigation }
    Flickable { id: scrollContent; visible: false; width: 100; height: 200; contentHeight: 1000 }
    Styles.DmeScrollBar { id: paletteScroll; x: 1140; y: 200; height: 300; flickable: scrollContent }
    Styles.GithubWindowButton { id: captionButton; x: 700; y: 5 }
    SignalSpy { id: captionSpy; target: captionButton; signalName: "triggered" }
    TestCase {
        name: "TibiaTheme"; when: windowShown
        function init() { scene.menuFpsVisible=false; scene.menuActionCalls=0; testFileMenu.close(); testViewMenu.close(); testToolsMenu.close(); Backend.uiTheme.style="tibia-ui"; Backend.otbmReader.loaded=true; scene.Window.window.width=1200; scene.Window.window.height=760; wait(60); }
        function test_captionButtonAssetsAndActions() {
            const icon=findChild(captionButton,"tibiaCaptionButton");
            verify(icon!==null); compare(icon.width,18); compare(icon.height,18);
            const actions=["minimize","maximize","close"];
            const assets=["minimize","maximize","close"];
            for(let i=0;i<actions.length;i++) {
                captionButton.controlType=actions[i];
                tryVerify(()=>icon.source.toString().endsWith(assets[i]+".svg"));
                captionSpy.clear();
                mousePress(captionButton,12,13);
                tryVerify(()=>captionButton.pressed); compare(icon.anchors.verticalCenterOffset,1);
                mouseRelease(captionButton,12,13);
                compare(captionSpy.count,1);
            }
            captionButton.controlType="maximize"; captionButton.maximized=true;
            tryVerify(()=>icon.source.toString().endsWith("restore.svg"));
            captionButton.maximized=false;
            Backend.uiTheme.style="fluent-dark";
            tryCompare(icon,"visible",false);
        }
        function test_topbarLayoutAtWindowSizes() {
            const controls=findChild(toolbar,"topbarLightingControls");
            const slider=findChild(toolbar,"topbarBrightnessSlider");
            const fill=findChild(slider,"brightnessValueFill");
            for(const w of [800,1024,1200]) {
                toolbar.width=w; wait(220);
                verify(controls.x+controls.width<=toolbar.width);
                verify(controls.y+controls.height<=toolbar.implicitHeight);
                verify(slider.classicValueFill); verify(fill.visible);
            }
            toolbar.width=scene.width; wait(220);
            grabImage(toolbar).save("build/tibia-topbar-toolbar.png");
            const first=findChild(toolbar,"topbarCommand_new");
            verify(first!==null); compare(first.width,toolbar.commandWidth);
            Backend.uiTheme.style="fluent-dark";
            tryCompare(slider,"classicValueFill",false);
        }
        function test_clientMenuHighlightAndSwitching() {
            const file=findChild(testMenuBar,"menuHeading_File");
            const view=findChild(testMenuBar,"menuHeading_View");
            verify(file!==null); verify(view!==null);
            const background=findChild(file,"menuHeadingBackground");
            const pressed=findChild(file,"menuHeadingPressedFrame");
            mouseMove(scene,500,700); wait(30);
            mouseMove(file,file.width/2,file.height/2,40);
            tryCompare(background,"color","#606060");
            mouseClick(file);
            tryCompare(testFileMenu,"opened",true); tryCompare(pressed,"active",true);
            verify(pressed.item.pressed);
            compare(testFileMenu.padding,6); compare(testFileMenu.spacing,0);
            mouseMove(testOpenEntry,testOpenEntry.width/2,8);
            const entryBackground=findChild(testOpenEntry,"menuEntryBackground");
            tryCompare(entryBackground,"color","#606060"); compare(entryBackground.border.width,0);
            mouseMove(view,view.width/2,view.height/2);
            tryCompare(testViewMenu,"opened",true); tryCompare(testFileMenu,"opened",false);
            keyClick(Qt.Key_Escape); tryCompare(testViewMenu,"opened",false);
            tryCompare(pressed,"active",false);
            mouseMove(scene,600,700);
        }
        function test_menuHeadingHitAreaMatchesItsText() {
            const file=findChild(testMenuBar,"menuHeading_File");
            const toolsHeading=findChild(testMenuBar,"menuHeading_Tools");
            for(const heading of [file,toolsHeading]) {
                const popup=heading===file ? testFileMenu : testToolsMenu;
                mouseClick(heading,2,heading.height/2);
                tryCompare(popup,"opened",true);
                mouseClick(heading,heading.width-2,heading.height/2);
                tryCompare(popup,"opened",false);
                compare(heading.leftPadding,12); compare(heading.rightPadding,12);
                verify(heading.contentItem.x>=heading.leftPadding);
                verify(heading.contentItem.x+heading.contentItem.width<=heading.width-heading.rightPadding+0.1);
            }
        }
        function test_checkedMenuActionTogglesExactlyOnce() {
            const heading=findChild(testMenuBar,"menuHeading_View");
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            const entry=testViewMenu.itemAt(1);
            const check=findChild(entry,"classicMenuCheckBox");
            verify(check!==null && check.visible);
            verify(String(check.source).endsWith("checkbox-unchecked.png"));
            mouseClick(entry,12,entry.height/2);
            compare(scene.menuActionCalls,1); compare(scene.menuFpsVisible,true);
            tryCompare(testViewMenu,"opened",false);
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            verify(String(check.source).endsWith("checkbox-checked.png"));
            mouseClick(entry,80,entry.height/2);
            compare(scene.menuActionCalls,2); compare(scene.menuFpsVisible,false);
            tryCompare(testViewMenu,"opened",false);
        }
        function test_menuLabelsAndShortcutsFitWithoutClipping() {
            const heading=findChild(testMenuBar,"menuHeading_View");
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            const plain=findChild(testViewMenu.itemAt(0),"menuEntryLabel");
            const checkable=findChild(testViewMenu.itemAt(1),"menuEntryLabel");
            const longEntry=testViewMenu.itemAt(2);
            const label=findChild(longEntry,"menuEntryLabel");
            const shortcut=findChild(longEntry,"menuEntryShortcut");
            compare(plain.x,checkable.x); compare(label.style,Text.Normal);
            compare(label.font.bold,false); compare(label.font.family,"Verdana");
            compare(shortcut.text,"Ctrl+Shift+P");
            verify(label.x+label.width+12<=shortcut.x);
            verify(longEntry.implicitWidth<=testViewMenu.availableWidth+0.1);
            verify(longEntry.height>=20);
            const disabled=testViewMenu.itemAt(4);
            mouseClick(disabled,40,disabled.height/2); compare(scene.menuActionCalls,0);
            keyClick(Qt.Key_Escape); tryCompare(testViewMenu,"opened",false);
        }
        function test_menuKeyboardActivation() {
            const heading=findChild(testMenuBar,"menuHeading_View");
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            testViewMenu.currentIndex=1;
            keyClick(Qt.Key_Return);
            compare(scene.menuActionCalls,1); compare(scene.menuFpsVisible,true);
            tryCompare(testViewMenu,"opened",false);
        }
        function test_menuOtherThemesRetainGeometry_data() {
            return ["fluent-dark","classic","windows-classic","github-dark","gray-dark"].map(style=>({tag:style,style:style}));
        }
        function test_menuOtherThemesRetainGeometry(data) {
            Backend.uiTheme.style=data.style; wait(20);
            const heading=findChild(testMenuBar,"menuHeading_View");
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            const entry=testViewMenu.itemAt(1);
            compare(entry.implicitHeight,24);
            verify(!findChild(entry,"classicMenuCheckBox").visible);
            compare(testViewMenu.padding,data.style==="fluent-dark" ? 4 : 1);
            keyClick(Qt.Key_Escape); tryCompare(testViewMenu,"opened",false);
        }
        function test_menuVisualLayout() {
            scene.menuFpsVisible=true;
            const heading=findChild(testMenuBar,"menuHeading_View");
            mouseClick(heading,heading.width/2,heading.height/2);
            tryCompare(testViewMenu,"opened",true);
            mouseMove(scene,700,700); wait(80);
            grabImage(scene.Window.window.contentItem).save("build/tibia-menu-polished.png");
            keyClick(Qt.Key_Escape);
        }
        function test_nativeScrollbarSlices() {
            compare(paletteScroll.width,12); compare(paletteScroll.pixelStep,20);
            const handle=findChild(paletteScroll,"clientScrollbarHandle"); verify(handle!==null);
            compare(handle.border.left,0); compare(handle.border.top,6); compare(handle.border.bottom,6);
            const horizontal=findChild(horizontalScroll,"mapScrollSlider").handle;
            compare(horizontal.height,12); compare(horizontal.border.left,6); compare(horizontal.border.right,6); compare(horizontal.border.top,0);
            compare(horizontal.horizontalTileMode,BorderImage.Stretch);
            const vertical=findChild(verticalScroll,"mapScrollSlider").handle;
            compare(vertical.width,12); compare(vertical.border.top,6); compare(vertical.border.bottom,6);
            compare(vertical.verticalTileMode,BorderImage.Stretch);
        }
        function test_mapScrollbarNavigation() {
            navigation.ox=30; navigation.oy=40; navigation.contentUpdated();
            mouseClick(findChild(horizontalScroll,"mapScrollNext")); verify(navigation.ox>32);
            mouseClick(findChild(horizontalScroll,"mapScrollPrevious")); verify(Math.abs(navigation.ox-30)<1);
            mouseClick(findChild(verticalScroll,"mapScrollNext")); verify(navigation.oy>42);
            mouseClick(findChild(verticalScroll,"mapScrollPrevious")); verify(Math.abs(navigation.oy-40)<1);
            const slider=findChild(horizontalScroll,"mapScrollSlider");
            mouseClick(slider,slider.width*0.7,slider.height/2); verify(navigation.ox>100);
        }
        function test_brushSizesStayInOneRow() {
            brush.sectionFrame=true;
            for (const width of [320,256,200,160]) {
                brush.width=width; wait(20);
                const first=findChild(brush,"fluentBrushsquare");
                const last=findChild(brush,"brushRadius11");
                compare(first.mapToItem(brush,0,0).y,last.mapToItem(brush,0,0).y);
                tryVerify(()=>last.mapToItem(brush,last.width,0).x<=brush.width-8+0.01);
            }
            brush.width=256;
            mouseClick(findChild(brush,"brushRadius11")); compare(map.brushSize,11);
            mouseClick(findChild(brush,"fluentBrushsquare")); compare(map.brushShape,"circle");
            map.brushSize=0; map.brushShape="square";
            brush.sectionFrame=false; brush.width=296;
        }
        function test_compactToolsRetainCommands() {
            tools.compact=true; tools.width=489; wait(20);
            const doors=findChild(tools,"fluentToolDoors");
            tryVerify(()=>doors.mapToItem(tools,doors.width,0).x<=tools.width+0.01);
            map.activeZone=0;
            mouseClick(findChild(tools,"fluentToolNP")); compare(map.activeZone,4);
            mouseClick(findChild(tools,"fluentToolNP")); compare(map.activeZone,0);
            tools.compact=false; tools.width=296;
        }
        function test_popupPaletteUsesNativeCaption() {
            tryCompare(floating,"status",Image.Ready);
            verify(String(floating.source).endsWith("dialog-frame-borderimage.png"));
            compare(floating.border.left,4); compare(floating.border.right,4);
            compare(floating.border.top,17); compare(floating.border.bottom,4);
            compare(floating.captionHeight,17); compare(floating.contentTopMargin,25);
            compare(floating.smooth,false);
        }
        function test_toolsCaptionButtonsRemainClickable() {
            tools.sectionFrame=true; tools.collapsed=false; wait(20);
            const frame=findChild(tools,"toolsSectionFrame").item;
            verify(frame!==null);
            mouseClick(findChild(frame,"tibiaWindowCloseButton"),6,6);
            compare(tools.collapsed,true); compare(tools.implicitHeight,17);
            tools.collapsed=false; tools.sectionFrame=false;
        }
        function test_nativeDocumentTabsKeepNavigationAndClose() {
            documentTabs.visible=true; tabDocuments.currentIndex=0; tabApp.closedTab=-1; wait(20);
            const first=findChild(documentTabs,"documentTab0");
            const second=findChild(documentTabs,"documentTab1");
            compare(first.height,24); compare(second.height,24);
            mouseClick(second,12,12); compare(tabDocuments.currentIndex,1);
            const close=findChild(documentTabs,"tibiaDocumentClose1");
            compare(close.width,12); compare(close.height,12);
            mouseClick(close,6,6); compare(tabApp.closedTab,1);
            Backend.uiTheme.style="fluent-dark";
            tryCompare(close,"visible",false); compare(second.height,28);
            documentTabs.visible=false;
        }
        function test_greyApplicationFrame() {
            tryCompare(applicationFrame,"status",Image.Ready);
            verify(String(applicationFrame.source).indexOf("menubox.png")>=0);
            compare(applicationFrame.border.top,3); compare(applicationFrame.border.left,3);
        }
        function test_toolsMatchFluentDark() {
            const names=["Lasso", "Border", "PZ", "NP", "NL", "PvP", "Auto", "Doors"];
            for (const style of ["fluent-dark", "tibia-ui"]) {
                Backend.uiTheme.style=style; wait(20);
                for (const name of names) verify(findChild(tools,"fluentTool"+name)!==null);
                for (const name of ["Draw", "Erase", "Select"]) compare(findChild(tools,"fluentTool"+name),null);
                compare(findChild(tools,"tibiaMoreZones"),null);
                map.activeZone=0;
                for (const entry of [["PZ",1],["NP",4],["NL",8],["PvP",16]]) {
                    mouseClick(findChild(tools,"fluentTool"+entry[0])); compare(map.activeZone,entry[1]);
                    mouseClick(findChild(tools,"fluentTool"+entry[0])); compare(map.activeZone,0);
                }
            }
        }
        function test_clientWindowOwnsItsInput() {
            floating.visible=true; floating.locked=false;
            const before=scene.backgroundClicks;
            mouseClick(floating,150,300);
            compare(scene.backgroundClicks,before,"Floating frame must shield map clicks");
            dragSpy.clear();
            const handle=findChild(floating,"tibiaWindowDragHandle");
            mousePress(handle,80,8); mouseMove(handle,100,9); mouseRelease(handle,100,9);
            compare(dragSpy.count,1);
            floating.locked=true; dragSpy.clear();
            mousePress(handle,80,8); mouseMove(handle,100,9); mouseRelease(handle,100,9);
            compare(dragSpy.count,0);
            closeSpy.clear(); mouseClick(findChild(floating,"tibiaWindowCloseButton"));
            compare(closeSpy.count,1);
            floating.visible=false;
        }
        function test_texturesAndThemeSwitch() {
            verify(shell.active);
            const image=findChild(shell,"tibiaSurfaceImage");
            tryCompare(image,"status",Image.Ready);
            compare(image.smooth,false);
            verify(String(image.source).indexOf("menubox.png")>=0);
            compare(Colors.c("accent"),"#35b3e8");
            Backend.uiTheme.style="fluent-dark";
            tryCompare(shell,"active",false); compare(shell.item,null);
            compare(Colors.c("background"),"#202020");
            Backend.uiTheme.style="tibia-ui"; tryVerify(()=>shell.item!==null);
        }
        function test_controlsRemainInteractive() {
            action.checked=false; mouseClick(action); verify(action.checked);
            mouseClick(check); verify(check.checked);
            search.focusEditor(); keyClick(Qt.Key_D); keyClick(Qt.Key_O); keyClick(Qt.Key_O); keyClick(Qt.Key_R);
            compare(search.text,"door");
            mouseClick(category); tryCompare(category,"open",true); wait(180); keyClick(Qt.Key_T); keyClick(Qt.Key_Return); compare(category.currentIndex,1); tryCompare(category,"open",false);
            const lasso=findChild(tools,"fluentToolLasso");
            mouseClick(lasso); verify(map.lassoMode && map.selectionMode);
            mouseClick(lasso); verify(!map.selectionMode);
            grabImage(scene).save("build/tibia-ui-controls.png");
        }
    }
}

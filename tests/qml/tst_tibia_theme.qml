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
    property int newTabRequests: 0
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
    Components.DocumentTabs { id: documentTabs; onNewRequested: scene.newTabRequests++; x:340; y:116; width:500; height:30; visible:false; app:tabApp; documents:tabDocuments }
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
        function init() { captionButton.paletteCaption=false; scene.menuFpsVisible=false; scene.menuActionCalls=0; testFileMenu.close(); testViewMenu.close(); testToolsMenu.close(); Backend.uiTheme.style="tibia-ui"; Backend.otbmReader.loaded=true; scene.Window.window.width=1200; scene.Window.window.height=760; wait(60); }
        function test_classicLightColorPopup() {
            Backend.uiTheme.style="tibia-ui";
            toolbar.width=1200;
            const button=findChild(toolbar,"brightnessColorButton");
            mouseClick(button);
            const popup=findChild(toolbar,"lightingColorPopup");
            tryCompare(popup,"visible",true);
            compare(popup.width,346);
            const cell=findChild(popup.contentItem,"lightColorCell_190");
            verify(cell);
            compare(cell.width,16);
            wait(200);
            mouseClick(cell,8,8);
            compare(map.lightAmbientColor,190);
            wait(40);
            grabImage(scene.Window.window.contentItem).save("build/classic-timer-color/light-color-classic.png");
            keyClick(Qt.Key_Escape);
            tryCompare(popup,"visible",false);
        }
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
        function test_mainCaptionUsesStandardWindowActions() {
            captionButton.paletteCaption=true;
            const icon=findChild(captionButton,"tibiaCaptionButton");
            tryCompare(captionButton,"implicitWidth",24); compare(captionButton.implicitHeight,26);
            tryCompare(icon,"width",12); compare(icon.height,12); compare(icon.y,8);
            for (const name of ["minimize","maximize","close"]) {
                captionButton.controlType=name;
                tryVerify(()=>icon.source.toString().endsWith(name+".svg"));
                captionSpy.clear(); mousePress(captionButton,12,13);
                tryVerify(()=>captionButton.pressed); compare(icon.y,9);
                mouseRelease(captionButton,12,13); compare(captionSpy.count,1);
            }
            captionButton.controlType="maximize"; captionButton.maximized=true;
            tryVerify(()=>icon.source.toString().endsWith("restore.svg"));
            captionButton.maximized=false; captionButton.paletteCaption=false;
            tryCompare(icon,"width",18);
        }
        function test_toolbarBoxAndTransparentTabs() {
            const host=Qt.createQmlObject('import QtQuick; Rectangle {width:1200;height:200;color:"#4c3159";z:100}',scene);
            const toolbarParent=toolbar.parent, tabsParent=documentTabs.parent;
            const toolbarY=toolbar.y, tabsX=documentTabs.x, tabsY=documentTabs.y;
            try {
                toolbar.parent=host; toolbar.y=0; waitForRendering(toolbar);
                // The toolbar owns a MenuBox; empty document-tab space shares the shell.
                verify(!Qt.colorEqual(grabImage(toolbar).pixel(2,8),"#4c3159"));
                toolbar.visible=false;
                documentTabs.parent=host; documentTabs.x=0; documentTabs.y=0; documentTabs.visible=true;
                waitForRendering(documentTabs);
                verify(!Qt.colorEqual(grabImage(documentTabs).pixel(documentTabs.width-3,8),"#4c3159"));
                Backend.uiTheme.style="fluent-dark"; toolbar.visible=true; waitForRendering(toolbar);
                verify(!Qt.colorEqual(grabImage(toolbar).pixel(2,8),"#4c3159"));
            } finally {
                toolbar.parent=toolbarParent; toolbar.y=toolbarY; toolbar.visible=true;
                documentTabs.parent=tabsParent; documentTabs.x=tabsX; documentTabs.y=tabsY; documentTabs.visible=false;
                host.destroy();
            }
        }
        function test_nativeSliderArrowsDragAndThemeRestore() {
            const slider=findChild(toolbar,"topbarBrightnessSlider");
            const skin=findChild(slider,"tibiaSliderSkin");
            const less=findChild(slider,"nativeSliderDecrease");
            const more=findChild(slider,"nativeSliderIncrease");
            map.lightAmbient=100; waitForRendering(slider);
            mouseClick(more,6,6); compare(map.lightAmbient,101);
            mouseClick(less,6,6); compare(map.lightAmbient,100);
            map.lightAmbient=0;
            mouseClick(less,6,6); compare(map.lightAmbient,0);
            map.lightAmbient=255;
            mouseClick(more,6,6); compare(map.lightAmbient,255);
            map.lightAmbient=100;
            mousePress(slider,slider.handle.x+slider.handle.width/2,slider.height/2);
            mouseMove(slider,slider.width-22,slider.height/2,30);
            mouseRelease(slider,slider.width-22,slider.height/2);
            verify(map.lightAmbient>100);
            slider.forceActiveFocus();
            const before=map.lightAmbient; keyClick(Qt.Key_Left);
            compare(map.lightAmbient,before-1);
            Backend.otbmReader.loaded=false;
            mouseClick(more,6,6); compare(map.lightAmbient,before-1);
            Backend.otbmReader.loaded=true;
            Backend.uiTheme.style="fluent-dark"; waitForRendering(slider);
            compare(skin.visible,false); compare(slider.background.opacity,1);
            compare(slider.handle.opacity,1); compare(slider.handle.width,16);
            compare(slider.handle.height,16);
            map.lightAmbient=143;
        }
        function test_topbarLayoutAtWindowSizes() {
            const controls=findChild(toolbar,"topbarLightingControls");
            const slider=findChild(toolbar,"topbarBrightnessSlider");
            const fill=findChild(slider,"brightnessValueFill");
            for(const w of [800,1024,1200]) {
                toolbar.width=w; wait(220);
                verify(controls.x+controls.width<=toolbar.width,"lighting x="+controls.x+" width="+controls.width+" toolbar="+toolbar.width);
                verify(controls.y+controls.height<=toolbar.implicitHeight,"lighting y="+controls.y+" height="+controls.height+" toolbar="+toolbar.implicitHeight);
                verify(slider.classicValueFill);
                compare(slider.background.opacity,0);
                compare(slider.handle.height,12);
                verify(findChild(slider,"nativeSliderTrack").visible);
            }
            toolbar.width=scene.width; wait(220);
            grabImage(toolbar).save("build/tibia-topbar-toolbar.png");
            const first=findChild(toolbar,"topbarCommand_new");
            verify(first!==null); compare(first.width,toolbar.commandWidth);
            compare(toolbar.commandWidth,32); compare(toolbar.commandHeight,32);
            compare(toolbar.implicitHeight,50);
            compare(findChild(toolbar,"lightingButtonIcon").width,22);
            Backend.uiTheme.style="fluent-dark";
            tryCompare(slider,"classicValueFill",false);
        }
        function test_nativeToolbarGroupsAndDockedPanels() {
            const actions=findChild(toolbar,"topbarActionsPanel");
            const lighting=findChild(toolbar,"topbarLightingGroup");
            const dock=findChild(toolbar,"topbarDockedPanels");
            try {
                for(const docks of [["palette","palette"],["topbar","palette"],["palette","topbar"],["topbar","topbar"]]) {
                    prefs.brushSizeDock=docks[0];prefs.toolsDock=docks[1];
                    for(const width of [720,800,1040,1200,1600,1920]) {
                        toolbar.width=width;wait(240);
                        verify(actions.visible && lighting.active);
                        compare(actions.x,8);compare(actions.height,36);
                        verify(actions.active);
                        compare(actions.item.kind,lighting.item.kind);
                        compare(actions.item.source,lighting.item.source);
                        const icons=["new","open","save","saveAs","undo","redo","cut","copy","paste"].map(op=>findChild(toolbar,"topbarCommand_"+op));
                        icons.push(findChild(toolbar,"fluentIngamePreviewButton"));
                        icons.push(findChild(toolbar,"fluentAnimationButton"));
                        for(let i=1;i<icons.length;++i) {
                            const previousRight=icons[i-1].mapToItem(toolbar,icons[i-1].width,0).x;
                            compare(icons[i].mapToItem(toolbar,0,0).x-previousRight,8);
                        }
                        compare(findChild(toolbar,"topbarFilesGroup"),null);
                        compare(findChild(toolbar,"topbarEditGroup"),null);
                        compare(findChild(toolbar,"topbarPreviewGroup"),null);
                        compare(lighting.x+lighting.width,toolbar.width-8);
                        verify(lighting.y+lighting.height<=toolbar.implicitHeight);
                        if(toolbar.compactLighting)verify(lighting.y>=actions.y+actions.height);
                        else verify(lighting.x>=actions.x+actions.width);
                        if(toolbar.hasDockedPanels) {
                            verify(dock.width>0);verify(dock.clip);
                            verify(dock.y+dock.height<=toolbar.implicitHeight);
                            if(toolbar.secondRow)verify(dock.y>=lighting.y+lighting.height);
                            else {
                                verify(dock.x>=actions.x+actions.width);
                                verify(dock.x+dock.width<=lighting.x);
                                verify(dock.width>=toolbar.dockedWidth);
                            }
                        }
                    }
                }
                Backend.uiTheme.style="fluent-dark";
                tryCompare(lighting,"active",false);
                Backend.uiTheme.style="tibia-ui";toolbar.width=1200;wait(240);
                compare(lighting.x+lighting.width,toolbar.width-8);
                grabImage(toolbar).save("build/tibia-toolbar-icon-groups.png");
            } finally {
                prefs.brushSizeDock="palette";prefs.toolsDock="palette";
                toolbar.width=scene.width;wait(240);
            }
        }
        function test_clientMenuHighlightAndSwitching() {
            const file=findChild(testMenuBar,"menuHeading_File");
            const view=findChild(testMenuBar,"menuHeading_View");
            verify(file!==null); verify(view!==null);
            compare(testMenuBar.topPadding,3);compare(testMenuBar.bottomPadding,3);
            compare(file.mapToItem(testMenuBar,0,0).y,3);compare(file.height,testMenuBar.height-6);
            const background=findChild(file,"menuHeadingBackground");
            const pressed=findChild(file,"menuHeadingPressedFrame");
            mouseMove(scene,500,700); wait(30);
            mouseMove(file,file.width/2,file.height/2,40);
            tryCompare(background,"color","#10ffffff");
            mouseClick(file);
            tryCompare(testFileMenu,"opened",true); tryCompare(pressed,"active",true);
            verify(pressed.item.pressed);
            compare(background.color,"#00000000");
            compare(testFileMenu.padding,6); compare(testFileMenu.spacing,0);
            mouseMove(testOpenEntry,testOpenEntry.width/2,8);
            tryCompare(pressed,"active",false);
            compare(testFileMenu.opened,true);
            compare(background.color,"#00000000");
            const entryBackground=findChild(testOpenEntry,"menuEntryBackground");
            tryCompare(entryBackground,"color","#18ffffff"); compare(entryBackground.border.width,0);
            grabImage(scene).save("build/DME-client-profiles-verify/classic-menu-states.png");
            mouseMove(view,view.width/2,view.height/2);
            tryCompare(testViewMenu,"opened",true); tryCompare(testFileMenu,"opened",false);
            keyClick(Qt.Key_Escape); tryCompare(testViewMenu,"opened",false);
            tryCompare(pressed,"active",false);
            mouseMove(scene,600,700);
        }
        function test_menuHeadingsReleaseAfterPointerAndFocusChanges() {
            const headings=["File","View","Tools"].map(name=>findChild(testMenuBar,"menuHeading_"+name));
            const menus=[testFileMenu,testViewMenu,testToolsMenu];
            const frames=headings.map(h=>findChild(h,"menuHeadingPressedFrame"));
            mouseClick(headings[0]);tryCompare(menus[0],"opened",true);
            for(let i=0;i<headings.length;++i) {
                mouseMove(headings[i],headings[i].width/2,headings[i].height/2,30);
                tryCompare(menus[i],"opened",true);
                for(let j=0;j<frames.length;++j)tryCompare(frames[j],"active",j===i);
            }
            keyClick(Qt.Key_Escape);mouseMove(scene,600,700);wait(50);
            for(let i=0;i<headings.length;++i) {
                compare(frames[i].active,false);
                compare(findChild(headings[i],"menuHeadingBackground").color,"#00000000");
            }
            for(const heading of headings) {
                mouseMove(heading,heading.width/2,heading.height/2,30);
                for(const frame of frames)compare(frame.active,false);
                mouseMove(scene,600,700);wait(20);
                compare(findChild(heading,"menuHeadingBackground").color,"#00000000");
            }
        }
        function test_menuClosesAfterLeavingSubmenu() {
            const heading = findChild(testMenuBar, "menuHeading_View");
            mouseClick(heading);
            tryCompare(testViewMenu, "opened", true);
            const entry = testViewMenu.itemAt(3);
            mouseMove(entry, entry.width / 2, entry.height / 2);
            const submenu = entry.subMenu;
            tryCompare(submenu, "opened", true);
            const child = submenu.itemAt(0);
            mouseMove(child, child.width / 2, child.height / 2);
            wait(250);
            compare(submenu.opened, true);
            compare(testViewMenu.opened, true);
            mouseMove(scene, 600, 700);
            tryCompare(testViewMenu, "opened", false);
            tryCompare(submenu, "opened", false);
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
            mouseClick(findChild(brush,"fluentBrushcircle")); compare(map.brushShape,"circle");
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
        function test_brushSummaryTracksSizeAndShape() {
            brush.sectionFrame=true;brush.width=340;wait(20);
            const frame=findChild(brush,"brushSectionFrame").item;
            map.brushSize=0;map.brushShape="square";
            compare(brush.brushSummary,"1 × 1 · Square");
            compare(frame.caption,"Brush size");
            compare(findChild(brush,"brushSizeReadout"),null);
            mouseClick(findChild(brush,"brushRadius2"));
            compare(brush.brushSummary,"5 × 5 · Square");
            mouseClick(findChild(brush,"fluentBrushcircle"));
            compare(brush.brushSummary,"5 × 5 · Circle");
            compare(frame.caption,"Brush size");
            const square=findChild(brush,"fluentBrushsquare"), circle=findChild(brush,"fluentBrushcircle");
            verify(circle.active && !square.active);
            mouseClick(circle);compare(map.brushShape,"circle");
            mouseClick(square);compare(map.brushShape,"square");
            verify(square.active && !circle.active);
            mouseClick(circle);
            brush.compact=true;map.brushSize=6;
            compare(brush.brushSummary,"13 × 13 · Circle");
            brush.compact=false;brush.sectionFrame=false;brush.width=296;
            map.brushSize=0;map.brushShape="square";
        }
        function test_newDocumentTabButton() {
            documentTabs.visible=true;
            const count=scene.newTabRequests;
            const button=findChild(documentTabs,"newDocumentTabButton");
            verify(button); compare(button.height,24);
            mouseClick(button); compare(scene.newTabRequests,count+1);
            documentTabs.visible=false;
        }
        function test_nativeDocumentTabsKeepNavigationAndClose() {
            documentTabs.visible=true; tabDocuments.currentIndex=0; tabApp.closedTab=-1; wait(20);
            const first=findChild(documentTabs,"documentTab0");
            const second=findChild(documentTabs,"documentTab1");
            compare(first.height,24); compare(second.height,24);
            const firstSkin=findChild(first,"tibiaDocumentTabBackground0");
            const secondSkin=findChild(second,"tibiaDocumentTabBackground1");
            verify(String(firstSkin.source).endsWith("console-tab-active.png"));
            verify(String(secondSkin.source).endsWith("console-tab-passive.png"));
            mouseClick(second,12,9); compare(tabDocuments.currentIndex,1);
            verify(String(firstSkin.source).endsWith("console-tab-passive.png"));
            verify(String(secondSkin.source).endsWith("console-tab-active.png"));
            const close=findChild(documentTabs,"tibiaDocumentClose1");
            compare(close.width,12); compare(close.height,12);
            const closeTarget=findChild(documentTabs,"tibiaDocumentCloseHitArea1");
            compare(closeTarget.width,22);compare(closeTarget.height,24);
            mouseClick(closeTarget,1,1); compare(tabApp.closedTab,1);
            compare(tabDocuments.currentIndex,1);
            Backend.uiTheme.style="fluent-dark";
            tryCompare(close,"visible",false); compare(second.height,28);
            documentTabs.visible=false;
        }
        function test_greyApplicationFrame() {
            tryCompare(applicationFrame,"status",Image.Ready);
            verify(String(applicationFrame.source).indexOf("menubox.png")>=0);
            compare(applicationFrame.border.top,3); compare(applicationFrame.border.left,3);
        }
        function test_toolsUseNativeSvgWithoutLabels() {
            const names=["Lasso","Border","PZ","NP","NL","PvP","Auto","Doors"];
            for(const compact of [false,true]) {
                tools.compact=compact;tools.width=compact ? 297 : 296;wait(30);
                for(const name of names) {
                    const button=findChild(tools,"fluentTool"+name);
                    const icon=findChild(button,"toolIcon"+name);
                    tryCompare(icon,"status",Image.Ready);
                    verify(String(icon.source).endsWith("tool-"+name.toLowerCase()+".svg"));
                    verify(!findChild(button,"toolLabel"+name).visible);
                    compare(icon.width,compact ? 22 : 24);
                    verify(button.Accessible.name.length>0);
                    compare(button.ToolTip.text,button.Accessible.name);
                }
            }
            tools.compact=false;tools.sectionFrame=true;tools.width=360;waitForRendering(tools);
            compare(findChild(tools,"toolIconPZ").width,24);
            tools.sectionFrame=false;
            Backend.uiTheme.style="fluent-dark";wait(30);
            verify(findChild(tools,"toolLabelPZ").visible);
            tools.compact=false;tools.width=296;
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

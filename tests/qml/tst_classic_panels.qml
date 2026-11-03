import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/themes/tibia" as Skin
import "../../editor/qml/components" as Components
import "../../editor/qml/style" as Styles

Item {
    id: scene
    width: 1668; height: 895
    property int backgroundClicks: 0
    MouseArea { anchors.fill: parent; onClicked: scene.backgroundClicks++ }
    Rectangle { anchors.fill: parent; color: "#141414" }
    Skin.TibiaPopupWindow {
        id: popup; x: 8; y: 8; width: 320; height: implicitHeight
        caption: "Terrain Palette"; showCollapseButton: true
        onCollapseRequested: collapsed=!collapsed
        Column {
            width: parent.width; spacing: 6
            Styles.DmeTextField { width: parent.width; height: 28; placeholderText: "Search items or ID..." }
            Rectangle { width: parent.width; height: 180; color: "#272727" }
        }
    }
    Skin.TibiaPopupWindow {
        id: scrolling; x: 340; y: 8; width: 240; height: 160
        caption: "Scrollable tool options"; scrollable: true
        contentHeight: 420
        Rectangle { width: parent.width; height: 420; color: "#333333" }
    }
    Skin.TibiaPopupWindow {
        id: floating; x: 600; y: 8; width: 240; height: 160
        caption: "Floating tool"; floating: true; windowActive: false
    }
    QtObject {
        id: map
        property bool selectionMode: false
        property bool lassoMode: false
        property bool eraseMode: false
        property bool optionalBorderMode: false
        property bool automagic: true
        property int activeZone: 0
        property string brushShape: "square"
        property int brushSize: 0
    }
    Components.FluentTools { id: tools; x: 8; y: popup.y+popup.height+6; width: 320; height: implicitHeight; sectionFrame: true; mapView: map }
    Components.PaletteBrushSizeSelector { id: brush; x: 8; y: tools.y+tools.height+6; width: 320; height: implicitHeight; sectionFrame: true; githubUi: false; mapCtrl: map }
    Rectangle { id: viewport; x: 340; y: 200; width: scene.width-x-8; height: scene.height-y-8; color: "#11151a" }
    Skin.TibiaSurface { anchors.fill: viewport; kind: "strip"; frameOnly: true }
    Styles.DmeDialog {
        id: dialog; title: "Tool options"; width: 280; height: 160
        Styles.DmeTextField { id: field; width: 220; height: 28; placeholderText: "Tool name" }
    }
    SignalSpy { id: dragSpy; target: floating; signalName: "dragStarted" }
    SignalSpy { id: rejectedSpy; target: dialog; signalName: "rejected" }
    SignalSpy { id: closeSpy; target: popup; signalName: "closeRequested" }
    TestCase {
        name: "ClassicPanels"; when: windowShown
        function init() {
            Backend.uiTheme.style="tibia-ui";
            scene.Window.window.width=1668; scene.Window.window.height=895;
            popup.collapsed=false; popup.padding=8;
            scrolling.scrollable=true; scrolling.flickable.contentY=0;
            floating.locked=false; floating.floating=true;
            tools.collapsed=false; brush.collapsed=false; brush.compact=false;
            dialog.closePolicy=Popup.CloseOnEscape; dialog.close(); rejectedSpy.clear(); wait(50);
        }
        function test_intrinsicSizeAndCollapse() {
            compare(popup.height,247); compare(popup.contentTopMargin,25);
            const b=findChild(popup,"tibiaWindowCollapseButton");
            mouseClick(b,b.width/2,b.height/2);
            compare(popup.collapsed,true); compare(popup.height,17);
            verify(!popup.contentItem.visible);
            mouseClick(b,b.width/2,b.height/2);
            compare(popup.height,247); verify(popup.contentItem.visible);
            popup.padding=10; compare(popup.height,251);
        }
        function test_optionalScrollAndResize() {
            const bar=findChild(scrolling,"tibiaPopupScrollBar");
            verify(bar.visible); verify(scrolling.flickable.interactive);
            mouseWheel(scrolling,70,80,0,-120); tryVerify(()=>scrolling.flickable.contentY>0);
            scrolling.height=500; tryCompare(bar,"visible",false);
            verify(!scrolling.flickable.interactive);
            scrolling.height=160; scrolling.scrollable=false;
            verify(!bar.visible); verify(!scrolling.flickable.interactive);
        }
        function test_headerActionsAndInputShield() {
            const before=scene.backgroundClicks;
            closeSpy.clear(); mouseClick(popup,100,21); compare(scene.backgroundClicks,before);
            const close=findChild(popup,"tibiaWindowCloseButton");
            mouseClick(close,6,6); compare(closeSpy.count,1);
            dragSpy.clear(); mouseDrag(floating,80,8,20,10); compare(dragSpy.count,1);
            floating.locked=true; dragSpy.clear(); mouseDrag(floating,80,8,20,10); compare(dragSpy.count,0);
            const caption=findChild(floating,"tibiaWindowCaption");
            compare(caption.color,"#909090"); floating.windowActive=true; compare(caption.color,"#c0c0c0"); floating.windowActive=false;
        }
        function test_sectionCollapseRetainsBrushAndViewport() {
            const geometry=Qt.rect(viewport.x,viewport.y,viewport.width,viewport.height);
            map.brushSize=6;
            for (const section of [tools,brush]) {
                const b=findChild(section,"tibiaWindowCollapseButton"); verify(b!==null);
                mouseClick(b,7,7); compare(section.height,17);
                mouseClick(b,7,7); verify(section.height>17);
            }
            compare(map.brushSize,6);
            compare(Qt.rect(viewport.x,viewport.y,viewport.width,viewport.height),geometry);
            brush.compact=true; brush.collapsed=true; compare(brush.height,30);
            verify(findChild(brush,"brushRadius11").visible);
        }
        function test_dialogKeyboardAndPosition() {
            dialog.open(); tryVerify(()=>dialog.opened);
            field.focusEditor(); keyClick(Qt.Key_S); keyClick(Qt.Key_A); compare(field.text,"sa");
            keyClick(Qt.Key_Escape); tryVerify(()=>!dialog.visible);
            dialog.x=420; dialog.y=300; dialog.open(); tryVerify(()=>dialog.opened);
            compare(dialog.x,420); compare(dialog.y,300);
            const close=findChild(dialog.header,"tibiaWindowCloseButton"); verify(close!==null);
            rejectedSpy.clear(); mouseClick(close,6,6); tryVerify(()=>!dialog.visible); compare(rejectedSpy.count,1);
            field.text="";
        }
        function test_busyDialogCannotCloseFromCaption() {
            dialog.closePolicy=Popup.NoAutoClose; dialog.open(); tryVerify(()=>dialog.opened);
            const close=findChild(dialog.header,"tibiaWindowCloseButton");
            verify(!close.visible);
            keyClick(Qt.Key_Escape); verify(dialog.visible);
            dialog.close();
        }
        function test_referenceRender() {
            mouseMove(scene,1000,800); wait(50);
            const snapshot=grabImage(scene);
            compare(snapshot.width,1668); compare(snapshot.height,895);
            compare(snapshot.pixel(500,500),"#11151a");
            snapshot.save(Qt.resolvedUrl("../../build/classic-panels/components-1668x895.png").toString().slice(Qt.platform.os === "windows" ? 8 : 7));
        }
    }
}

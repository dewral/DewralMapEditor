pragma ComponentBehavior: Bound

import QtQuick

import QtQuick.Controls

import QtQuick.Layouts

import "../themes/fluent/Colors.js" as Colors

Control {

    id: bar

    required property var session

    required property var mapView

    signal activationRequested()

    readonly property bool sessionActive: !!session && session.mapSessionActive

    readonly property bool locked: !!mapView.generationInputBlocked || (!!session && session.library.busy)

    readonly property string styleName: session && session.selectedStyleIndex >= 0 && session.selectedStyleIndex < session.styles.length ? session.styles[session.selectedStyleIndex].name : "Wybierz wzorzec"

    implicitHeight: commands.implicitHeight + 12 + (sessionActive ? statusLine.implicitHeight + 6 : 0)

    height: implicitHeight

    palette.window: Colors.c("toolbar")

    palette.base: Colors.c("field")

    palette.text: Colors.c("text")

    palette.windowText: Colors.c("text")

    palette.button: Colors.c("button")

    palette.buttonText: Colors.c("buttonText")

    palette.highlight: Colors.c("accent")

    palette.highlightedText: Colors.c("text")

    background: Rectangle {color: Colors.c("toolbar")}

    Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;color: Colors.c("separator")}

    Shortcut {sequence: "Escape";enabled: bar.sessionActive&&!bar.session.visible&&!stylePicker.visible&&!options.visible;onActivated: bar.session.endOnMap()}

    component BrushButton: ToolButton {

        id: control

        property bool dropdown: false

        property url previewSource: ""

        property string svgIcon: ""

        property bool showLabel: svgIcon.length===0

        property string helpText: text

        implicitHeight: 32

        implicitWidth: Math.max(34,implicitContentWidth+leftPadding+rightPadding)

        padding: 6

        rightPadding: dropdown?18:6

        spacing: 8

        focusPolicy: Qt.NoFocus

        font.pixelSize: 12

        font.family: "Segoe UI"

        display: showLabel?AbstractButton.TextBesideIcon:AbstractButton.IconOnly

        icon.source: svgIcon.length?Qt.resolvedUrl("../themes/fluent/icons/procedural-"+svgIcon+".svg"):previewSource

        readonly property int imageExtent: previewSource.toString().length&&!svgIcon.length?24:20

        icon.width: imageExtent

        icon.height: imageExtent

        icon.color: previewSource.toString().length&&!svgIcon.length?"transparent":Colors.c("buttonText")

        palette.buttonText: Colors.c("buttonText")

        opacity: enabled?1:0.4

        Accessible.name: text

        Accessible.description: helpText

        ToolTip.visible: hovered

        ToolTip.delay: 450

        ToolTip.text: helpText

        Canvas {anchors.right: parent.right;anchors.rightMargin: 8;anchors.verticalCenter: parent.verticalCenter;width: 7;height: 4;visible: control.dropdown;onPaint: {const c=getContext("2d");c.clearRect(0,0,width,height);c.beginPath();c.moveTo(0,0);c.lineTo(3.5,3.5);c.lineTo(7,0);c.strokeStyle=Colors.c("muted");c.lineWidth=1;c.stroke();}}

        background: Rectangle {radius: 4;color: control.checked?Colors.c("selected"):control.down?Colors.c("pressed"):control.hovered?Colors.c("hover"):"transparent";border.color: control.checked?Colors.c("accent"):"transparent"}

    }

    Flow {

        id: commands

        anchors {left: parent.left;right: parent.right;top: parent.top;leftMargin: 8;rightMargin: 8;topMargin: 6}

        spacing: 6

        BrushButton {objectName: "mapBrushToggle";svgIcon: "brush";text: "Pędzel z wzorca";checked: bar.sessionActive;onClicked: {if(bar.sessionActive)bar.session.endOnMap();else bar.activationRequested();}}

        BrushButton {objectName: "mapBrushStyle";visible: bar.sessionActive;width: Math.min(220,bar.width-16);text: bar.styleName;dropdown: true;previewSource: bar.session?bar.session.styleIcon(bar.session.styleId):"";enabled: !bar.locked;onClicked: stylePicker.open()}

        Row {

            visible: bar.sessionActive;spacing: 2

            BrushButton {objectName: "mapBrushCorridor";svgIcon: "path";helpText: "Ścieżka — przeciągnij, aby narysować korytarz";text: "Ścieżka";checked: bar.mapView.generationPaintMode===3;enabled: !!bar.session&&!!bar.session.styleId&&!bar.locked;onClicked: bar.session.armBrush(3)}

            BrushButton {objectName: "mapBrushRoom";svgIcon: "area";helpText: "Obszar — maluj większe, nieregularne powierzchnie";text: "Obszar";checked: bar.mapView.generationPaintMode===4;enabled: !!bar.session&&!!bar.session.styleId&&!bar.locked;onClicked: {if(bar.session.brushWidthValue<11)bar.session.setMapWidth(17);bar.session.armBrush(4)}}

            BrushButton {objectName: "mapBrushReshape";svgIcon: "reshape";text: "Zwężaj";helpText: "Przebudowa — zamykaj fragmenty komory i dopasuj ściany";checked: bar.mapView.generationPaintMode===6;enabled: !!bar.session&&!!bar.session.styleId&&!bar.locked;onClicked: bar.session.armBrush(6)}

            BrushButton {objectName: "mapBrushPrefab";svgIcon: "prefab";helpText: "Prefab — wstaw cały zapisany układ";text: "Struktura";checked: bar.mapView.generationPaintMode===5;enabled: !!bar.session&&bar.session.brushPrefabs.length>0&&!bar.locked;onClicked: bar.session.armBrush(5)}

        }

        Row {

            visible: bar.sessionActive;spacing: 6

            Label {height: 30;verticalAlignment: Text.AlignVCenter;text: "Rozmiar";color: Colors.c("muted");font.pixelSize: 11}

            SpinBox {objectName: "mapBrushWidth";width: 90;height: 30;from: 3;to: 63;value: bar.session?bar.session.brushWidthValue:7;editable: true;enabled: !bar.locked;onValueModified: bar.session.setMapWidth(value)}

        }

        BrushButton {objectName: "pickMapBrushSource";svgIcon: "select";helpText: "Zaznacz gotowy fragment mapy do nauki wzorca";visible: bar.sessionActive;text: "Zaznacz wzorzec";enabled: !bar.locked;onClicked: bar.session.pickMapExample()}

        BrushButton {objectName: "learnMapBrushSource";svgIcon: "learn";helpText: "Zapamiętaj zaznaczenie jako nowy wzorzec";visible: bar.sessionActive;text: "Zapamiętaj";enabled: !!bar.session&&bar.mapView.selectionCount>0&&!bar.locked;onClicked: bar.session.learnMapSelection()}

        ComboBox {visible: bar.sessionActive&&bar.mapView.generationPaintMode===5;width: 180;height: 30;model: bar.session?bar.session.brushPrefabs:[];textRole: "name";currentIndex: bar.session?bar.session.selectedPrefabIndex:-1;onActivated: bar.session.choosePrefab(currentIndex)}

        SpinBox {visible: bar.sessionActive&&bar.mapView.generationPaintMode===5;width: 80;height: 30;from: 1;to: 32;value: bar.session?bar.session.structureSpan:1;onValueModified: bar.session.structureSpan=value;ToolTip.visible: hovered;ToolTip.text: "Długość powtarzanej części struktury"}

        BrushButton {objectName: "mapBrushOptions";svgIcon: "settings";visible: bar.sessionActive;text: "Ustawienia";dropdown: true;enabled: !bar.locked;onClicked: options.open()}

        BrushButton {objectName: "mapBrushLibrary";svgIcon: "library";visible: bar.sessionActive;text: "Biblioteka";enabled: !bar.locked;onClicked: bar.session.openLibrary()}

        BrushButton {objectName: "closeMapBrush";svgIcon: "close";visible: bar.sessionActive;text: "Wyłącz pędzel";onClicked: bar.session.endOnMap()}

        Label {visible: !bar.sessionActive;height: 30;verticalAlignment: Text.AlignVCenter;text: "Maluj na podstawie fragmentów swojej mapy";color: Colors.c("muted");font.pixelSize: 11;width: Math.min(300,bar.width-16);elide: Text.ElideRight}

    }

    RowLayout {

        id: statusLine

        anchors {left: parent.left;right: parent.right;top: commands.bottom;leftMargin: 12;rightMargin: 12;topMargin: 6}

        visible: bar.sessionActive

        BusyIndicator {Layout.preferredWidth: 16;Layout.preferredHeight: 16;running: !!bar.session&&bar.session.library.busy;visible: running}

        Label {objectName: "mapBrushStatus";Layout.fillWidth: true;maximumLineCount: 2;wrapMode: Text.WordWrap;elide: Text.ElideRight;color: Colors.c("muted");font.pixelSize: 11;text: bar.session?(bar.session.library.busy?(bar.session.quickLearning?"Zapamiętuję wzorzec…":"Przygotowuję teren…"):bar.session.message):"";ToolTip.visible: statusHover.hovered;ToolTip.text: text;HoverHandler {id: statusHover}}

        Label {visible: bar.width>1000;text: "LPM — maluj    Ctrl+Z — cofnij    Esc — zakończ";color: Colors.c("muted");font.pixelSize: 11}

    }

    Popup {

        id: stylePicker

        objectName: "mapBrushStylePicker"

        focus: true

        onClosed: bar.mapView.forceActiveFocus()

        x: Math.max(0,Math.min(190,bar.width-width));y: commands.y+30

        width: Math.min(330,bar.width);padding: 8

        background: Rectangle {color: Colors.c("popup");border.color: Colors.c("border");radius: 6}

        contentItem: Column {

            spacing: 6

            Label {text: "Twoje wzorce";color: Colors.c("text");font.bold: true;padding: 6}

            ScrollView {

                width: parent.width;height: Math.min(320,styleList.implicitHeight);clip: true

                Column {

                    id: styleList;width: stylePicker.availableWidth;spacing: 4

                    Repeater {

                        model: bar.session?bar.session.styles:[]

                        delegate: BrushButton {

                            required property var modelData

                            required property int index

                            objectName: "mapBrushStyleCard"+index

                            width: styleList.width;height: 48

                            checked: !!bar.session&&bar.session.selectedStyleIndex===index

                            text: modelData.name

                            previewSource: bar.session?bar.session.styleIcon(modelData.id):""

                            onClicked: {bar.session.chooseStyle(index);stylePicker.close();bar.mapView.forceActiveFocus()}

                        }

                    }

                }

            }

            Label {visible: !!bar.session&&!bar.session.styles.length;width: parent.width;wrapMode: Text.WordWrap;text: "Zaznacz gotowy fragment mapy i kliknij Zapamiętaj.";color: Colors.c("muted");padding: 6}

            BrushButton {text: "Pobierz nowy wzorzec z mapy";width: parent.width;onClicked: {stylePicker.close();bar.session.pickMapExample()}}

        }

    }

    Popup {

        id: options

        focus: true

        onClosed: bar.mapView.forceActiveFocus()

        x: Math.max(0,bar.width-width-8);y: commands.y+commands.height

        width: Math.min(310,bar.width);padding: 12

        background: Rectangle {color: Colors.c("popup");border.color: Colors.c("border");radius: 6}

        contentItem: ColumnLayout {

            spacing: 8

            Label {text: "Kształt i teren";color: Colors.c("text");font.bold: true}

            Label {text: "Nieregularność: "+(bar.session?bar.session.roughnessValue:35)+"%";color: Colors.c("muted")}

            Slider {Layout.fillWidth: true;from: 0;to: 100;stepSize: 1;value: bar.session?bar.session.roughnessValue:35;onMoved: bar.session.setMapRoughness(Math.round(value))}

            CheckBox {text: "Zastępuj teren pod pędzlem";checked: !!bar.session&&bar.session.replaceTerrain;palette.windowText: Colors.c("text");onToggled: bar.session.setReplaceTerrain(checked)}

            BrushButton {text: "Użyj zaznaczonego podłoża jako wnętrza";enabled: bar.mapView.selectionCount>0;onClicked: {

                const sample=bar.mapView.generationExample();let ground=0;

                for(const t of sample.tiles||[]){const g=(t.items||[]).find(i=>i.ground);if(g){ground=g.id;break;}}

                const style=bar.session.library.load(bar.session.styleId),roles=Object.assign({},style.model?style.model.roles||{}:{});roles.focusGround=ground;

                const result=bar.session.library.reviseStyle(bar.session.styleId,roles);

                if(result.success){const index=bar.session.styles.findIndex(s=>s.id===result.id);bar.session.chooseStyle(index);options.close();}else bar.session.message=result.error;

            }}

            Label {Layout.fillWidth: true;wrapMode: Text.WordWrap;text: "Puszczenie myszy zapisuje pociągnięcie. Ctrl+Z cofa teren razem z brzegami.";color: Colors.c("muted");font.pixelSize: 11}

        }

    }

    onSessionActiveChanged: if(!sessionActive){stylePicker.close();options.close()}

}

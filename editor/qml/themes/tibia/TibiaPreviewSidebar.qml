import QtQuick
import QtQuick.Controls
import Tibia 1.0

Item {
    id: root
    required property var settings
    required property var mapView
    required property var controller
    required property var outfitSource
    property int minimapFloorOffset: 0
    property point minimapPan: Qt.point(0, 0)
    signal outfitRequested()
    signal lightColorRequested()
    signal navigationRequested()
    function openSection(name) {
        const section = name === "player" ? playerSection : name === "weather" ? weatherSection : null;
        if (!section) return;
        if (section === weatherSection) settingsSection.expanded = true;
        section.expanded = true;
        Qt.callLater(() => {
            if (!section.visible || scroller.height <= 0) return;
            const top = section.mapToItem(sections, 0, 0).y;
            const firstVisible = top + Math.min(section.height, scroller.height) - scroller.height;
            scroller.contentY = Math.max(0, Math.min(top, Math.max(firstVisible, scroller.contentY)));
        });
    }
    function centreMinimap() { minimap.pxPerTile = 2; minimapFloorOffset = 0; minimapPan = Qt.point(0, 0); }

    TibiaPreviewFrame { anchors.fill: parent; kind: "sidebar" }
    component Label: TibiaPreviewLabel { }
    component ClassicButton: AbstractButton {
        id: button
        property string glyph: ""
        property string tip: text
        property string labelFont: "5"
        implicitWidth: glyph ? 20 : 86; implicitHeight: 20
        padding: 0; hoverEnabled: true; activeFocusOnTab: true
        background: Item {
            TibiaPreviewFrame { anchors.fill: parent; kind: "button"; pressed: button.down || button.checked; visible: !button.glyph }
            Image {
                anchors.fill: parent; visible: button.glyph.length > 0
                source: visible ? Qt.resolvedUrl("classic/minimap-" + button.glyph + ".png") : ""
                sourceClipRect: Qt.rect(0, button.down ? 20 : 0, 20, 20); smooth: false
                opacity: button.enabled ? 1 : 0.4
            }
        }
        contentItem: Label {
            text: button.glyph ? "" : button.text; bitmapFont: button.labelFont
            horizontalAlignment: Text.AlignHCenter
            color: button.enabled ? "#bfbfbf" : "#777777"
            y: button.down ? 1 : 0
        }
        Keys.onReturnPressed: clicked()
        ToolTip.visible: hovered && tip.length > 0; ToolTip.text: tip
        Accessible.name: tip
    }
    component ClassicCheckBox: AbstractButton {
        id: check
        implicitHeight: 20; implicitWidth: 180
        hoverEnabled: true; activeFocusOnTab: true
        background: Item { }
        contentItem: Item {
            Image { x: 2; y: Math.floor((parent.height - 12) / 2); width: 12; height: 12; source: Qt.resolvedUrl("classic/native-checkbox-" + (check.checked ? "on" : "off") + ".png"); smooth: false; opacity: check.enabled ? 1 : 0.5 }
            Label { x: 20; width: parent.width - 20; height: parent.height; text: check.text; color: check.enabled ? "#bfbfbf" : "#606060" }
        }
        Accessible.name: text
        Accessible.role: Accessible.CheckBox
    }
    component ClassicField: TextField {
        id: field
        implicitHeight: 22
        padding: 3; topPadding: 2; bottomPadding: 2
        font.family: "Verdana"; font.pixelSize: 11; font.bold: true
        color: "transparent"; selectionColor: "transparent"; selectedTextColor: "transparent"
        clip: true; selectByMouse: true
        background: TibiaPreviewFrame { kind: "field" }
        cursorDelegate: Item { }
        Label {
            id: prefix; visible: false
            text: field.text.slice(0, field.cursorPosition)
        }
        Label {
            id: display
            x: 3 - Math.max(0, prefix.implicitWidth - (field.width - 7))
            y: Math.floor((field.height - height) / 2)
            width: implicitWidth; text: field.text
        }
        Label { id: beforeSelection; visible: false; text: field.text.slice(0, field.selectionStart) }
        Rectangle {
            x: display.x + beforeSelection.implicitWidth; y: display.y; width: selected.implicitWidth; height: 14
            visible: field.selectionEnd > field.selectionStart; color: "#404080"
            Label { id: selected; anchors.fill: parent; text: field.selectedText; color: "white" }
        }
        Rectangle {
            x: display.x + prefix.implicitWidth; y: display.y + 1; width: 1; height: 10; color: "#c0c0c0"
            visible: field.activeFocus && !field.readOnly && caret.on
        }
        Timer { id: caret; property bool on: true; interval: 500; repeat: true; running: field.activeFocus; onTriggered: on = !on }
        onCursorPositionChanged: caret.on = true
    }
    component ClassicSpinBox: Item {
        id: spin
        property int from: 0
        property int to: 100
        property int value: 0
        signal edited(int next)
        signal valueModified()
        onValueModified: edited(value)
        implicitHeight: 22
        function setValue(next) { if (isFinite(next)) edited(Math.max(from, Math.min(to, Math.round(next)))); }
        ClassicField {
            id: spinInput; objectName: "classicSpinInput"; width: parent.width - 12; height: parent.height
            text: String(spin.value); validator: IntValidator { bottom: spin.from; top: spin.to }
            onTextEdited: spin.setValue(Number(text))
            onEditingFinished: text = String(spin.value)
            Keys.onUpPressed: spin.setValue(spin.value + 1)
            Keys.onDownPressed: spin.setValue(spin.value - 1)
        }
        Connections { target: spin; function onValueChanged() { if (spinInput.text !== String(spin.value)) spinInput.text = String(spin.value); } }
        Column {
            anchors.right: parent.right; width: 12
            Repeater {
                model: [-1, 1]
                AbstractButton {
                    id: stepButton
                    required property int modelData
                    width: 12; height: Math.floor(spin.height / 2); padding: 0
                    background: TibiaPreviewFrame { kind: "button"; pressed: stepButton.down }
                    contentItem: Label { bitmapFont: "5"; text: stepButton.modelData < 0 ? "^" : "v"; horizontalAlignment: Text.AlignHCenter }
                    onClicked: spin.setValue(spin.value - modelData)
                    Accessible.name: modelData < 0 ? "Increase" : "Decrease"
                }
            }
        }
    }
    component ClassicSlider: Slider {
        id: slider
        implicitHeight: 18; padding: 0; leftPadding: 12; rightPadding: 12
        stepSize: 1; focusPolicy: Qt.TabFocus
        background: Image {
            x: slider.leftPadding; y: Math.floor((slider.height - 12) / 2)
            width: slider.availableWidth; height: 12
            source: Qt.resolvedUrl("classic/native-slider-track.png"); fillMode: Image.Tile; smooth: false
        }
        handle: BorderImage {
            width: Math.min(slider.availableWidth, Math.max(12, Math.trunc(slider.availableWidth * 11 / (slider.to - slider.from + 11)))); height: 12
            x: slider.leftPadding + Math.trunc(slider.visualPosition * (slider.availableWidth - width))
            y: Math.floor((slider.height - height) / 2)
            source: Qt.resolvedUrl("classic/native-slider-thumb.png"); border.left: 6; border.right: 6
            horizontalTileMode: BorderImage.Repeat; smooth: false
        }
        Repeater {
            model: ["less", "more"]
            AbstractButton {
                id: arrow
                autoRepeat: true; autoRepeatDelay: 700; autoRepeatInterval: 100
                objectName: "classicSlider-" + modelData
                required property string modelData
                x: modelData === "less" ? 0 : slider.width - 12
                y: Math.floor((slider.height - 12) / 2); width: 12; height: 12; padding: 0
                background: Image { source: Qt.resolvedUrl("classic/native-slider-" + arrow.modelData + (arrow.down ? "-down" : "") + ".png"); smooth: false }
                contentItem: Item { }
                onClicked: { if (modelData === "less") slider.decrease(); else slider.increase(); slider.moved(); }
                Accessible.name: modelData === "less" ? "Decrease slider" : "Increase slider"
            }
        }
        TibiaSliderSkin { control: slider }
    }
    component ClassicComboBox: ComboBox {
        id: combo
        implicitHeight: 20; padding: 3; rightPadding: 18; font.family: "Verdana"; font.pixelSize: 11
        background: TibiaPreviewFrame { kind: "field" }
        contentItem: Label { text: combo.displayText; bitmapFont: "5" }
        indicator: Label { x: combo.width - 15; width: 12; height: combo.height; text: "v"; bitmapFont: "5"; horizontalAlignment: Text.AlignHCenter }
        delegate: ItemDelegate {
            id: option
            required property int index
            required property var modelData
            width: combo.width - 2; height: 20
            highlighted: combo.highlightedIndex === index
            contentItem: Label { text: option.modelData; bitmapFont: "5" }
            background: Rectangle { color: option.highlighted ? "#585858" : "#363636" }
        }
        popup: Popup {
            y: combo.height; width: combo.width; padding: 1
            implicitHeight: contentItem.implicitHeight + 2
            background: TibiaPreviewFrame { kind: "field" }
            contentItem: ListView { implicitHeight: contentHeight; model: combo.popup.visible ? combo.delegateModel : null; currentIndex: combo.highlightedIndex; clip: true }
        }
    }
    component Section: Item {
        id: section
        property string title: ""
        property string glyph: ""
        property bool expanded: true
        property int inset: 9
        property int rowSpacing: 4
        default property alias sectionContent: body.data
        width: parent.width
        height: popupFrame.captionHeight + popupFrame.border.bottom
                + (expanded ? body.implicitHeight + 10 : 0)

        TibiaWindowFrame {
            id: popupFrame
            anchors.fill: parent
            showHeader: false
            movable: false
            enabled: false
        }
        AbstractButton {
            id: heading
            objectName: section.objectName + "Header"
            x: popupFrame.border.left
            width: section.width - popupFrame.border.left - popupFrame.border.right
            height: popupFrame.captionHeight
            padding: 0; hoverEnabled: true; activeFocusOnTab: true
            onClicked: section.expanded = !section.expanded
            background: Rectangle {
                y: 2; height: parent.height - 4
                color: heading.down ? "#24000000"
                    : heading.hovered || heading.visualFocus ? "#16ffffff" : "transparent"
            }
            contentItem: Item {
                Image {
                    x: 3; y: Math.floor((parent.height - height) / 2)
                    width: 12; height: 12; smooth: false
                    source: section.glyph === "player" ? Qt.resolvedUrl("classic/skills.png")
                        : section.glyph === "visibility" ? Qt.resolvedUrl("classic/battle.png")
                        : section.glyph === "settings" ? Qt.resolvedUrl("classic/options.png")
                        : section.glyph === "lighting" ? Qt.resolvedUrl("icons/lighting.svg") : ""
                    sourceClipRect: section.glyph === "settings" ? Qt.rect(0, 0, 20, 20)
                        : section.glyph === "lighting" ? Qt.rect(0, 0, 0, 0) : Qt.rect(0, 0, 12, 12)
                    visible: source.toString().length > 0
                }
                Text {
                    x: 3; width: 12; height: parent.height
                    visible: section.glyph === "weather"; text: "☂"
                    font.family: "Segoe UI Symbol"; font.pixelSize: 12
                    color: "#d7d7d2"; verticalAlignment: Text.AlignVCenter
                }
                Label {
                    x: section.glyph ? 19 : 3
                    width: Math.max(0, collapseMark.x - x - 4)
                    height: parent.height; text: section.title; bitmapFont: "ui"
                    color: "#deded9"
                }
                Image {
                    id: collapseMark
                    x: parent.width - width - 1
                    y: Math.floor((parent.height - height) / 2)
                    width: 12; height: 12
                    source: Qt.resolvedUrl("classic/miniwindow_buttons.png"); smooth: false
                    sourceClipRect: Qt.rect(section.expanded ? 1 : 15, heading.down ? 29 : heading.hovered ? 15 : 1, 12, 12)
                }
            }
            Accessible.name: section.title
            Accessible.description: section.expanded ? "Collapse section" : "Expand section"
        }
        Column {
            id: body
            x: section.inset; y: popupFrame.captionHeight + 4
            width: parent.width - section.inset * 2
            spacing: section.rowSpacing
            visible: section.expanded
        }
    }
    component VitalBar: Row {
        property string glyph: "health"
        width: parent.width; height: 13; spacing: 6
        Image { width: 12; height: 11; source: Qt.resolvedUrl("classic/native-" + parent.glyph + ".png"); smooth: false }
        BorderImage {
            width: parent.width - 60; height: 11; source: Qt.resolvedUrl("classic/native-bar-" + parent.glyph + ".png")
            border.left: 2; border.right: 2; smooth: false
        }
        Label { width: 36; height: 12; bitmapFont: "5"; text: "100%"; horizontalAlignment: Text.AlignRight }
    }
    TibiaPreviewScrollBar {
        id: sidebarScroll; objectName: "previewClassicScrollbar"
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.margins: 3
        width: 12; z: 1; flickable: scroller; visible: range > 0
    }
    Flickable {
        id: scroller; objectName: "previewSidebarScroll"
        anchors.fill: parent; anchors.margins: 3
        contentWidth: width; contentHeight: sections.height; clip: true; boundsBehavior: Flickable.StopAtBounds
        Column {
            id: sections; width: scroller.width - (scroller.contentHeight > scroller.height ? 13 : 0); spacing: 2
            Section {
                objectName: "sidebarMinimapSection"; title: "Minimap"; inset: 6; rowSpacing: 2
                Item {
                    width: parent.width; height: 115
                    TibiaPreviewFrame {
                        x: 0; y: 3; width: parent.width - 51; height: 108; kind: "flat"
                        Rectangle { anchors.fill: parent; anchors.margins: 1; color: "black" }
                        MinimapView {
                            id: minimap; objectName: "previewSidebarMinimap"
                            anchors.fill: parent; anchors.margins: 1; source: root.mapView; pxPerTile: 2
                            centerTile: Qt.point(root.controller.visualX + root.minimapPan.x, root.controller.visualY + root.minimapPan.y)
                            focusFloor: Math.max(0, Math.min(15, root.controller.z + root.minimapFloorOffset))
                            onTileActivated: (x, y) => {
                                root.settings.ingamePreviewFollowCursor = false;
                                root.controller.setPosition(x, y, focusFloor); root.navigationRequested();
                            }
                        }
                    }
                    Item {
                        anchors.right: parent.right; width: 43; height: 115
                        Image {
                            y: 3; width: 43; height: 43; source: Qt.resolvedUrl("classic/minimap-circle.png"); smooth: false
                            MouseArea {
                                anchors.fill: parent
                                onClicked: mouse => {
                                    const dx = mouse.x < 14 ? -10 : mouse.x > 28 ? 10 : 0;
                                    const dy = mouse.y < 14 ? -10 : mouse.y > 28 ? 10 : 0;
                                    if (!dx && !dy) root.centreMinimap();
                                    else root.minimapPan = Qt.point(root.minimapPan.x + dx, root.minimapPan.y + dy);
                                }
                            }
                        }
                        ClassicButton { objectName: "sidebarMinimapZoomOut"; x: 0; y: 48; glyph: "zoom-out"; tip: "Zoom minimap out"; enabled: minimap.pxPerTile > 1; onClicked: minimap.pxPerTile = Math.max(1, minimap.pxPerTile - 1) }
                        ClassicButton { objectName: "sidebarMinimapZoomIn"; x: 0; y: 71; glyph: "zoom-in"; tip: "Zoom minimap in"; enabled: minimap.pxPerTile < 8; onClicked: minimap.pxPerTile = Math.min(8, minimap.pxPerTile + 1) }
                        ClassicButton { objectName: "sidebarMinimapFloorUp"; x: 23; y: 48; glyph: "floor-up"; tip: "Browse floor above"; enabled: minimap.focusFloor > 0; onClicked: root.minimapFloorOffset = minimap.focusFloor - 1 - root.controller.z }
                        ClassicButton { objectName: "sidebarMinimapFloorDown"; x: 23; y: 71; glyph: "floor-down"; tip: "Browse floor below"; enabled: minimap.focusFloor < 15; onClicked: root.minimapFloorOffset = minimap.focusFloor + 1 - root.controller.z }
                        ClassicButton { objectName: "sidebarMinimapCentre"; y: 93; width: 43; text: "Centre"; tip: "Centre minimap on the player"; onClicked: root.centreMinimap() }
                    }
                }
                Column { width: parent.width; spacing: 0; VitalBar { objectName: "sidebarHealth" } VitalBar { objectName: "sidebarMana"; glyph: "mana" } }
            }
            Section {
                id: playerSection; objectName: "sidebarPlayerSection"; title: "Player options"; glyph: "player"
                readonly property int labelWidth: 60
                Row {
                    width: parent.width; spacing: 6
                    Label { width: playerSection.labelWidth; height: nameField.height; text: "Name:" }
                    ClassicField {
                        id: nameField; objectName: "sidebarPlayerName"; width: parent.width - playerSection.labelWidth - parent.spacing
                        text: root.settings.ingamePreviewPlayerName
                        onTextEdited: root.settings.ingamePreviewPlayerName = text
                        Accessible.name: "Player name"
                    }
                }
                Row {
                    width: parent.width; spacing: 6
                    Label { width: playerSection.labelWidth; height: levelField.height; text: "Level:" }
                    ClassicSpinBox {
                        id: levelField; objectName: "sidebarPlayerLevel"; width: parent.width - playerSection.labelWidth - parent.spacing
                        from: 1; to: 1000; value: root.settings.ingamePreviewPlayerLevel
                        onEdited: next => { root.settings.ingamePreviewPlayerLevel = next; root.settings.ingamePreviewSpeed = 0; }
                    }
                }
                Row {
                    width: parent.width; spacing: 6
                    Label { width: playerSection.labelWidth; height: speedField.height; text: "Speed:" }
                    ClassicSpinBox {
                        id: speedField; objectName: "sidebarPlayerSpeed"; width: parent.width - playerSection.labelWidth - parent.spacing
                        from: 20; to: 3000; value: root.controller.speed
                        onEdited: next => root.settings.ingamePreviewSpeed = next
                    }
                }
                Row {
                    width: parent.width; height: 48; spacing: 6
                    Label { width: playerSection.labelWidth; height: parent.height; text: "Outfit:" }
                    Item {
                        width: parent.width - playerSection.labelWidth - parent.spacing; height: parent.height
                        ClassicButton {
                            objectName: "sidebarPreviousOutfit"
                            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                            width: 26; height: 24; labelFont: "ui"; text: "<"; tip: "Previous outfit"
                            onClicked: root.settings.ingamePreviewLookType = Math.max(1, root.settings.ingamePreviewLookType - 1)
                        }
                        Image {
                            objectName: "sidebarOutfitSprite"
                            anchors.centerIn: parent; width: 44; height: 44
                            smooth: false; fillMode: Image.PreserveAspectFit
                            source: root.outfitSource(root.settings.ingamePreviewLookType, root.settings.ingamePreviewLookHead,
                                root.settings.ingamePreviewLookBody, root.settings.ingamePreviewLookLegs, root.settings.ingamePreviewLookFeet)
                        }
                        ClassicButton {
                            objectName: "sidebarNextOutfit"
                            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                            width: 26; height: 24; labelFont: "ui"; text: ">"; tip: "Next outfit"
                            onClicked: root.settings.ingamePreviewLookType = Math.min(Math.max(1, Backend.datReader.outfitCount), root.settings.ingamePreviewLookType + 1)
                        }
                    }
                }
                ClassicButton {
                    objectName: "sidebarChooseOutfit"
                    width: parent.width; height: 24; labelFont: "7"; text: "Choose outfit..."
                    onClicked: root.outfitRequested()
                }
            }
            Section {
                id: settingsSection; objectName: "sidebarSettingsSection"; title: "Preview settings"; glyph: "settings"; inset: 6
                Section {
                    id: weatherSection; objectName: "sidebarWeatherSection"; title: "Weather"; glyph: "weather"; expanded: false
                    readonly property int labelWidth: 56
                    readonly property int valueWidth: 59
                    ClassicCheckBox { objectName: "sidebarWeatherEnabled"; text: "Enable weather"; checked: root.settings.previewWeatherEnabled; onClicked: root.settings.previewWeatherEnabled = !checked }
                    Repeater {
                        model: [{label: "Rain", key: "Rain"}, {label: "Storm", key: "Storm"}, {label: "Snow", key: "Snow"}, {label: "Fog", key: "Fog"}, {label: "Leaves", key: "Leaves"}, {label: "Fireflies", key: "Fireflies"}, {label: "Wind", key: "Wind"}]
                        delegate: Row {
                            required property var modelData
                            width: parent.width; spacing: 5; height: 18
                            Label { width: weatherSection.labelWidth; height: parent.height; text: modelData.label + ":" }
                            ClassicSlider {
                                objectName: "sidebarWeather" + modelData.key
                                width: Math.max(0, parent.width - weatherSection.labelWidth - weatherSection.valueWidth - 2 * parent.spacing)
                                height: parent.height
                                enabled: root.settings.previewWeatherEnabled
                                opacity: enabled ? 1 : 0.48
                                Accessible.name: modelData.label
                                from: modelData.key === "Wind" ? -160 : 0; to: modelData.key === "Wind" ? 160 : 100
                                value: root.settings["previewWeather" + modelData.key]
                                onMoved: root.settings["previewWeather" + modelData.key] = Math.round(value)
                            }
                            Label {
                                width: weatherSection.valueWidth; height: parent.height
                                horizontalAlignment: Text.AlignRight
                                text: root.settings["previewWeather" + modelData.key] + (modelData.key === "Wind" ? " px/s" : "%")
                                color: root.settings.previewWeatherEnabled ? "#dfdfda" : "#a8a8a3"
                            }
                        }
                    }
                    Row {
                        width: parent.width; spacing: 5
                        Label { width: weatherSection.labelWidth; height: qualityBox.height; text: "Quality:" }
                        ClassicComboBox {
                            id: qualityBox; objectName: "sidebarWeatherQuality"
                            width: parent.width - weatherSection.labelWidth - parent.spacing
                            enabled: root.settings.previewWeatherEnabled
                            opacity: enabled ? 1 : 0.48
                            model: ["Low", "Balanced", "High"]; currentIndex: root.settings.previewWeatherQuality
                            onActivated: index => root.settings.previewWeatherQuality = index
                            Accessible.name: "Weather quality"
                        }
                    }
                }
                Section {
                    objectName: "sidebarVisibilitySection"; title: "Visibility"; glyph: "visibility"; expanded: false
                    ClassicCheckBox { objectName: "sidebarShowCreatures"; text: "Show creatures"; checked: root.settings.ingamePreviewShowCreatures; onClicked: root.settings.ingamePreviewShowCreatures = !checked }
                    ClassicCheckBox { objectName: "sidebarShowItems"; text: "Show items"; checked: root.settings.ingamePreviewShowItems; onClicked: root.settings.ingamePreviewShowItems = !checked }
                    ClassicCheckBox { objectName: "sidebarAnimateItems"; text: "Animate items"; checked: root.settings.ingamePreviewAnimateItems; onClicked: root.settings.ingamePreviewAnimateItems = !checked }
                    ClassicCheckBox { objectName: "sidebarShowBorders"; text: "Show borders"; checked: root.settings.ingamePreviewShowBorders; onClicked: root.settings.ingamePreviewShowBorders = !checked }
                }
                Section {
                    objectName: "sidebarLightingSection"; title: "Lighting"; glyph: "lighting"; expanded: false
                    ClassicCheckBox { objectName: "sidebarLightingEnabled"; text: "Enable lighting"; checked: root.settings.ingamePreviewLighting; onClicked: root.settings.ingamePreviewLighting = !checked }
                    Label { width: parent.width; text: "Ambient: " + root.mapView.lightAmbient }
                    ClassicSlider { objectName: "sidebarAmbient"; width: parent.width; from: 0; to: 255; value: root.mapView.lightAmbient; onMoved: root.mapView.lightAmbient = Math.round(value) }
                    ClassicButton { objectName: "sidebarLightColor"; width: parent.width; text: "Light color..."; onClicked: root.lightColorRequested() }
                }
            }
        }
    }
}

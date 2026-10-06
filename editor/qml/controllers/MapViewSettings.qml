import QtQuick

Item {
    id: controller
    visible: false
    width: 0
    height: 0

    required property var settings
    required property var mapView
    property bool initialized: false
    readonly property var preferenceNames: [
        "showWallOutlines", "showGrid", "showPathing", "showCreatures",
        "showSpawns", "showHouses", "showZones", "showZonesAlways",
        "visibleZoneMask", "zoneOpacities",
        "tilesOpacity", "itemsOpacity", "houseOpacity",
        "showAnimations", "torchOn", "lightAmbient", "minimapOn",
        "showShade", "showLowerFloors", "placeEffect", "automagic",
        "compensatedSelect", "selectionFloors"
    ]

    Component.onCompleted: {
        // Several properties share a notify signal. Ignore changes during
        // restoration so the remaining saved values cannot be overwritten.
        for (const name of preferenceNames)
            mapView[name] = settings[name];
        initialized = true;
    }

    function save() {
        if (!initialized)
            return;
        for (const name of preferenceNames)
            settings[name] = mapView[name];
    }

    // Observe the view itself: menus, toolbars and keyboard shortcuts all
    // change these properties, and should all save the same preferences.
    Connections {
        target: controller.mapView
        function onViewFlagsChanged() { controller.save(); }
        function onShowAnimationsChanged() { controller.save(); }
        function onTorchChanged() { controller.save(); }
        function onMinimapOnChanged() { controller.save(); }
        function onShowShadeChanged() { controller.save(); }
        function onShowLowerFloorsChanged() { controller.save(); }
        function onPlaceEffectChanged() { controller.save(); }
        function onAutomagicChanged() { controller.save(); }
        function onSelectionOptionsChanged() { controller.save(); }
    }
}

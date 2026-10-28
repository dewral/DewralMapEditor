import QtQuick
import Tibia 1.0

Item {
    id: layer
    required property var mapView
    required property var settings
    property string settingsPrefix: "weather"
    property bool previewCamera: false
    property real previewOriginX: 0
    property real previewOriginY: 0
    property int previewFloor: 7
    property real viewX: 0
    property real viewY: 0
    readonly property int particleCount: weather.particleCount
    clip: true
    function refreshCamera() {
        if (!previewCamera && mapView) {
            viewX = mapView.renderOriginX();
            viewY = mapView.renderOriginY();
        }
    }
    Component.onCompleted: refreshCamera()
    Connections {
        target: layer.mapView
        function onContentUpdated() { layer.refreshCamera() }
        function onReadersChanged() { layer.refreshCamera() }
    }
    WeatherPreview {
        id: weather
        objectName: "weatherRenderer"
        anchors.fill: parent
        map: layer.mapView.otbm
        originX: layer.previewCamera ? layer.previewOriginX : layer.viewX
        originY: layer.previewCamera ? layer.previewOriginY : layer.viewY
        tileSize: layer.previewCamera ? 32 : layer.mapView.tileSize
        floor: layer.previewCamera ? layer.previewFloor : layer.mapView.floor
        configuration: ({
            enabled: !!layer.settings[layer.settingsPrefix + "Enabled"],
            paused: !!layer.settings[layer.settingsPrefix + "Paused"],
            rain: layer.settings[layer.settingsPrefix + "Rain"] || 0,
            storm: layer.settings[layer.settingsPrefix + "Storm"] || 0,
            snow: layer.settings[layer.settingsPrefix + "Snow"] || 0,
            fog: layer.settings[layer.settingsPrefix + "Fog"] || 0,
            leaves: layer.settings[layer.settingsPrefix + "Leaves"] || 0,
            fireflies: layer.settings[layer.settingsPrefix + "Fireflies"] || 0,
            wind: layer.settings[layer.settingsPrefix + "Wind"] || 0,
            quality: layer.settings[layer.settingsPrefix + "Quality"] === undefined ? 1 : layer.settings[layer.settingsPrefix + "Quality"],
            respectRoofs: layer.settings[layer.settingsPrefix + "RespectRoofs"] !== false,
            fps: layer.settings.renderMaxFps > 0 ? Math.min(60, layer.settings.renderMaxFps) : 60
        })
    }
}

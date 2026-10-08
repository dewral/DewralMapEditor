import QtQuick
import Tibia 1.0

Item {
    id: root
    required property var controller
    property string playerName: ""
    property real viewScale: 1
    signal blocked

    visible: controller.positioned

    property var outfitFrame: Backend.datReader.outfitFramePreview(
                                  controller.lookType,
                                  controller.direction,
                                  controller.walking,
                                  controller.walkAnimationTick)

    Image {
        id: playerSprite
        // OTClient composes multi-tile outfits from their bottom-right tile.
        x: parent.width / 2 + 16 - width - (root.outfitFrame.offsetX ?? 8)
        y: parent.height / 2 + 16 - height - (root.outfitFrame.offsetY ?? 8)
        width: Math.max(1, root.outfitFrame.width || 1) * 32
        height: Math.max(1, root.outfitFrame.height || 1) * 32
        smooth: false
        cache: true
        fillMode: Image.PreserveAspectFit
        source: {
            const frame = root.outfitFrame
            return frame.ids !== undefined && frame.ids.length > 0
                    ? Backend.sprReader.outfitImageSource(
                          frame.ids, frame.maskIds || [], frame.width,
                          frame.height, controller.lookHead,
                          controller.lookBody, controller.lookLegs,
                          controller.lookFeet) : ""
        }
    }

    // OTClient anchors creature information two map pixels above the tile.
    // Font and HP bar remain screen-sized while the world view scales.
    Item {
        id: information
        width: 27
        height: 4
        x: Math.round(root.width / 2 - (root.outfitFrame.offsetX ?? 8) - width / 2)
        y: root.height / 2 - 18 - (root.outfitFrame.offsetY ?? 8)
        scale: 1 / Math.max(0.01, root.viewScale)
        transformOrigin: Item.Top
        OtclientName {
            anchors.horizontalCenter: parent.horizontalCenter
            y: -12
            text: root.playerName
        }
        Rectangle {
            anchors.fill: parent
            color: "black"
            Rectangle {
                x: 1; y: 1
                width: 25; height: 2
                color: "#00bc00"
            }
        }
    }

    Rectangle {
        id: marker
        anchors.centerIn: parent
        width: 28
        height: 28
        color: "transparent"
        border.width: playerSprite.status === Image.Ready ? 0 : 1
        border.color: "#58A6FF"
    }

    Connections {
        target: root.controller
        function onMovementBlocked() { blockedAnimation.restart() }
    }

    SequentialAnimation {
        id: blockedAnimation
        ColorAnimation { target: marker; property: "border.color"; to: "#F85149"; duration: 60 }
        ColorAnimation { target: marker; property: "border.color"; to: "#58A6FF"; duration: 160 }
    }
}

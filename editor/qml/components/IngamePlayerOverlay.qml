import QtQuick
import Tibia 1.0

Item {
    id: root
    required property var controller
    signal blocked

    visible: controller.positioned

    property var outfitFrame: Backend.datReader.outfitFramePreview(
                                  controller.lookType,
                                  controller.direction,
                                  controller.walking,
                                  controller.walkAnimationTick)

    Image {
        id: playerSprite
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.verticalCenter
        anchors.bottomMargin: -16
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

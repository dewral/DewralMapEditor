import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../dialogs"

Item {
    id: root
    component TimerLabel: Text { font.family: Backend.uiTheme.style === "tibia-ui" ? "Verdana" : "Segoe UI"; font.bold: Backend.uiTheme.style === "tibia-ui" }
    property var timerService: Backend.workTimer
    property var activityService: Backend.activity
    property var popupHost
    function openPanel() { if (popupHost) popupHost.openFor(root); }
    property bool statusBarMode: false
    implicitWidth: statusBarMode ? 344 : 245
    implicitHeight: 30
    property color foreground: "#C9D1D9"
    property color accent: "#3FB950"
    property var selectedSession: null
    property string pendingTimerRemoval: ""
    property bool showTimerDetails: false
    readonly property bool fluentTheme: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool grayTheme: fluentTheme || Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property color panelColor: grayTheme ? "#202122" : "#0F141B"
    readonly property color cardColor: grayTheme ? "#292b2d" : "#151B23"
    readonly property color inputColor: grayTheme ? "#202020" : "#0D1117"
    readonly property color hoverColor: grayTheme ? "#303030" : "#1B2632"
    readonly property color borderColor: grayTheme ? "#414447" : "#2D3743"
    readonly property color strongText: grayTheme ? "#F0F0F0" : "#F0F3F6"
    readonly property color mutedText: grayTheme ? "#9A9A9A" : "#8B949E"
    readonly property color subtleText: grayTheme ? "#777777" : "#6E7681"
    readonly property color activeFill: fluentTheme ? "#414447" : grayTheme ? "#4A3A1F" : "#163B2C"

    function durationFromText(value) {
        var parts = value.split(":")
        if (parts.length !== 3)
            return -1
        var seconds = Number(parts[0]) * 3600 + Number(parts[1]) * 60 + Number(parts[2])
        return isNaN(seconds) ? -1 : Math.max(0, Math.floor(seconds))
    }

    Rectangle {
        anchors.fill: parent
        radius: Backend.uiTheme.style === "tibia-ui" ? 0 : root.statusBarMode ? 4 : root.fluentTheme ? 0 : 4
        color: hit.containsMouse ? root.hoverColor : "transparent"
    }
    RowLayout {
        z: 1
        anchors.fill: parent
        anchors.leftMargin: root.statusBarMode ? 8 : 4
        anchors.rightMargin: root.statusBarMode ? 4 : 4
        spacing: root.width < 180 ? 5 : 8
        Rectangle {
            visible: root.statusBarMode
            width: 7; height: 7; radius: 3.5
            color: !root.timerService.active ? root.mutedText : root.timerService.running ? "#58BD87" : "#E4B153"
        }
        Canvas {
            id: clockGlyph
            visible: (!root.statusBarMode || root.width >= 175) && !root.timerService.active
            Layout.alignment: Qt.AlignVCenter
            width: 16
            height: 16
            property color glyphColor: root.accent
            onGlyphColorChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = glyphColor
                ctx.lineWidth = 1.6
                ctx.beginPath()
                ctx.arc(8, 8.5, 5.5, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(8, 8.5)
                ctx.lineTo(8, 5)
                ctx.moveTo(8, 8.5)
                ctx.lineTo(10.5, 10)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(6, 1.5)
                ctx.lineTo(10, 1.5)
                ctx.stroke()
            }
        }
        TaskIcon {
            visible: root.timerService.active && (!root.statusBarMode || root.width >= 175)
            Layout.preferredWidth: 18; Layout.preferredHeight: 18
            icon: root.timerService.taskIcon || "mapping"
        }
        TimerLabel {
            objectName: "workTimerTaskName"
            visible: root.width >= 240
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.timerService.active ? root.timerService.taskName : "Work timer"
            color: root.foreground; font.pixelSize: 12
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        TextMetrics {
            id: durationMetrics
            font.family: "Consolas"; font.pixelSize: root.statusBarMode ? 14 : 12; font.weight: Font.DemiBold
            text: root.timerService.taskElapsedText
        }
        TimerLabel {
            id: elapsedStatusText
            objectName: "workTimerDuration"
            Layout.preferredWidth: Math.max(root.statusBarMode ? 76 : 62, Math.ceil(durationMetrics.advanceWidth) + 2)
            Layout.fillWidth: root.width < 240
            height: 28
            text: root.timerService.taskElapsedText
            color: root.foreground
            font.pixelSize: root.statusBarMode ? 14 : 12
            font.family: "Consolas"; font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        Item {
            id: statusToggle
            objectName: "workTimerToggle"
            Layout.preferredWidth: root.statusBarMode ? 28 : 24
            Layout.preferredHeight: root.statusBarMode ? 28 : 24

            Rectangle {
                anchors.fill: parent
                radius: Backend.uiTheme.style === "tibia-ui" ? 0 : root.statusBarMode ? 4 : root.fluentTheme ? 0 : 4
                color: statusToggleMouse.containsMouse ? root.activeFill : "transparent"
                border.width: root.statusBarMode || root.timerService.active ? 1 : 0
                border.color: root.accent
            }
            Canvas {
                id: statusGlyph
                anchors.centerIn: parent
                width: 14
                height: 14
                visible: root.timerService.active
                property bool timerRunning: root.timerService.running
                property color glyphColor: root.accent

                onTimerRunningChanged: requestPaint()
                onGlyphColorChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = glyphColor
                    if (timerRunning) {
                        ctx.fillRect(2, 2, 3.5, 10)
                        ctx.fillRect(8.5, 2, 3.5, 10)
                    } else {
                        ctx.beginPath()
                        ctx.moveTo(3, 2)
                        ctx.lineTo(12, 7)
                        ctx.lineTo(3, 12)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
            }
            TimerLabel {
                anchors.centerIn: parent
                visible: !root.timerService.active
                text: "+"
                color: root.accent
                font.pixelSize: 14
                font.bold: true
            }
            MouseArea {
                id: statusToggleMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (!root.timerService.active) {
                        root.openPanel()
                    } else if (root.timerService.running) {
                        root.timerService.pauseSession()
                    } else {
                        root.timerService.resumeSession()
                    }
                }
            }
        }
    }
    MouseArea {
        id: hit
        z: 0
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.openPanel()
    }

}

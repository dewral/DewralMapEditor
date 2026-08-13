import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Tibia 1.0
import "../style"

Item {
    id: root
    implicitWidth: 245
    implicitHeight: 30
    property color foreground: "#C9D1D9"
    property color accent: "#3FB950"
    property var selectedSession: null
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
    readonly property color panelColor: grayTheme ? "#1A1A1A" : "#0F141B"
    readonly property color cardColor: grayTheme ? "#242424" : "#151B23"
    readonly property color inputColor: grayTheme ? "#202020" : "#0D1117"
    readonly property color hoverColor: grayTheme ? "#303030" : "#1B2632"
    readonly property color borderColor: grayTheme ? "#484848" : "#2D3743"
    readonly property color strongText: grayTheme ? "#F0F0F0" : "#F0F3F6"
    readonly property color mutedText: grayTheme ? "#9A9A9A" : "#8B949E"
    readonly property color subtleText: grayTheme ? "#777777" : "#6E7681"
    readonly property color activeFill: grayTheme ? "#4A3A1F" : "#163B2C"

    function durationFromText(value) {
        var parts = value.split(":")
        if (parts.length !== 3)
            return -1
        var seconds = Number(parts[0]) * 3600 + Number(parts[1]) * 60 + Number(parts[2])
        return isNaN(seconds) ? -1 : Math.max(0, Math.floor(seconds))
    }

    function begin() {
        const task = taskField.text.trim()
        if (task.length === 0)
            return

        const started = Backend.workTimer.active
                      ? Backend.workTimer.switchTask(task)
                      : Backend.workTimer.startSession(task)
        if (started)
            taskField.text = ""
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: hit.containsMouse ? root.hoverColor : "transparent"
    }
    Row {
        z: 1
        anchors.centerIn: parent
        height: 24
        spacing: 7
        Canvas {
            id: clockGlyph
            anchors.verticalCenter: parent.verticalCenter
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
        Text {
            id: elapsedStatusText
            anchors.verticalCenter: parent.verticalCenter
            width: 62
            height: 24
            text: Backend.workTimer.taskElapsedText
            color: root.foreground
            font.pixelSize: 12
            font.family: "Consolas"
            font.weight: Font.DemiBold
            font.letterSpacing: 0.15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            onTextChanged: tickAnimation.restart()

            ParallelAnimation {
                id: tickAnimation
                NumberAnimation { target: elapsedStatusText; property: "opacity"; from: 0.55; to: 1; duration: 150; easing.type: Easing.OutCubic }
                NumberAnimation { target: elapsedStatusText; property: "scale"; from: 0.94; to: 1; duration: 180; easing.type: Easing.OutBack }
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 80
            height: 24
            text: Backend.workTimer.active ? Backend.workTimer.taskName : "Work timer"
            color: root.foreground
            font.pixelSize: 12
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        Item {
            id: statusToggle
            width: 24
            height: 24

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: statusToggleMouse.containsMouse ? root.activeFill : "transparent"
                border.width: Backend.workTimer.active ? 1 : 0
                border.color: root.accent
            }
            Canvas {
                id: statusGlyph
                anchors.centerIn: parent
                width: 14
                height: 14
                visible: Backend.workTimer.active
                property bool timerRunning: Backend.workTimer.running
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
            Text {
                anchors.centerIn: parent
                visible: !Backend.workTimer.active
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
                    if (!Backend.workTimer.active) {
                        panel.open()
                    } else if (Backend.workTimer.running) {
                        Backend.workTimer.pauseSession()
                    } else {
                        Backend.workTimer.resumeSession()
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
        onClicked: panel.open()
    }

    Popup {
        id: panel
        width: 500
        height: 548
        x: Math.min(0, root.width - width)
        y: -height - 8
        padding: 0
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: root.panelColor
            border.color: root.borderColor
            border.width: 1
            radius: 8
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Row {
                width: parent.width
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: stack.currentIndex === 0 ? "Work timer" : "Session history"
                    color: root.strongText
                    font.pixelSize: 16
                    font.bold: true
                }
                Item { width: parent.width - parent.children[0].implicitWidth - historyButton.width; height: 1 }
                DmeButton { id: historyButton; width: 76; text: stack.currentIndex === 0 ? "History" : "Back"; onClicked: stack.currentIndex = 1 - stack.currentIndex }
            }

            StackLayout {
                id: stack
                width: parent.width
                height: parent.height - 44
                currentIndex: 0

                Item {
                    Flickable {
                        id: timerScroll
                        anchors { fill: parent; rightMargin: 12 }
                        contentWidth: width
                        contentHeight: timerContent.height
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                    Column {
                    id: timerContent
                    width: timerScroll.width
                    spacing: 10
                    Text {
                        text: Backend.workTimer.active ? "ADD OR SWITCH TASK" : "TASK NAME"
                        color: root.mutedText
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.6
                    }
                    DmeTextField {
                        id: taskField
                        width: parent.width
                        placeholderText: Backend.workTimer.active
                                         ? "Enter another timer name..."
                                         : "What are you working on?"
                        onAccepted: root.begin()
                    }
                    Text { text: "TASK TIMERS"; color: root.mutedText; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6 }
                    DmePanel {
                        width: parent.width
                        height: 132
                        ListView {
                            id: taskTimerList
                            anchors { fill: parent; margins: 3; rightMargin: 14 }
                            clip: true
                            spacing: 2
                            model: Backend.workTimer.taskTimers
                            delegate: Rectangle {
                                required property var modelData
                                width: taskTimerList.width
                                height: 42
                                radius: 3
                                color: modelData.active ? root.activeFill : (timerMouse.containsMouse ? root.hoverColor : "transparent")
                                border.width: modelData.active ? 1 : 0
                                border.color: root.accent
                                Row {
                                    anchors { fill: parent; leftMargin: 7; rightMargin: 7 }
                                    spacing: 7
                                    Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.running ? "Ⅱ" : "▶︎"; color: root.accent; font.pixelSize: 11; font.bold: true }
                                    Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 92; text: modelData.name; color: root.strongText; font.pixelSize: 12; font.bold: modelData.active; elide: Text.ElideRight }
                                    Text { anchors.verticalCenter: parent.verticalCenter; width: 70; horizontalAlignment: Text.AlignRight; text: modelData.durationText; color: modelData.active ? root.strongText : root.mutedText; font.pixelSize: 11; font.family: "Consolas" }
                                }
                                MouseArea {
                                    id: timerMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Backend.workTimer.switchTask(modelData.name)
                                }
                            }
                        }
                        DmeScrollBar {
                            anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 2 }
                            flickable: taskTimerList
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: Backend.workTimer.taskTimers.length === 0
                            text: "Create your first timer above"
                            color: root.subtleText
                            font.pixelSize: 11
                        }
                    }
                    DmePanel {
                        width: parent.width
                        height: 118
                        Column {
                            anchors { fill: parent; margins: 9 }
                            spacing: 4
                            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: Backend.workTimer.taskElapsedText; color: root.strongText; font.pixelSize: 32; font.weight: Font.DemiBold; font.family: "Consolas" }
                            Text { text: Backend.workTimer.mapName || "No active map"; color: root.mutedText; width: parent.width; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 8
                                DmeButton { width: 96; visible: !Backend.workTimer.active; text: "Start"; variant: "primary"; enabled: taskField.text.trim().length > 0 && Backend.docMgr.currentDocumentId.length > 0; onClicked: root.begin() }
                                DmeButton { width: 104; visible: Backend.workTimer.active; text: "New timer"; variant: taskField.text.trim().length > 0 ? "primary" : "default"; enabled: taskField.text.trim().length > 0; onClicked: root.begin() }
                                DmeButton { width: 82; visible: Backend.workTimer.running; text: "Pause"; variant: "primary"; onClicked: Backend.workTimer.pauseSession() }
                                DmeButton { width: 82; visible: Backend.workTimer.active && !Backend.workTimer.running; text: "Resume"; variant: "primary"; onClicked: Backend.workTimer.resumeSession() }
                                DmeButton { width: 104; visible: Backend.workTimer.active; text: "Finish task"; variant: "danger"; onClicked: Backend.workTimer.finishSession(noteField.text) }
                            }
                        }
                    }
                    Row {
                        spacing: 8
                        DmeCheckBox { text: "Session checkpoints"; checked: Backend.workTimer.checkpointEnabled; onClicked: Backend.workTimer.checkpointEnabled = !checked }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "Break reminder"; color: root.mutedText; font.pixelSize: 11 }
                        DmeSpinBox { width: 70; from: 0; to: 480; value: Backend.workTimer.breakReminderMinutes; onValueModified: Backend.workTimer.breakReminderMinutes = value }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "min"; color: root.mutedText; font.pixelSize: 11 }
                    }
                    DmeTextField { id: noteField; width: parent.width; visible: Backend.workTimer.active; placeholderText: "Optional session note" }
                    Text { text: "SESSION SUMMARY"; color: root.mutedText; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6 }
                    DmePanel {
                        width: parent.width
                        height: 82
                        Grid {
                            anchors { fill: parent; margins: 9 }
                            columns: 2; columnSpacing: 42; rowSpacing: 7
                            Text { text: "Today   " + Backend.workTimer.todayText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "This week   " + Backend.workTimer.weekText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "This map   " + Backend.workTimer.currentMapText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "Total   " + Backend.workTimer.totalText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "Operations   " + Backend.workTimer.operationCount; color: root.mutedText; font.pixelSize: 10 }
                            Text { text: "Changed tiles   " + Backend.workTimer.changedTileCount; color: root.mutedText; font.pixelSize: 10 }
                        }
                    }
                    Row {
                        spacing: 8
                        DmeCheckBox {
                            text: "Idle-Pause"
                            checked: Backend.workTimer.idlePauseMinutes > 0
                            onClicked: Backend.workTimer.idlePauseMinutes = checked ? 0 : 15
                        }
                        DmeSpinBox { width: 70; from: 0; to: 120; value: Backend.workTimer.idlePauseMinutes; onValueModified: Backend.workTimer.idlePauseMinutes = value }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "min"; color: root.mutedText; font.pixelSize: 11 }
                    }
                    Item { width: 1; height: 2 }
                    }
                    }

                    DmeScrollBar {
                        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                        flickable: timerScroll
                    }
                }

                Column {
                    spacing: 8
                    ListView {
                        id: historyList
                        width: parent.width
                        height: root.selectedSession ? 280 : 390
                        clip: true
                        model: Backend.workTimer.history
                        delegate: Rectangle {
                            id: historyRow
                            required property var modelData
                            required property int index
                            width: historyList.width
                            height: 58
                            radius: 4
                            color: historyMouse.containsMouse ? root.hoverColor : (index % 2 ? root.panelColor : root.cardColor)
                            border.width: 1
                            border.color: root.borderColor
                            Column {
                                anchors { fill: parent; margins: 7 }
                                Text { width: parent.width; text: modelData.task + "  ·  " + Backend.workTimer.formatDuration(modelData.durationSeconds); color: root.strongText; font.pixelSize: 12; elide: Text.ElideRight }
                                Text { width: parent.width; text: modelData.mapName + "  |  " + modelData.startedAt + "  |  " + modelData.changedTiles + " tiles"; color: root.mutedText; font.pixelSize: 10; elide: Text.ElideRight }
                            }
                            MouseArea {
                                id: historyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedSession = modelData
                                    editTask.text = modelData.task
                                    editNote.text = modelData.note || ""
                                    editDuration.text = modelData.durationText
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    }
                    Column {
                        width: parent.width
                        visible: root.selectedSession !== null
                        spacing: 5
                        Row {
                            spacing: 6
                            DmeTextField { id: editTask; width: 190; placeholderText: "Task" }
                            DmeTextField { id: editDuration; width: 105; placeholderText: "HH:MM:SS" }
                        }
                        DmeTextField { id: editNote; width: parent.width; placeholderText: "Note" }
                        Row {
                            spacing: 6
                            DmeButton {
                                text: "Save changes"
                                onClicked: {
                                    var seconds = root.durationFromText(editDuration.text)
                                    if (seconds >= 0 && Backend.workTimer.updateSession(root.selectedSession.id, editTask.text, editNote.text, seconds))
                                        root.selectedSession = null
                                }
                            }
                            DmeButton { text: "Delete"; onClicked: { Backend.workTimer.removeSession(root.selectedSession.id); root.selectedSession = null } }
                            DmeButton { text: "Cancel"; onClicked: root.selectedSession = null }
                        }
                    }
                    Row {
                        spacing: 8
                        DmeButton { text: "Export CSV"; onClicked: csvDialog.open() }
                        DmeButton { text: "Export JSON"; onClicked: jsonDialog.open() }
                    }
                }
            }
        }
    }

    FileDialog { id: csvDialog; title: "Export work sessions as CSV"; fileMode: FileDialog.SaveFile; nameFilters: ["CSV files (*.csv)"]; defaultSuffix: "csv"; onAccepted: Backend.workTimer.exportCsv(selectedFile) }
    FileDialog { id: jsonDialog; title: "Export work sessions as JSON"; fileMode: FileDialog.SaveFile; nameFilters: ["JSON files (*.json)"]; defaultSuffix: "json"; onAccepted: Backend.workTimer.exportJson(selectedFile) }
}

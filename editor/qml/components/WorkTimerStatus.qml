import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../dialogs"

Item {
    id: root
    property var timerService: Backend.workTimer
    property var activityService: Backend.activity
    property bool statusBarMode: false
    implicitWidth: statusBarMode ? 344 : 245
    implicitHeight: 30
    property color foreground: "#C9D1D9"
    property color accent: "#3FB950"
    property var selectedSession: null
    property string pendingTimerRemoval: ""
    property bool showTimerDetails: false
    readonly property bool fluentTheme: Backend.uiTheme.style === "fluent-dark"
    readonly property bool grayTheme: fluentTheme || Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property color panelColor: grayTheme ? "#1A1A1A" : "#0F141B"
    readonly property color cardColor: grayTheme ? "#242424" : "#151B23"
    readonly property color inputColor: grayTheme ? "#202020" : "#0D1117"
    readonly property color hoverColor: grayTheme ? "#303030" : "#1B2632"
    readonly property color borderColor: grayTheme ? "#484848" : "#2D3743"
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

    function begin() {
        const task = taskField.text.trim()
        if (task.length === 0)
            return

        const started = root.timerService.active
                      ? root.timerService.switchTask(task)
                      : root.timerService.startSession(task)
        if (started)
            taskField.text = ""
    }

    Rectangle {
        anchors.fill: parent
        radius: root.statusBarMode ? 4 : root.fluentTheme ? 0 : 4
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
            visible: !root.statusBarMode || root.width >= 175
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
        Text {
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
        Text {
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
                radius: root.statusBarMode ? 4 : root.fluentTheme ? 0 : 4
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
            Text {
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
                        panel.open()
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
        onClicked: panel.open()
    }

    Popup {
        id: panel
        width: 470
        height: (root.showTimerDetails || stack.currentIndex !== 0) ? 548 : 442
        x: Math.min(0, root.width - width)
        y: -height - 8
        padding: 0
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: root.panelColor
            border.color: root.borderColor
            border.width: 1
            radius: root.fluentTheme ? 0 : 8
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Row {
                width: parent.width
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: stack.currentIndex === 0 ? "Work timer" : stack.currentIndex === 1 ? "Session history" : "My activity"
                    color: root.strongText
                    font.pixelSize: 16
                    font.bold: true
                }
                Item { width: parent.width - parent.children[0].implicitWidth - historyButton.width - activityButton.width - 8; height: 1 }
                DmeButton { id: activityButton; width: 96; text: stack.currentIndex === 2 ? "Timer" : "My activity"; onClicked: stack.currentIndex = stack.currentIndex === 2 ? 0 : 2 }
                Item { width: 8; height: 1 }
                DmeButton { id: historyButton; width: 76; text: stack.currentIndex === 1 ? "Back" : "History"; onClicked: stack.currentIndex = stack.currentIndex === 1 ? 0 : 1 }
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
                        text: root.timerService.active ? "ADD OR SWITCH TASK" : "TASK NAME"
                        color: root.mutedText
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.6
                    }
                    Row {
                        width: parent.width
                        spacing: 8
                        DmeTextField {
                            id: taskField
                            width: parent.width - createTimerButton.width - parent.spacing
                            placeholderText: root.timerService.active
                                             ? "New task name..."
                                             : "What are you working on?"
                            onAccepted: root.begin()
                        }
                        DmeButton {
                            id: createTimerButton
                            width: 86
                            text: root.timerService.active ? "Switch" : "Start"
                            variant: "primary"
                            enabled: taskField.text.trim().length > 0
                                     && Backend.docMgr.currentDocumentId.length > 0
                            onClicked: root.begin()
                        }
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
                            model: root.timerService.taskTimers
                            delegate: Rectangle {
                                required property var modelData
                                width: taskTimerList.width
                                height: 42
                                radius: root.fluentTheme ? 0 : 3
                                color: modelData.active ? root.activeFill : (timerMouse.containsMouse ? root.hoverColor : "transparent")
                                border.width: modelData.active ? 1 : 0
                                border.color: root.accent
                                Row {
                                    anchors { fill: parent; leftMargin: 7; rightMargin: 4 }
                                    spacing: 7
                                    Text { anchors.verticalCenter: parent.verticalCenter; text: modelData.running ? "Ⅱ" : "▶︎"; color: root.accent; font.pixelSize: 11; font.bold: true }
                                    Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 122; text: modelData.name; color: root.strongText; font.pixelSize: 12; font.bold: modelData.active; elide: Text.ElideRight }
                                    Text { anchors.verticalCenter: parent.verticalCenter; width: 66; horizontalAlignment: Text.AlignRight; text: modelData.durationText; color: modelData.active ? root.strongText : root.mutedText; font.pixelSize: 11; font.family: "Consolas" }
                                    DmeButton {
                                        width: 28
                                        height: 28
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "×"
                                        variant: "danger"
                                        onClicked: {
                                            root.pendingTimerRemoval = modelData.name
                                            removeTimerConfirm.open()
                                        }
                                    }
                                }
                                MouseArea {
                                    id: timerMouse
                                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom; right: parent.right; rightMargin: 38 }
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.timerService.switchTask(modelData.name)
                                }
                            }
                        }
                        DmeScrollBar {
                            anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 2 }
                            flickable: taskTimerList
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: root.timerService.taskTimers.length === 0
                            text: "Create your first timer above"
                            color: root.subtleText
                            font.pixelSize: 11
                        }
                    }
                    DmePanel {
                        width: parent.width
                        height: 100
                        Column {
                            anchors { fill: parent; margins: 9 }
                            spacing: 3
                            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.timerService.taskElapsedText; color: root.strongText; font.pixelSize: 28; font.weight: Font.DemiBold; font.family: "Consolas" }
                            Text { text: (root.timerService.active ? root.timerService.taskName + "  ·  " : "") + (root.timerService.mapName || "No active map"); color: root.mutedText; width: parent.width; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 8
                                DmeButton { width: 82; visible: root.timerService.running; text: "Pause"; variant: "primary"; onClicked: root.timerService.pauseSession() }
                                DmeButton { width: 82; visible: root.timerService.active && !root.timerService.running; text: "Resume"; variant: "primary"; onClicked: root.timerService.resumeSession() }
                                DmeButton { width: 104; visible: root.timerService.active; text: "Finish task"; variant: "danger"; onClicked: root.timerService.finishSession(noteField.text) }
                                DmeButton { width: 82; text: root.showTimerDetails ? "Less" : "Details"; onClicked: root.showTimerDetails = !root.showTimerDetails }
                            }
                        }
                    }
                    Row {
                        visible: root.showTimerDetails
                        spacing: 8
                        DmeCheckBox { text: "Session checkpoints"; checked: root.timerService.checkpointEnabled; onClicked: root.timerService.checkpointEnabled = !checked }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "Break reminder"; color: root.mutedText; font.pixelSize: 11 }
                        DmeSpinBox { width: 70; from: 0; to: 480; value: root.timerService.breakReminderMinutes; onValueModified: root.timerService.breakReminderMinutes = value }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: "min"; color: root.mutedText; font.pixelSize: 11 }
                    }
                    DmeTextField { id: noteField; width: parent.width; visible: root.showTimerDetails && root.timerService.active; placeholderText: "Optional session note" }
                    Text { visible: root.showTimerDetails; text: "SESSION SUMMARY"; color: root.mutedText; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6 }
                    DmePanel {
                        visible: root.showTimerDetails
                        width: parent.width
                        height: 82
                        Grid {
                            anchors { fill: parent; margins: 9 }
                            columns: 2; columnSpacing: 42; rowSpacing: 7
                            Text { text: "Today   " + root.timerService.todayText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "This week   " + root.timerService.weekText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "This map   " + root.timerService.currentMapText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "Total   " + root.timerService.totalText; color: root.foreground; font.pixelSize: 11 }
                            Text { text: "Operations   " + root.timerService.operationCount; color: root.mutedText; font.pixelSize: 10 }
                            Text { text: "Changed tiles   " + root.timerService.changedTileCount; color: root.mutedText; font.pixelSize: 10 }
                        }
                    }
                    Row {
                        visible: root.showTimerDetails
                        spacing: 8
                        DmeCheckBox {
                            text: "Idle-Pause"
                            checked: root.timerService.idlePauseMinutes > 0
                            onClicked: root.timerService.idlePauseMinutes = checked ? 0 : 15
                        }
                        DmeSpinBox { width: 70; from: 0; to: 120; value: root.timerService.idlePauseMinutes; onValueModified: root.timerService.idlePauseMinutes = value }
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
                        model: root.timerService.history
                        delegate: Rectangle {
                            id: historyRow
                            required property var modelData
                            required property int index
                            width: historyList.width
                            height: 58
                            radius: root.fluentTheme ? 0 : 4
                            color: historyMouse.containsMouse ? root.hoverColor : (index % 2 ? root.panelColor : root.cardColor)
                            border.width: 1
                            border.color: root.borderColor
                            Column {
                                anchors { fill: parent; margins: 7 }
                                Text { width: parent.width; text: modelData.task + "  ·  " + root.timerService.formatDuration(modelData.durationSeconds); color: root.strongText; font.pixelSize: 12; elide: Text.ElideRight }
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
                                    if (seconds >= 0 && root.timerService.updateSession(root.selectedSession.id, editTask.text, editNote.text, seconds))
                                        root.selectedSession = null
                                }
                            }
                            DmeButton { text: "Delete"; onClicked: { root.timerService.removeSession(root.selectedSession.id); root.selectedSession = null } }
                            DmeButton { text: "Cancel"; onClicked: root.selectedSession = null }
                        }
                    }
                    Row {
                        spacing: 8
                        DmeButton { text: "Export CSV"; onClicked: csvDialog.open() }
                        DmeButton { text: "Export JSON"; onClicked: jsonDialog.open() }
                    }
                }
                Flickable {
                    contentWidth: width; contentHeight: personalActivity.implicitHeight
                    clip: true; boundsBehavior: Flickable.StopAtBounds
                    WorkActivityPanel {
                        id: personalActivity; width: parent.width; service: root.activityService
                        foreground: root.strongText; muted: root.mutedText; accent: root.accent
                        cardColor: root.cardColor; borderColor: root.borderColor
                        onAccountRequested: activityAccount.open()
                    }
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                }
            }
        }
    }
    ActivityDialog { id: activityAccount; service: root.activityService }

    DmeConfirmDialog {
        id: removeTimerConfirm
        title: "Delete timer"
        message: "Delete timer \"" + root.pendingTimerRemoval
                 + "\", including its current run and recorded sessions?"
        onAccepted: {
            root.timerService.removeTaskTimer(root.pendingTimerRemoval)
            root.pendingTimerRemoval = ""
        }
    }

    FileDialog { id: csvDialog; title: "Export work sessions as CSV"; fileMode: FileDialog.SaveFile; nameFilters: ["CSV files (*.csv)"]; defaultSuffix: "csv"; onAccepted: root.timerService.exportCsv(selectedFile) }
    FileDialog { id: jsonDialog; title: "Export work sessions as JSON"; fileMode: FileDialog.SaveFile; nameFilters: ["JSON files (*.json)"]; defaultSuffix: "json"; onAccepted: root.timerService.exportJson(selectedFile) }
}

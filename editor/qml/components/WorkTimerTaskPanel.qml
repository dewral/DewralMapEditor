import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Column {
    id: root
    enabled: typeof timerService.ready !== "boolean" || timerService.ready
    component TimerLabel: Text { font.family: "Segoe UI" }
    required property var timerService
    property bool canStart: true
    property string draftIcon: "mapping"
    property bool iconWasChosen: false
    property string note: ""
    property bool detailsExpanded: false
    property color panelColor: "#202122"
    property color cardColor: "#292b2d"
    property color borderColor: "#414447"
    property color foreground: "#f0f0f0"
    property color muted: "#9a9a9a"
    property color accent: "#49a4c5"
    signal iconRequested(string task, string icon)
    signal removeRequested(string task)
    signal detailsRequested()
    spacing: 14
    objectName: "workTimerTaskPanel"
    function chooseDraftIcon(icon) { draftIcon = icon; iconWasChosen = true; }
    function begin() {
        const name = taskField.text.trim();
        if (!name) return;
        const existed = timerService.taskTimers.some(task => task.name === name);
        const started = timerService.active ? timerService.switchTask(name) : timerService.startSession(name);
        if (!started) return;
        const saved = existed && !iconWasChosen ? true : (draftIcon.indexOf("file:") === 0
            ? timerService.importTaskIcon(name, draftIcon) : timerService.setTaskIcon(name, draftIcon));
        if (saved) { taskField.text = ""; draftIcon = "mapping"; iconWasChosen = false; }
    }
    Rectangle {
        id: hero
        objectName: "workTimerHero"
        width: parent.width; height: 214; radius: 7
        color: root.cardColor; border.color: root.borderColor
        Column {
            anchors { fill: parent; margins: 16 }
            spacing: 10
            RowLayout {
                width: parent.width; height: 48; spacing: 12
                Button {
                    objectName: "activeTaskIcon"
                    Layout.preferredWidth: 44; Layout.preferredHeight: 44
                    enabled: root.timerService.active
                    onClicked: root.iconRequested(root.timerService.taskName, root.timerService.taskIcon || "mapping")
                    background: Rectangle { radius: 6; color: "transparent"; border.color: parent.hovered ? root.accent : root.borderColor }
                    contentItem: TaskIcon { icon: root.timerService.taskIcon || "mapping"; anchors.margins: 8 }
                    ToolTip.visible: hovered; ToolTip.text: "Change task icon"
                    Accessible.name: "Change active task icon"
                }
                Column {
                    Layout.fillWidth: true; spacing: 3
                    TimerLabel {
                        text: root.timerService.active ? (root.timerService.running ? "●  TRACKING" : "●  PAUSED") : "READY TO MAP"
                        color: root.timerService.running ? "#62bd98" : root.muted; font.pixelSize: 10; font.letterSpacing: 0.7
                    }
                    TimerLabel { width: parent.width; text: root.timerService.active ? root.timerService.taskName : "Start your next task"; color: root.foreground; font.pixelSize: 19; font.weight: Font.DemiBold; elide: Text.ElideRight }
                    TimerLabel { width: parent.width; text: root.timerService.mapName || "Open a map to begin"; color: root.muted; font.pixelSize: 11; elide: Text.ElideMiddle }
                }
            }
            TimerLabel { objectName: "workTimerHeroTime"; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.timerService.taskElapsedText; color: root.foreground; font.family: "Consolas"; font.pixelSize: 40; font.weight: Font.DemiBold }
            TimerLabel { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: "Tracked time"; color: root.muted; font.pixelSize: 11 }
            Rectangle { width: parent.width; height: 1; color: root.borderColor }
            RowLayout {
                width: parent.width; spacing: 8
                DmeButton {
                    objectName: "workTimerResumePause"
                    Layout.fillWidth: true; Layout.preferredHeight: 34
                    enabled: root.timerService.active
                    text: root.timerService.running ? "Ⅱ  Pause" : "▶  Resume"; variant: "primary"
                    onClicked: root.timerService.running ? root.timerService.pauseSession() : root.timerService.resumeSession()
                }
                DmeButton { objectName: "workTimerFinish"; Layout.preferredWidth: 110; Layout.preferredHeight: 34; enabled: root.timerService.active; text: "■  Finish task"; onClicked: root.timerService.finishSession(root.note) }
                DmeButton { objectName: "workTimerDetails"; Layout.preferredWidth: 76; Layout.preferredHeight: 34; text: root.detailsExpanded ? "Less" : "Details"; onClicked: root.detailsRequested() }
            }
        }
    }
    RowLayout {
        width: parent.width; height: 22
        TimerLabel { text: "Task timers"; color: root.foreground; font.pixelSize: 14; font.weight: Font.DemiBold }
        Rectangle {
            Layout.preferredWidth: 26; Layout.preferredHeight: 21; radius: 10; color: root.cardColor
            TimerLabel { anchors.centerIn: parent; text: root.timerService.taskTimers.length; color: root.muted; font.pixelSize: 11 }
        }
        Item { Layout.fillWidth: true }
    }
    Item {
        width: parent.width; height: root.timerService.taskTimers.length ? Math.min(3, root.timerService.taskTimers.length) * 62 : 62
        ListView {
            id: tasks
            objectName: "workTimerTaskList"
            anchors.fill: parent
            clip: true; spacing: 6
            model: root.timerService.taskTimers
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: row
                required property var modelData
                objectName: "workTimerTask_" + modelData.name
                width: tasks.width - 10; height: 56; radius: 6
                color: modelData.active || rowHover.containsMouse ? root.cardColor : root.panelColor
                border.color: modelData.active ? root.accent : root.borderColor
                Rectangle { width: 3; height: parent.height - 12; y: 6; radius: 1; color: root.accent; visible: row.modelData.active }
                MouseArea { id: rowHover; anchors.fill: parent; hoverEnabled: true; onClicked: root.timerService.switchTask(row.modelData.name) }
                RowLayout {
                    anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                    spacing: 10
                    Button {
                        objectName: "taskIcon_" + row.modelData.name
                        Layout.preferredWidth: 34; Layout.preferredHeight: 34
                        background: Rectangle { radius: 4; color: "transparent"; border.color: parent.hovered ? root.accent : root.borderColor }
                        contentItem: TaskIcon { icon: row.modelData.icon || "mapping" }
                        onClicked: root.iconRequested(row.modelData.name, row.modelData.icon || "mapping")
                        ToolTip.visible: hovered; ToolTip.text: "Change task icon"
                        Accessible.name: "Change icon for " + row.modelData.name
                    }
                    TimerLabel { Layout.fillWidth: true; text: row.modelData.name; color: root.foreground; font.pixelSize: 12; font.weight: row.modelData.active ? Font.DemiBold : Font.Normal; elide: Text.ElideRight }
                    Column {
                        Layout.preferredWidth: 82; spacing: 3
                        TimerLabel { width: parent.width; horizontalAlignment: Text.AlignRight; text: row.modelData.durationText; color: root.foreground; font.family: "Consolas"; font.pixelSize: 12 }
                        TimerLabel { width: parent.width; horizontalAlignment: Text.AlignRight; text: row.modelData.running ? "Tracking" : row.modelData.active ? "Paused" : "Idle"; color: root.muted; font.pixelSize: 10 }
                    }
                    DmeButton {
                        objectName: "taskMenu_" + row.modelData.name
                        Layout.preferredWidth: 28; Layout.preferredHeight: 30; text: "⋯"
                        onClicked: { taskMenu.task = row.modelData.name; taskMenu.taskIcon = row.modelData.icon || "mapping"; taskMenu.popup(this, 0, height); }
                    }
                }
            }
        }
        TimerLabel { anchors.centerIn: parent; visible: !root.timerService.taskTimers.length; text: "Add your first task below"; color: root.muted; font.pixelSize: 12 }
    }
    Rectangle { width: parent.width; height: 1; color: root.borderColor }
    Column {
        width: parent.width; spacing: 7
        TimerLabel { text: "Add or switch task"; color: root.muted; font.pixelSize: 11 }
        RowLayout {
            width: parent.width; spacing: 8
            Button {
                objectName: "newTaskIcon"
                Layout.preferredWidth: 34; Layout.preferredHeight: 34
                contentItem: TaskIcon { icon: root.draftIcon }
                background: Rectangle { color: "transparent"; radius: 4; border.color: parent.hovered ? root.accent : root.borderColor }
                onClicked: root.iconRequested("", root.draftIcon)
                ToolTip.visible: hovered; ToolTip.text: "Choose icon for new task"
                Accessible.name: "Choose icon for new task"
            }
            DmeTextField { id: taskField; objectName: "workTimerNewTaskName"; Layout.fillWidth: true; Layout.preferredHeight: 34; placeholderText: "New task name…"; onAccepted: root.begin() }
            DmeButton { objectName: "workTimerAddTask"; Layout.preferredWidth: 100; Layout.preferredHeight: 34; text: "+ Add task"; variant: "primary"; enabled: taskField.text.trim().length > 0 && root.canStart; onClicked: root.begin() }
        }
    }
    TimerLabel { width: parent.width; text: root.timerService.errorString || ""; visible: text.length > 0; wrapMode: Text.WordWrap; color: "#e68a8a"; font.pixelSize: 11 }
    DmeMenu {
        id: taskMenu
        property string task: ""
        property string taskIcon: "mapping"
        DmeMenuItem { text: "Change icon…"; onTriggered: root.iconRequested(taskMenu.task, taskMenu.taskIcon) }
        MenuSeparator { }
        DmeMenuItem { objectName: "workTimerDeleteTask"; text: "Delete task…"; onTriggered: root.removeRequested(taskMenu.task) }
    }
}

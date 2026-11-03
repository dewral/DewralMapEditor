import QtQuick.Dialogs

FolderDialog {
    required property var startupWindow
    parentWindow: typeof startupWindow.app.editorWindow !== "undefined" && startupWindow.app.editorWindow
                  ? startupWindow.app.editorWindow : startupWindow
    title: "Select client folder for " + startupWindow.app.profileLabel(startupWindow.app.pendingKey)
           + " (Tibia.dat / Tibia.spr / items.otb)"
    onAccepted: startupWindow.app.onVersionFolderPicked(selectedFolder)
    onRejected: startupWindow.loadingMap = false
}

import QtQuick.Dialogs
import Tibia 1.0

FileDialog {
    required property var startupWindow
    title: "Open .otbm map"
    nameFilters: ["OTBM maps (*.otbm)", "All files (*)"]
    onAccepted: startupWindow.beginLoadMap(Backend.fileTools.toLocalFile(selectedFile), startupWindow.selectedProfileKey)
}

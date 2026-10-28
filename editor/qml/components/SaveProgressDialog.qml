import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: dialog
    objectName: "saveProgressDialog"
    property string fileName: ""
    title: "Saving map…"
    width: 400
    movable: false
    closePolicy: Popup.NoAutoClose
    standardButtons: Dialog.NoButton
    // The feedback must be fully visible in the frame preceding the writer.
    enter: Transition {}
    exit: Transition {}
    contentItem: Column {
        spacing: 12
        Text {
            width: parent.width
            text: dialog.fileName
            elide: Text.ElideMiddle
            color: dialog.windowsClassic ? "#202020" : "#e0e0e0"
            font.family: dialog.tibiaTheme ? "Verdana" : Qt.application.font.family
            font.bold: true
        }
        Text {
            width: parent.width
            text: "Please wait until saving is complete."
            wrapMode: Text.WordWrap
            color: dialog.windowsClassic ? "#505050" : "#aaaaaa"
            font.family: dialog.tibiaTheme ? "Verdana" : Qt.application.font.family
        }
    }
}

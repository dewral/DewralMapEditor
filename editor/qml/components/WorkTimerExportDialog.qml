import QtQuick.Dialogs
FileDialog {
    required property var timerService
    property bool csv: true
    title: csv ? "Export work sessions as CSV" : "Export work sessions as JSON"
    fileMode: FileDialog.SaveFile
    nameFilters: csv ? ["CSV files (*.csv)"] : ["JSON files (*.json)"]
    defaultSuffix: csv ? "csv" : "json"
    onAccepted: { if (csv) timerService.exportCsv(selectedFile); else timerService.exportJson(selectedFile); }
}

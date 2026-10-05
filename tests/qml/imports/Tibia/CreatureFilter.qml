import QtQuick

ListModel {
    property var sourceModel
    property string searchText: ""
    property string typeFilter: "all"
    function rowForCreature(name, isNpc) { return -1; }
}

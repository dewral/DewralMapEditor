import QtQuick

// A Loader sizes its item, so reading item.height feeds that size back into
// itself. Use the component's natural height when switching or unloading it.
Loader {
    height: item ? item.implicitHeight : 0
}

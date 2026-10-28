import QtQuick

// TibiaFrame3Pixel/TibiaFrame8Pic: four native corners and four tiled edges.
Item {
    id: frame
    Image { x: 0; y: 0; width: 3; height: 3; source: Qt.resolvedUrl("images/3pixel-frame-top-left.png"); smooth: false }
    Image { x: frame.width-3; y: 0; width: 3; height: 3; source: Qt.resolvedUrl("images/3pixel-frame-top-right.png"); smooth: false }
    Image { x: 0; y: frame.height-3; width: 3; height: 3; source: Qt.resolvedUrl("images/3pixel-frame-bottom-left.png"); smooth: false }
    Image { x: frame.width-3; y: frame.height-3; width: 3; height: 3; source: Qt.resolvedUrl("images/3pixel-frame-bottom-right.png"); smooth: false }
    Image { x: 3; y: 0; width: Math.max(0,frame.width-6); height: 3; fillMode: Image.Tile; source: Qt.resolvedUrl("images/3pixel-frame-top.png"); smooth: false }
    Image { x: 3; y: frame.height-3; width: Math.max(0,frame.width-6); height: 3; fillMode: Image.Tile; source: Qt.resolvedUrl("images/3pixel-frame-bottom.png"); smooth: false }
    Image { x: 0; y: 3; width: 3; height: Math.max(0,frame.height-6); fillMode: Image.Tile; source: Qt.resolvedUrl("images/3pixel-frame-left.png"); smooth: false }
    Image { x: frame.width-3; y: 3; width: 3; height: Math.max(0,frame.height-6); fillMode: Image.Tile; source: Qt.resolvedUrl("images/3pixel-frame-right.png"); smooth: false }
}

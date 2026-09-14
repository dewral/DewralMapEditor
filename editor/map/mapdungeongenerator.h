#ifndef MAPDUNGEONGENERATOR_H
#define MAPDUNGEONGENERATOR_H

#include <QPoint>
#include <QRect>
#include <QVector>

class MapDungeonGenerator
{
public:
    enum class TileKind : quint8 { Room, Corridor, Entrance, Boss };
    enum class Style : quint8 { Tomb, Cave, Mixed };
    enum class Layout : quint8 { Rooms, OrganicCave };
    enum class RoomShape : quint8 { Rectangle, Ellipse, Cross, Irregular };

    struct Settings {
        quint32 seed = 1;
        int roomCount = 24;
        int minWidth = 6;
        int minHeight = 6;
        int maxWidth = 14;
        int maxHeight = 12;
        int spacing = 2;
        int corridorWidth = 2;
        int loopPercent = 30;
        int corridorWinding = 10;
        int maxRoomDegree = 4;
        int caveDensity = 52;
        int caveSmoothSteps = 4;
        int caveMinRegionSize = 24;
        int caveWallThreshold = 32;
        Style style = Style::Mixed;
        Layout layout = Layout::Rooms;
    };

    struct Room {
        QRect bounds;
        TileKind kind = TileKind::Room;
        RoomShape shape = RoomShape::Rectangle;
        quint32 variationSeed = 0;
    };

    struct Connection { int first = -1; int second = -1; };
    struct Tile { int x = 0; int y = 0; TileKind kind = TileKind::Room; };

    struct Result {
        QVector<Room> rooms;
        QVector<Connection> connections;
        QVector<Tile> tiles;
        QVector<QPoint> doorways;
        int deadEnds = 0;
        int corridorLength = 0;
        bool fullyConnected = false;
    };

    static Result generate(const QVector<QPoint> &selection, const Settings &settings);
};

#endif

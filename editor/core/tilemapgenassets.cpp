#include "tilemapgenassets.h"
#include "datreader.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <algorithm>

namespace TileMapGenAssets {
QJsonObject geometry(const ClientItem &item, int rotateTo) {
    return {{"pattern_x", std::max(1, int(item.pattern_x))},
            {"pattern_y", std::max(1, int(item.pattern_y))},
            {"pattern_z", std::max(1, int(item.pattern_z))},
            {"elevation", item.has(ClientProperty::Elevation) ? item.elevation : 0},
            {"stackable", item.has(ClientProperty::Stackable)}, {"rotate_to", rotateTo}};
}

QString writeItem(QSqlDatabase &db, int serverId, const ClientItem &item,
                  const std::function<QImage(int)> &imageForPattern) {
    const int count = std::max(1, int(item.pattern_x)) * std::max(1, int(item.pattern_y))
                    * std::max(1, int(item.pattern_z));
    for (int pattern = 0; pattern < count; ++pattern) {
        const auto image = imageForPattern(pattern);
        if (image.isNull()) continue;
        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
            return QCoreApplication::translate("TileMapGenBridge", "Cannot encode sprite preview.");
        QSqlQuery query(db);
        // Older readers continue to use assets; new readers select a pattern
        // from sprite_variants using position or stack count.
        query.prepare(pattern == 0 ? "INSERT INTO assets VALUES(?,?)"
                                   : "INSERT INTO sprite_variants VALUES(?,?,?)");
        query.addBindValue(serverId);
        if (pattern != 0) query.addBindValue(pattern);
        query.addBindValue(png);
        if (!query.exec())
            return QCoreApplication::translate("TileMapGenBridge", "Cannot write sprite preview.");
    }
    return {};
}
}

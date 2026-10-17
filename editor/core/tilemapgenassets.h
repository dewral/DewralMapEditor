#pragma once
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <functional>

struct ClientItem;
class QSqlDatabase;

namespace TileMapGenAssets {
// Optional preview data stays outside the strict v1 pack manifest.
QJsonObject geometry(const ClientItem &item, int rotateTo);
QString writeItem(QSqlDatabase &db, int serverId, const ClientItem &item,
                  const std::function<QImage(int)> &imageForPattern);
}

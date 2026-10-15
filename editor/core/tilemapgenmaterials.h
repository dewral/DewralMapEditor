#pragma once
#include <QJsonObject>
#include <QHash>
#include <QVariantList>
namespace TileMapGenMaterials {
QVariantList groundChoices(const QJsonObject &catalog, const QHash<QString, int> &previewIds = {});
QString paletteError(const QJsonObject &catalog, const QVariantMap &palette);
}

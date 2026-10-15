#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QVariantList>
namespace TileMapGenMaterials {
QJsonArray uniqueCreatures(const QJsonArray &creatures);
QVariantList groundChoices(const QJsonObject &catalog, const QHash<QString, int> &previewIds = {});
QVariantList wallChoices(const QJsonObject &catalog, const QHash<QString, int> &previewIds = {},
                        const QSet<int> *visibleIds = nullptr);
QString paletteError(const QJsonObject &catalog, const QVariantMap &palette);
}

#include "brushstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

namespace {

QVariantMap variant(int id, int chance)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("chance"), chance}};
}

QVariantList variantSlots(int firstId, int secondId)
{
    QVariantList alignments;
    alignments.append(QVariant::fromValue(QVariantList()));
    for (int i = 1; i < 13; ++i)
        alignments.append(QVariant::fromValue(
            QVariantList{variant(firstId, 1), variant(secondId, 3)}));
    return alignments;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;

    QJsonArray legacyBorder;
    legacyBorder.append(0);
    for (int i = 1; i < 13; ++i) legacyBorder.append(101);

    QJsonObject root;
    root.insert(QStringLiteral("borders"), QJsonObject{{QStringLiteral("legacy"), legacyBorder}});
    root.insert(QStringLiteral("grounds"), QJsonObject{
        {QStringLiteral("grass"), QJsonObject{
            {QStringLiteral("zorder"), 100},
            {QStringLiteral("lookid"), 1},
            {QStringLiteral("friends"), QJsonArray{QStringLiteral("grass"), QStringLiteral("*")}},
            {QStringLiteral("solo_optional"), true},
            {QStringLiteral("hate_friends"), true},
            {QStringLiteral("custom"), QStringLiteral("preserved")},
            {QStringLiteral("items"), QJsonArray{QJsonArray{1, 1}}},
            {QStringLiteral("borders"), QJsonArray{QJsonObject{
                {QStringLiteral("align"), QStringLiteral("inner")},
                {QStringLiteral("to"), QString()},
                {QStringLiteral("border"), QStringLiteral("legacy")}
            }}}
        }},
        {QStringLiteral("stone"), QJsonObject{
            {QStringLiteral("items"), QJsonArray{QJsonArray{2, 1}}},
            {QStringLiteral("friends"), QJsonArray{QStringLiteral("grass")}},
            {QStringLiteral("borders"), QJsonArray{QJsonObject{
                {QStringLiteral("align"), QStringLiteral("inner")},
                {QStringLiteral("to"), QStringLiteral("grass")},
                {QStringLiteral("border"), QStringLiteral("gb_grass__inner_empty")}
            }}}
        }}
    });

    QFile sourceFile(dir.filePath(QStringLiteral("brushes.json")));
    if (!sourceFile.open(QIODevice::WriteOnly)) return 2;
    if (sourceFile.write(QJsonDocument(root).toJson()) <= 0) return 3;
    sourceFile.close();

    BrushStore store;
    if (!store.loadForDir(dir.path())) return 4;
    BrushStore mountainStore;
    const QString profile = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
        + QStringLiteral("/../data/1098");
    if (!mountainStore.loadForDir(profile)) return 14;
    if (mountainStore.rotatedSelectionBorderItem(4456, 1) != 4457
        || mountainStore.rotatedSelectionBorderItem(4456, 2) != 4458
        || mountainStore.rotatedSelectionBorderItem(4460, 2) != 4462
        || mountainStore.rotatedSelectionBorderItem(4464, 2) != 4466
        || mountainStore.rotatedSelectionBorderItem(4456, -1) != 4459) return 15;
    for (int id = 4456; id <= 4467; ++id) {
        int rotated = id;
        for (int turn = 0; turn < 4; ++turn)
            rotated = mountainStore.rotatedSelectionBorderItem(rotated, 1);
        if (rotated != id) return 16;
    }
    BrushStore legacyMountainStore;
    if (!legacyMountainStore.loadForDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
        + QStringLiteral("/../data/772"))) return 17;
    for (int id = 4456; id <= 4467; ++id)
        for (int turns = 0; turns < 4; ++turns)
            if (legacyMountainStore.rotatedSelectionBorderItem(id, turns)
                != mountainStore.rotatedSelectionBorderItem(id, turns)) return 18;
    if (legacyMountainStore.rotatedSelectionWallItem(1037, -1) != 1036
        || legacyMountainStore.rotatedSelectionWallItem(1040, -1) != 1039
        || legacyMountainStore.rotatedSelectionWallItem(1040, 1) != 1041
        || legacyMountainStore.rotatedSelectionWallItem(1596, -1) != 1600) return 19;
    for (int id : {1036, 1037, 1596, 1600}) {
        int rotated = id;
        for (int turn = 0; turn < 4; ++turn)
            rotated = legacyMountainStore.rotatedSelectionWallItem(rotated, 1);
        if (rotated != id) return 20;
    }
    if (legacyMountainStore.computeWallItem(QStringLiteral("framework wall"), true, true, false, false) != 1040
        || legacyMountainStore.computeWallItem(QStringLiteral("framework wall"), false, true, false, true) != 1039
        || legacyMountainStore.computeWallItem(QStringLiteral("wooden railing"), true, false, false, true) != 1596
        || legacyMountainStore.computeWallItem(QStringLiteral("wooden railing"), false, true, true, false) != 1600)
        return 21;

    const QVariantMap original = store.groundBrushEdit(QStringLiteral("grass"));
    const QVariantList originalBlocks = original.value(QStringLiteral("borders")).toList();
    if (originalBlocks.size() != 1) return 5;
    const QVariantList originalSlots = originalBlocks.first().toMap()
                                           .value(QStringLiteral("tiles")).toList();
    const QVariantList originalNorth = originalSlots.at(1).toList();
    if (originalNorth.size() != 1
        || originalNorth.first().toMap().value(QStringLiteral("id")).toInt() != 101)
        return 6;

    QVariantMap block;
    block.insert(QStringLiteral("align"), QStringLiteral("inner"));
    block.insert(QStringLiteral("to"), QString());
    block.insert(QStringLiteral("tiles"), variantSlots(101, 102));
    const QVariantList items{variant(1, 1)};
    const QVariantList optional = variantSlots(201, 202);
    if (!store.saveGroundBrush(QStringLiteral("grass"), 100, items,
                               QVariantList{block}, optional))
        return 7;

    QFile savedFile(dir.filePath(QStringLiteral("brushes.json")));
    if (!savedFile.open(QIODevice::ReadOnly)) return 8;
    const QJsonObject saved = QJsonDocument::fromJson(savedFile.readAll()).object();
    savedFile.close();
    const QJsonObject borders = saved.value(QStringLiteral("borders")).toObject();
    bool foundWeightedSlot = false;
    for (const QString &key : borders.keys()) {
        const QJsonArray alignments = borders.value(key).toArray();
        if (alignments.size() > 1 && alignments.at(1).isObject()
            && alignments.at(1).toObject().value(QStringLiteral("variants")).toArray().size() == 2) {
            foundWeightedSlot = true;
            break;
        }
    }
    if (!foundWeightedSlot) return 9;

    BrushStore reloaded;
    if (!reloaded.loadForDir(dir.path())) return 10;
    const QVariantList blocks = reloaded.groundBrushEdit(QStringLiteral("grass"))
                                    .value(QStringLiteral("borders")).toList();
    const QVariantList alignments = blocks.first().toMap().value(QStringLiteral("tiles")).toList();
    if (alignments.at(1).toList().size() != 2) return 11;

    bool sawFirst = false;
    bool sawSecond = false;
    const QStringList emptyNeighbours(8, QString());
    for (int i = 0; i < 200 && !(sawFirst && sawSecond); ++i) {
        const QVector<int> result = reloaded.computeBorderItems(
            QStringLiteral("grass"), emptyNeighbours, false);
        for (int id : result) {
            if (id == 101) sawFirst = true;
            else if (id == 102) sawSecond = true;
            else return 12;
        }
    }
    if (!(sawFirst && sawSecond)) return 13;

    const int revision = store.revision();
    if (store.saveGroundBrush(QStringLiteral("stone"), 100, items,
                              QVariantList{block}, optional, QStringLiteral("grass"))) return 22;
    if (store.saveGroundBrush(QStringLiteral("dirt"), 100, items,
                              QVariantList{block}, optional, QStringLiteral("missing"))) return 23;
    if (store.revision() != revision || !store.isGroundBrush(QStringLiteral("grass"))) return 24;

    int changedSignals = 0;
    QObject::connect(&store, &BrushStore::brushesChanged, [&] { ++changedSignals; });
    if (!store.saveGroundBrush(QStringLiteral("dirt"), 2000, items,
                               QVariantList{block}, optional, QStringLiteral("grass"))) return 25;
    if (changedSignals != 1 || store.isGroundBrush(QStringLiteral("grass"))
        || store.groundBrushForServerId(1) != QStringLiteral("dirt")) return 26;
    const QVariantMap dirt = store.groundBrushEdit(QStringLiteral("dirt"));
    if (dirt.value(QStringLiteral("borders")).toList() != QVariantList{block}) return 27;
    if (dirt.value(QStringLiteral("optionalTiles")).toList() != optional) return 28;

    BrushStore renamedReload;
    if (!renamedReload.loadForDir(dir.path())
        || renamedReload.isGroundBrush(QStringLiteral("grass"))
        || renamedReload.groundBrushEdit(QStringLiteral("dirt")) != dirt) return 29;
    savedFile.close();
    if (!savedFile.open(QIODevice::ReadOnly)) return 30;
    const QJsonObject renamed = QJsonDocument::fromJson(savedFile.readAll()).object();
    savedFile.close();
    const QJsonObject renamedGrounds = renamed.value(QStringLiteral("grounds")).toObject();
    const QJsonObject renamedDirt = renamedGrounds.value(QStringLiteral("dirt")).toObject();
    const QJsonObject stone = renamedGrounds.value(QStringLiteral("stone")).toObject();
    if (renamedDirt.value(QStringLiteral("custom")).toString() != QStringLiteral("preserved")
        || !renamedDirt.value(QStringLiteral("solo_optional")).toBool()
        || !renamedDirt.value(QStringLiteral("hate_friends")).toBool()
        || renamedDirt.value(QStringLiteral("friends")).toArray()
            != QJsonArray{QStringLiteral("dirt"), QStringLiteral("*")}
        || stone.value(QStringLiteral("friends")).toArray() != QJsonArray{QStringLiteral("dirt")}
        || stone.value(QStringLiteral("borders")).toArray().first().toObject()
            .value(QStringLiteral("to")).toString() != QStringLiteral("dirt")) return 31;
    if (!renamed.value(QStringLiteral("borders")).toObject()
             .contains(QStringLiteral("gb_grass__inner_empty"))) return 32;

    // A failed disk write must keep both the persisted brush and the in-memory draft intact.
    const QString savedPath = dir.filePath(QStringLiteral("saved.json"));
    if (!QFile::rename(sourceFile.fileName(), savedPath)
        || !QDir().mkdir(sourceFile.fileName())) return 33;
    if (store.saveGroundBrush(QStringLiteral("mud"), 2000, items,
                              QVariantList{block}, optional, QStringLiteral("dirt"))) return 34;
    if (changedSignals != 1 || store.groundBrushEdit(QStringLiteral("dirt")) != dirt
        || store.isGroundBrush(QStringLiteral("mud"))) return 35;
    if (!QDir().rmdir(sourceFile.fileName())
        || !QFile::rename(savedPath, sourceFile.fileName())) return 36;
    if (!store.saveGroundBrush(QStringLiteral("dirt"), 2500, items,
                               QVariantList{block}, {}, QStringLiteral("dirt"))) return 37;
    if (store.isGroundBrush(QStringLiteral("mud")) || store.groundBrushHasOptional(QStringLiteral("dirt"))) return 38;
    return 0;
}

#include "brushstore.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
            {QStringLiteral("items"), QJsonArray{QJsonArray{1, 1}}},
            {QStringLiteral("borders"), QJsonArray{QJsonObject{
                {QStringLiteral("align"), QStringLiteral("inner")},
                {QStringLiteral("to"), QString()},
                {QStringLiteral("border"), QStringLiteral("legacy")}
            }}}
        }}
    });

    QFile sourceFile(dir.filePath(QStringLiteral("brushes.json")));
    if (!sourceFile.open(QIODevice::WriteOnly)) return 2;
    if (sourceFile.write(QJsonDocument(root).toJson()) <= 0) return 3;
    sourceFile.close();

    BrushStore store;
    if (!store.loadForDir(dir.path())) return 4;

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
    return sawFirst && sawSecond ? 0 : 13;
}

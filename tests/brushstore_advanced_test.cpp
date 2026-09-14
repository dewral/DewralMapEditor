#include "brushstore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>

#define CHECK(expr) do { if (!(expr)) { qCritical() << "Failed at" << __LINE__ << #expr; return 1; } } while (false)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    QFile source(dir.filePath("brushes.json"));
    CHECK(source.open(QIODevice::WriteOnly));
    source.write(R"({"grounds":{},"walls":{"wall":{"lookid":100,"custom":"keep","items":{"6":[[100,1],[101,3],[102,0]],"9":[[103,1]],"16":[[104,1],[105,1]]}},"ambiguous":{"lookid":100,"items":{"9":[[100,1]]}}},"carpets":{"rug":{"lookid":200,"items":{"center":[[200,3],[201,7]],"cnw":[[202,1]]}}},"doors":{"door":{"custom":true}},"doodads":{}})");
    source.close();
    BrushStore store;
    CHECK(store.loadForDir(dir.path()));
    const auto wall = store.advancedBrushEdit("walls", "wall");
    CHECK(store.saveAdvancedBrush("walls", "wall", "wall", wall).value("success").toBool());
    CHECK(store.advancedBrushEdit("walls", "wall") == wall);
    CHECK(!store.saveAdvancedBrush("walls", "wall", "", wall).value("success").toBool());
    CHECK(!store.saveAdvancedBrush("walls", "renamed", "wall", wall).value("success").toBool());
    int first = 0, second = 0;
    for (int i = 0; i < 3000; ++i) {
        const int id = store.computeWallItem("wall", false, true, true, false);
        CHECK(id == 100 || id == 101);
        first += id == 100; second += id == 101;
    }
    CHECK(first > 450 && first < 1050 && second > first);
    CHECK(store.advancedBrushEdit("walls", "wall").value("items").toMap().value("16").toList().size() == 2);
    const auto rug = store.advancedBrushEdit("carpets", "rug");
    CHECK(store.saveAdvancedBrush("carpets", "rug", "rug", rug).value("success").toBool());
    CHECK(store.computeCarpetItem("rug", true,true,true,true,true,true,true,true) >= 200);
    QVariantList tiles{QVariantMap{{"dx",0},{"dy",0},{"dz",0},{"items",QVariantList{100,101,999}}},
                       QVariantMap{{"dx",1},{"dy",0},{"dz",1},{"items",QVariantList{101,202}}}};
    auto learned = store.learnBrushSelection("walls", tiles);
    CHECK(learned.value("unassigned").toList().size() == 3); // 100 ambiguous, 999/202 unknown
    auto align = learned.value("draft").toMap().value("items").toMap();
    CHECK(align.value("6").toList().first().toList() == QVariantList({101,2}));
    CHECK(store.learnBrushSelection("carpets", tiles).value("draft").toMap().value("items").toMap().contains("cnw"));
    auto doodad = store.learnBrushSelection("doodads", tiles).value("draft").toMap();
    doodad.insert("custom", "preserved");
    auto alts = doodad.value("alternates").toList();
    alts.append(QVariantMap{{"singles",QVariantList{QVariant::fromValue(QVariantList{300,9}),QVariant::fromValue(QVariantList{301,0})}}, {"composites",QVariantList{}}});
    doodad.insert("alternates", alts);
    CHECK(store.saveAdvancedBrush("doodads", "learned", "", doodad).value("success").toBool());
    CHECK(store.advancedBrushEdit("doodads", "learned") == doodad);
    CHECK(store.doodadVariantCount("learned") == 2);
    CHECK(store.doodadVariantTiles("learned", 0).size() == 2);
    CHECK(store.doodadVariantTiles("learned", 0)[1].dz == 1);
    auto invalid = doodad;
    invalid.insert("lookid", 1.5);
    CHECK(!store.saveAdvancedBrush("doodads", "learned", "learned", invalid).value("success").toBool());
    CHECK(store.advancedBrushEdit("doodads", "learned") == doodad);
    BrushStore reload;
    CHECK(reload.loadForDir(dir.path()));
    CHECK(reload.advancedBrushEdit("doodads", "learned") == doodad);
    CHECK(reload.advancedBrushEdit("walls", "wall") == wall);
    CHECK(source.open(QIODevice::ReadOnly));
    CHECK(QJsonDocument::fromJson(source.readAll()).object().value("doors").toObject().contains("door"));
    qInfo() << "Advanced brushes: round trips, validation, learning and weighted picking passed";
    return 0;
}

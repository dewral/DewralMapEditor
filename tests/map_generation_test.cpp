#include "../editor/map/mapproceduralbrush.h"
#include "generationlibrary.h"
#include "mapgeneration.h"
#include "otbmreader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
using namespace MapGeneration;
namespace
{
bool check(bool b, const char *s)
{
    if (!b)
        qCritical() << s;
    return b;
}
QVariantMap cell(int x, int y, int z = 0, int id = 100)
{
    return {{"x", x},
            {"y", y},
            {"z", z},
            {"items", QVariantList{QVariantMap{{"id", id}, {"count", 1}, {"ground", true}}}}};
}
QVariantList area(int w, int h, int ox = 0, int oy = 0, int z = 0)
{
    QVariantList a;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            a.append(cell(x + ox, y + oy, z));
    return a;
}
QVariantMap info()
{
    QVariantMap out;
    for (int id : {100, 101, 200, 300})
        out[QString::number(id)] =
            QVariantMap{{"valid", true}, {"ground", id < 200}, {"blocked", id == 200 || id == 101}};
    return out;
}
bool wait(GenerationLibrary &lib)
{
    QElapsedTimer t;
    t.start();
    while (lib.busy() && t.elapsed() < 10000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return !lib.busy() && lib.result().value("success").toBool();
}
} // namespace
#include "raw_map_generation_checks.h"
#include "procedural_generation_checks.h"
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if(!testRawMapGeneration()||!testProceduralGeneration())return 1;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    auto sample = area(9, 9);
    for (int i = 0; i < sample.size(); ++i)
        if (i % 9 == 4)
        {
            auto c = sample[i].toMap();
            auto items = c.value("items").toList();
            items[0] = QVariantMap{{"id", 101}, {"count", 1}, {"ground", true}};
            c["items"] = items;
            sample[i] = c;
        }
    auto model = learn({QVariantMap{{"tiles", sample}}}, {}, cancel);
    if (!check(model.value("success").toBool(), "Could not learn cave terrain"))
        return 1;
    // A custom client's ground must never suppress all its decoration, even
    // when an older snapshot incorrectly marked that ground as structural.
    {
        auto snow = area(15, 15);
        for (int y = 0; y < 15; ++y)
            for (int x = 0; x < 15; ++x)
            {
                auto t = cell(x, y, 0, x < 7 ? 103 : 100);
                auto items = t.value("items").toList();
                auto g = items.first().toMap();
                g["structural"] = true;
                items[0] = g;
                if (x == 7 && y > 0 && y < 14)
                    items.append(QVariantMap{{"id", 501}, {"count", 1}, {"ground", false}});
                if (x == 11 && y == 7)
                    items.append(QVariantMap{{"id", 300}, {"count", 1}, {"ground", false}});
                t["items"] = items;
                snow[y * 15 + x] = t;
            }
        auto custom = learn({QVariantMap{{"tiles", snow}}}, {}, cancel);
        bool decoration = false, edge = false;
        for (const auto &v : custom.value("decorations").toList())
            for (const auto &i : v.toMap().value("items").toList())
                decoration |= i.toMap().value("id").toInt() == 300;
        for (const auto &v : custom.value("terrainEdges").toList())
            edge |= v.toMap().value("id").toInt() == 501;
        if (!check(decoration && edge, "Custom ground identities suppressed decoration or observed edges"))
            return 1;
    }
    auto forest = learn({QVariantMap{{"tiles", area(7, 7)}}}, {}, cancel);
    GenerationRequest request{forest,
                              {{"mask", area(8, 8, 50, 60, 7)}, {"seed", 123}},
                              {{"width", 256}, {"height", 256}, {"itemInfo", info()}}};
    auto a = generate(request, cancel), b = generate(request, cancel);
    if (!check(a.success && a.patch == b.patch, "Same seed did not reproduce the same map"))
        return 1;
    if (!check(a.patch.value("tiles").toList().size() == 64, "Generation wrote outside its mask"))
        return 1;
    auto protectedCell = cell(51, 61, 7);
    protectedCell["occupied"] = true;
    request.context["tiles"] = QVariantList{protectedCell};
    a = generate(request, cancel);
    if (!check(a.success && a.patch.value("tiles").toList().size() == 63,
               "Protected content was overwritten"))
        return 1;
    request.blueprint["replace"] = true;
    a = generate(request, cancel);
    if (!check(a.success && a.patch.value("tiles").toList().size() == 64, "Explicit replacement failed"))
        return 1;
    request.blueprint["paths"] = QVariantList{cell(49, 60, 7)};
    if (!check(!generate(request, cancel).success, "Accepted path outside the mask"))
        return 1;
    request.blueprint.remove("paths");
    auto incompatible = cell(49, 60, 7, 101);
    request.context["tiles"] = QVariantList{incompatible};
    if (!check(!generate(request, cancel).success, "Accepted impossible context transition"))
        return 1;
    request.blueprint["brush"] = QVariantMap{{"liveMap", true}, {"walkable", true}};
    auto onMap = generate(request, cancel);
    if (!check(onMap.success && onMap.patch.value("tiles").toList().size() == 64,
               "On-map painting rejected terrain different from its source sample"))
        return 1;
    for (const auto &v : onMap.patch.value("tiles").toList())
        if (!check(v.toMap().value("x").toInt() >= 50, "On-map brush modified its outside context"))
            return 1;
    // A live terrain stroke learns its interior from the source center, not
    // a patchwork of the example's background. Variants still share a family.
    {
        auto mixed = area(9, 9);
        for (int y = 2; y < 7; ++y)
            for (int x = 2; x < 7; ++x)
                mixed[y * 9 + x] = cell(x, y, 0, (x + y) % 2 ? 100 : 102);
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 9; ++x)
                if (x < 2 || x >= 7 || y < 2 || y >= 7)
                    mixed[y * 9 + x] = cell(x, y, 0, 103);
        auto terrain = learn({QVariantMap{{"tiles", mixed}}}, {}, cancel);
        terrain["focusGround"] = 100;
        auto metadata = info();
        metadata["100"] = QVariantMap{{"valid", true}, {"ground", true}, {"terrain", "path"}};
        metadata["102"] = QVariantMap{{"valid", true}, {"ground", true}, {"terrain", "path"}};
        metadata["103"] = QVariantMap{{"valid", true}, {"ground", true}, {"terrain", "background"}};
        GenerationRequest stroke{
            terrain,
            {{"mask", area(20, 7, 40, 40, 7)},
             {"replace", true},
             {"seed", 91},
             {"brush", QVariantMap{{"liveMap", true}, {"terrainPaint", true}, {"walkable", true}}}},
            {{"width", 256}, {"height", 256}, {"itemInfo", metadata}}};
        auto result = generate(stroke, cancel);
        if (!check(result.success && result.patch.value("autoBorders").toBool(),
                   "Terrain stroke did not request geometric borders"))
            return 1;
        QSet<int> variants;
        for (const auto &v : result.patch.value("tiles").toList())
        {
            auto items = v.toMap().value("items").toList();
            int id = items.first().toMap().value("id").toInt();
            if (!check(id == 100 || id == 102, "Source background was scattered inside the painted path"))
                return 1;
            variants.insert(id);
        }
        if (!check(variants.size() == 2 && result.patch == generate(stroke, cancel).patch,
                   "Terrain family variation or reproducibility was lost"))
            return 1;
    }
    request.blueprint.remove("brush");
    request.context["tiles"] = QVariantList{};
    QVariantMap prefab{
        {"name", "House"},
        {"tiles",
         QVariantList{QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QVariantList{100, 200}}},
                      QVariantMap{{"dx", 1}, {"dy", 0}, {"dz", -1}, {"items", QVariantList{100, 300}}}}}};
    request.model["prefabs"] = QVariantList{prefab};
    request.blueprint["pins"] = QVariantList{QVariantMap{{"name", "House"}, {"x", 50}, {"y", 60}, {"z", 7}}};
    a = generate(request, cancel);
    if (!check(a.success && a.patch.value("tiles").toList().size() == 65,
               "Whole multilevel prefab was not placed"))
        return 1;
    request.blueprint["pins"] = QVariantList{QVariantMap{{"name", "House"}, {"x", 57}, {"y", 60}, {"z", 7}}};
    if (!check(!generate(request, cancel).success, "Cut a prefab at the mask boundary"))
        return 1;
    request.blueprint["pins"] = QVariantList{QVariantMap{{"name", "House"}, {"x", 50}, {"y", 60}, {"z", 0}}};
    request.blueprint["mask"] = area(8, 8, 50, 60, 0);
    if (!check(!generate(request, cancel).success, "Placed a prefab above floor zero"))
        return 1;
    cancel->store(true);
    if (!check(!generate(request, cancel).success, "Ignored cancellation"))
        return 1;
    cancel->store(false);
    if (!check(!learn({QVariantMap{{"tiles", area(2, 2)}}}, {}, cancel).value("success").toBool(),
               "Unknown sample cells were treated as empty"))
        return 1;
    // A cave model retains its learned blocked/passable transitions.
    request = {model,
               {{"mask", area(7, 7, 40, 40, 7)}, {"seed", 44}},
               {{"width", 256}, {"height", 256}, {"itemInfo", info()}}};
    a = generate(request, cancel);
    if (!check(a.success, "Learned cave overlaps failed"))
        return 1;
    request.blueprint["brush"] = QVariantMap{{"walkable", true}};
    a = generate(request, cancel);
    if (!check(a.success, "A walkable procedural room could not be generated"))
        return 1;
    for (const auto &v : a.patch.value("tiles").toList())
        for (const auto &iv : v.toMap().value("items").toList())
            if (iv.toMap().value("ground").toBool() &&
                !check(iv.toMap().value("id").toInt() != 101,
                       "Procedural room filled its interior with blocked ground"))
                return 1;
    // Recognized buildings are whole prefab observations, never a pool of wall
    // pieces.
    auto built = area(9, 9);
    QVariantMap hut{
        {"name", "Hut"},
        {"tiles",
         QVariantList{QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QVariantList{100, 200}}},
                      QVariantMap{{"dx", 1}, {"dy", 0}, {"dz", 0}, {"items", QVariantList{100, 300}}}}}};
    for (int i : {40, 41})
    {
        auto c = built[i].toMap();
        auto stack = c.value("items").toList();
        stack.append(QVariantMap{{"id", i == 40 ? 200 : 300}, {"ground", false}, {"count", 1}});
        c["items"] = stack;
        built[i] = c;
    }
    auto learnedHut = learn({QVariantMap{{"tiles", built}}}, {hut}, cancel);
    if (!check(learnedHut.value("success").toBool() && learnedHut.value("occurrences").toList().size() == 1,
               "Exact prefab recognition failed"))
        return 1;
    for (const auto &v : learnedHut.value("decorations").toList())
        if (!check(v.toMap().value("items").toList().isEmpty(),
                   "A recognized prefab was decomposed into decorations"))
            return 1;
    request = {forest,
               {{"mask", area(300, 300, 100, 100, 7)}, {"seed", 8}},
               {{"width", 1024}, {"height", 1024}, {"itemInfo", info()}}};
    QElapsedTimer timer;
    timer.start();
    a = generate(request, cancel);
    if (!check(a.success && a.patch.value("tiles").toList().size() == 90000, "Large-mask generation failed"))
        return 1;
    qInfo() << "90000-cell generation ms:" << timer.elapsed();
    request = {forest,
               {{"mask", QVariantList{cell(50, 60, 7), cell(51, 60, 6)}}, {"seed", 1}},
               {{"width", 256}, {"height", 256}, {"itemInfo", info()}}};
    if (!check(!generate(request, cancel).success, "Generated terrain on multiple active floors"))
        return 1;
    {
        auto corridor = MapProceduralBrush::stroke({20, 20}, {40, 35}, 7, 50, 123);
        auto same = MapProceduralBrush::stroke({20, 20}, {40, 35}, 7, 50, 123);
        auto room = MapProceduralBrush::stroke({20, 20}, {20, 20}, 17, 65, 123, true);
        auto variant = MapProceduralBrush::stroke({20, 20}, {20, 20}, 17, 65, 987, true);
        if (!check(corridor.cells == same.cells && corridor.path == same.path && room.cells != variant.cells,
                   "Brush footprints are not deterministic or do not vary with seed"))
            return 1;
        QSet<quint64> cells;
        for (auto p : corridor.cells)
            cells.insert(key(p.x(), p.y(), 0));
        for (auto p : corridor.path)
            if (!check(cells.contains(key(p.x(), p.y(), 0)), "Guide escaped its corridor footprint"))
                return 1;
        QSet<quint64> reached;
        QVector<QPoint> queue{{20, 20}};
        reached.insert(key(20, 20, 0));
        for (int i = 0; i < queue.size(); ++i)
            for (auto delta : {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)})
            {
                auto p = queue[i] + delta;
                auto k = key(p.x(), p.y(), 0);
                if (cells.contains(k) && !reached.contains(k))
                {
                    reached.insert(k);
                    queue.append(p);
                }
            }
        if (!check(reached.size() == cells.size(), "A diagonal organic corridor has disconnected islands"))
            return 1;
        auto thin = MapProceduralBrush::stroke({20, 20}, {20, 20}, 3, 0, 1);
        auto wide = MapProceduralBrush::stroke({20, 20}, {20, 20}, 15, 0, 1);
        if (!check(wide.cells.size() > thin.cells.size(), "Brush width does not change its footprint"))
            return 1;
    }
    // Mandatory border stacks survive both spacing and walkable path guides.
    auto bordered = area(7, 7);
    for (auto &v : bordered)
    {
        auto c = v.toMap();
        auto stack = c.value("items").toList();
        stack.append(QVariantMap{{"id", 300}, {"count", 1}, {"ground", false}, {"border", true}});
        c["items"] = stack;
        c["monster"] = "Rat";
        v = c;
    }
    auto borderModel = learn({QVariantMap{{"tiles", bordered}}}, {}, cancel);
    request = {
        borderModel,
        {{"mask", area(5, 5, 20, 20, 7)}, {"seed", 12}, {"spacing", 16}, {"paths", area(5, 1, 20, 20, 7)}},
        {{"width", 256}, {"height", 256}, {"itemInfo", info()}}};
    a = generate(request, cancel);
    if (!check(a.success, "Border/path generation failed"))
        return 1;
    for (const auto &v : a.patch.value("tiles").toList())
        if (!check(v.toMap().value("items").toList().size() == 2 && !v.toMap().contains("monster"),
                   "Lost a mandatory border or generated a disabled monster"))
            return 1;
    request.blueprint["monsters"] = true;
    a = generate(request, cancel);
    int spawned = 0;
    for (const auto &v : a.patch.value("tiles").toList())
        if (v.toMap().contains("monster"))
            ++spawned;
    if (!check(a.success && spawned == 20, "Monster density or passable guide protection failed"))
        return 1;
    request = {forest,
               {{"mask", area(8, 8, 50, 60, 7)},
                {"seed", 1},
                {"pins", QVariantList{QVariantMap{{"name", "House"}, {"x", 50}, {"y", 60}, {"z", 7}}}}},
               {{"width", 256}, {"height", 256}, {"itemInfo", info()}}};
    request.model["prefabs"] = QVariantList{prefab};
    auto upper = cell(51, 60, 6);
    upper["occupied"] = true;
    request.context["tiles"] = QVariantList{upper};
    if (!check(!generate(request, cancel).success, "Prefab overwrote protected content on another floor"))
        return 1;
    request.blueprint["replace"] = true;
    if (!check(generate(request, cancel).success, "Explicit multilevel replacement failed"))
        return 1;
    QTemporaryDir dir;
    QString exampleId, styleId, blueprintId;
    {
        GenerationLibrary lib(nullptr, dir.path());
        if (!check(lib.selectProfile("772/test"), "Cannot select profile"))
            return 1;
        auto saved = lib.capture("Forest", {{"success", true}, {"tiles", area(7, 7)}});
        exampleId = saved.value("id").toString();
        if (!check(saved.value("success").toBool() && lib.learnStyle("Forest style", {exampleId}, {}, {}) &&
                       wait(lib),
                   "Async learning/persistence failed"))
            return 1;
        styleId = lib.result().value("id").toString();
        auto savedPlan = lib.saveBlueprint(
            "Forest plan", {{"styleId", styleId}, {"mask", area(5, 5, 20, 20, 7)}, {"seed", 3}});
        blueprintId = savedPlan.value("id").toString();
        if (!check(savedPlan.value("success").toBool() && !lib.remove(exampleId).value("success").toBool(),
                   "Broke referenced library resources"))
            return 1;
        if (!check(lib.selectProfile("860/other") && lib.entries().isEmpty() && lib.load(styleId).isEmpty(),
                   "Leaked resources across client profiles"))
            return 1;
    }
    {
        GenerationLibrary lib(nullptr, dir.path());
        if (!check(lib.selectProfile("772/test") && !lib.load(exampleId).value("tiles").toList().isEmpty() &&
                       lib.load(blueprintId).value("blueprint").toMap().value("seed").toInt() == 3,
                   "Copied samples or blueprints did not survive restart"))
            return 1;
        if (!check(lib.generate(styleId, {{"mask", area(5, 5, 20, 20, 7)}, {"seed", 3}},
                                {{"width", 256}, {"height", 256}, {"itemInfo", info()}}) &&
                       wait(lib),
                   "Async generation failed"))
            return 1;
    }
    {
        OtbmReader source;
        source.newMap(64, 64, 860, 2, 22);
        for (int y = 10; y < 16; ++y)
            for (int x = 10; x < 16; ++x)
                source.placeItem(x, y, 7, 100, 0, false, true);
        source.placeItem(11, 11, 7, 300, 1, false, false);
        source.setItemActionIdAt(11, 11, 7, 1, 500);
        source.setItemUniqueIdAt(11, 11, 7, 1, 600);
        source.setHouseTileAt(11, 11, 7, 99);
        source.setCreatureAt(11, 11, 7, "Manual NPC", 60, true);
        const auto file = dir.filePath("source.otbm");
        if (!check(source.saveFile(file), "Could not save import fixture"))
            return 1;
        GenerationLibrary lib(nullptr, dir.filePath("imports"));
        lib.selectProfile("772/test");
        if (!check(lib.importExample(
                       "Imported fragment", file,
                       {{"x", 10}, {"y", 10}, {"z", 7}, {"width", 3}, {"height", 3}, {"floors", 1}},
                       {{"itemInfo", info()}, {"otbMajor", 2}, {"otbMinor", 22}}) &&
                       wait(lib),
                   "Selected OTBM fragment import failed"))
            return 1;
        auto imported = lib.load(lib.result().value("id").toString());
        if (!check(imported.value("tiles").toList().size() == 9,
                   "Import analyzed or copied cells outside the chosen fragment"))
            return 1;
        for (const auto &v : imported.value("tiles").toList())
        {
            auto t = v.toMap();
            if (!check(!t.contains("houseId") && !t.contains("monster"), "Imported an NPC or house identity"))
                return 1;
            for (const auto &iv : t.value("items").toList())
            {
                auto i = iv.toMap();
                if (!check(!i.contains("actionId") && !i.contains("uniqueId") && !i.contains("teleport"),
                           "Copied unsafe item identities"))
                    return 1;
            }
        }
        QFile::remove(file);
        if (!check(!lib.load(imported.value("id").toString()).value("tiles").toList().isEmpty(),
                   "Sample depends on original OTBM file"))
            return 1;
        if (!check(lib.generate(styleId, {}, {}) == false, "Accepted a style from another library"))
            return 1;
    }
    {
        // OTBM 1 contains inline subtypes. An importer without the loaded OTB
        // schema used to fail with the exact item-8195 diagnostic shown in the UI.
        auto u16 = [](QByteArray &b, quint16 n)
        {
            b.append(char(n & 255));
            b.append(char(n >> 8));
        };
        auto u32 = [&](QByteArray &b, quint32 n)
        {
            u16(b, n & 65535);
            u16(b, n >> 16);
        };
        auto node = [](int type, const QByteArray &props, const QByteArray &children = QByteArray())
        {
            QByteArray b(1, char(0xfe)), data(1, char(type));
            data += props;
            for (char c : data)
            {
                if (quint8(c) >= 0xfd)
                    b.append(char(0xfd));
                b.append(c);
            }
            b += children;
            b.append(char(0xff));
            return b;
        };
        QByteArray header, area, vial;
        u32(header, 0);
        u16(header, 256);
        u16(header, 256);
        u32(header, 1);
        u32(header, 3);
        u16(area, 100);
        u16(area, 100);
        area.append(char(7));
        u16(vial, 8195);
        vial.append(char(7));
        vial.append(char(4));
        u16(vial, 1234);
        const auto path = dir.filePath("legacy-inline-subtype.otbm");
        QFile file(path);
        if (!check(file.open(QIODevice::WriteOnly), "Could not create legacy import fixture"))
            return 1;
        file.write(QByteArray(4, '\0') +
                   node(0, header, node(2, {}, node(4, area, node(5, QByteArray(2, '\0'), node(6, vial))))));
        file.close();
        GenerationLibrary lib(nullptr, dir.filePath("legacy-imports"));
        lib.selectProfile("772/test");
        QVariantMap types{
            {"8195", QVariantMap{{"valid", true}, {"ground", false}, {"subtype", true}, {"fluid", false}}}};
        if (!check(lib.importExample("Legacy sample", path,
                                     {{"x", 100}, {"y", 100}, {"z", 7}, {"width", 3}, {"height", 3}},
                                     {{"itemInfo", types}, {"otbMajor", 1}, {"otbMinor", 3}}) &&
                       wait(lib),
                   "Legacy importer did not receive matching OTB item definitions"))
            return 1;
        const auto tiles = lib.load(lib.result().value("id").toString()).value("tiles").toList();
        const auto items = tiles.first().toMap().value("items").toList();
        if (!check(tiles.size() == 9 && items.size() == 1 &&
                       items.first().toMap().value("id").toInt() == 8195 &&
                       items.first().toMap().value("count").toInt() == 7 &&
                       !items.first().toMap().contains("actionId"),
                   "Legacy import lost its inline subtype or copied its action "
                   "identity"))
            return 1;
    }
    qInfo() << "Map generation and library checks passed";
    return 0;
}

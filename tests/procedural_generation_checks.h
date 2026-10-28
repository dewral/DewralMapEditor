#pragma once
#include "terrainborderalignment.h"
static bool testProceduralGeneration()
{
    using namespace MapGeneration;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    QVariantMap meta{
        {"100", QVariantMap{{"valid", true}, {"ground", true}, {"surface", "floor"}}},
        {"101", QVariantMap{{"valid", true}, {"ground", true}, {"blocked", true}, {"surface", "rock"}}},
        {"300", QVariantMap{{"valid", true}}},
        {"301", QVariantMap{{"valid", true}}}};
    QVariantList samples;
    for (int shape = 0; shape < 3; ++shape)
    {
        auto inside = [&](int x, int y) {
            bool rect = x >= 3 && x <= 13 && y >= 3 && y <= 13;
            if (shape == 1)
                rect &= !(x >= 9 && y >= 9);
            if (shape == 2)
                rect &= !(x >= 7 && x <= 9 && y >= 7 && y <= 9);
            return rect;
        };
        QVariantList tiles;
        for (int y = 0; y < 17; ++y)
            for (int x = 0; x < 17; ++x)
            {
                bool in = inside(x, y);
                QVariantList items{QVariantMap{{"id", in ? 100 : 101},
                                               {"count", 1},
                                               {"ground", true},
                                               {"blocked", !in},
                                               {"surface", in ? "floor" : "rock"}}};
                if (in)
                {
                    int bits = 0;
                    const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
                    for (int n = 0; n < 8; ++n)
                        if (!inside(x + dx[n], y + dy[n]))
                            bits |= 1 << n;
                    quint32 code = TerrainBorderAlignment::kBorderTypes[bits];
                    for (int n = 0; n < 4; ++n)
                    {
                        int slot = (code >> (n * 8)) & 255;
                        if (!slot)
                            continue;
                        int id = 500 + slot;
                        meta[QString::number(id)] = QVariantMap{{"valid", true}};
                        items.append(QVariantMap{{"id", id}, {"count", 1}, {"ground", false}});
                    }
                }
                if (in && y == 5 && (x == 5 || x == 6))
                    items.append(QVariantMap{
                        {"id", x == 5 ? 300 : 301}, {"count", x == 5 ? 7 : 8}, {"ground", false}});
                tiles.append(QVariantMap{{"x", x}, {"y", y}, {"z", 0}, {"items", items}});
            }
        samples.append(QVariantMap{{"tiles", tiles}});
    }
    auto model = learnProcedural(samples, {}, {}, cancel);
    if (!check(model.value("success").toBool(), "Procedural source analysis failed"))
    {
        qCritical() << model;
        return false;
    }
    QVariantList mask;
    for (int y = 25; y < 36; ++y)
        for (int x = 25; x < 38; ++x)
            if (!(x >= 32 && y >= 32))
                mask.append(QVariantMap{{"x", x}, {"y", y}, {"z", 7}});
    GenerationRequest req{
        model,
        {{"mask", mask}, {"seed", 15}, {"replace", true}, {"brush", QVariantMap{{"procedural", true}}}},
        {{"width", 256}, {"height", 256}, {"itemInfo", meta}}};
    auto r = generate(req, cancel);
    if (!check(r.success, "Procedural L-shaped generation failed"))
    {
        qCritical() << r.error << r.diagnostics;
        return false;
    }
    if (!check(r.patch.value("tiles").toList().size() == mask.size(),
               "Procedural generation lost or added target cells"))
        return false;
    QSet<quint64> footprint;
    for (const auto &v : mask)
    {
        auto t = v.toMap();
        footprint.insert(key(t.value("x").toInt(), t.value("y").toInt(), 7));
    }
    for (const auto &v : r.patch.value("tiles").toList())
    {
        auto t = v.toMap();
        auto items = t.value("items").toList();
        if (!check(items.first().toMap().value("id").toInt() == 100,
                   "Copied exterior rock into a procedural room"))
            return false;
        for (const auto &iv : items)
        {
            auto i = iv.toMap();
            if (i.value("id").toInt() == 300 && !check(i.value("count").toInt() == 7, "Lost a motif subtype"))
                return false;
        }
    }
    if (!check(generate(req, cancel).patch == r.patch, "Procedural output is not repeatable"))
        return false;
    auto protectedCell = mask[3].toMap();
    protectedCell["protected"] = true;
    protectedCell["items"] = QVariantList{QVariantMap{{"id", 100}, {"ground", true}, {"count", 1}}};
    req.context["tiles"] = QVariantList{protectedCell};
    auto safe = generate(req, cancel);
    if (!check(safe.success && safe.patch.value("tiles").toList().size() == mask.size() - 1,
               "Protected procedural tile was overwritten"))
        return false;
    req.context["tiles"] = QVariantList{};
    req.blueprint["paths"] = QVariantList{mask[20], mask[40]};
    if (!check(generate(req, cancel).success, "Procedural guides were disconnected"))
        return false;
    cancel->store(true);
    if (!check(!generate(req, cancel).success, "Procedural cancellation ignored"))
        return false;
    cancel->store(false);
    auto unknown = mask.first().toMap();
    unknown["x"] = 24;
    unknown["items"] = QVariantList{QVariantMap{{"id", 999}, {"ground", true}}};
    req.context["tiles"] = QVariantList{unknown};
    auto badMeta = meta;
    badMeta["999"] = QVariantMap{{"valid", true}, {"ground", true}, {"surface", "unknown"}};
    req.context["itemInfo"] = badMeta;
    // Unknown context is protected; it cannot be silently rewritten as a learned wall.
    auto unknownResult = generate(req, cancel);
    if (!check(unknownResult.success, "Protected unknown outside content should remain untouched"))
        return false;

    req.context["tiles"] = QVariantList{};
    req.context["itemInfo"] = meta;
    req.blueprint.remove("paths");
    // New geometry must never split a connected two-cell authored decoration.
    bool foundMotif = false;
    for (int seed = 0; seed < 30 && !foundMotif; ++seed)
    {
        req.blueprint["seed"] = seed;
        auto generated = generate(req, cancel);
        if (!generated.success)
            return false;
        QMap<quint64, QVariantMap> tiles;
        for (const auto &v : generated.patch.value("tiles").toList())
        {
            auto t = v.toMap();
            tiles[key(t.value("x").toInt(), t.value("y").toInt(), 7)] = t;
        }
        for (auto it = tiles.cbegin(); it != tiles.cend(); ++it)
            for (const auto &iv : it->value("items").toList())
                if (iv.toMap().value("id").toInt() == 300)
                {
                    foundMotif = true;
                    bool partner = false;
                    for (const auto &other : tiles.value(it.key() + 1).value("items").toList())
                        partner |= other.toMap().value("id").toInt() == 301 &&
                                   other.toMap().value("count").toInt() == 8;
                    if (!check(partner, "Procedural synthesis cut a complete RAW motif"))
                        return false;
                }
    }
    if (!check(foundMotif, "Procedural synthesis lost all authored motifs"))
        return false;
    meta["302"] = QVariantMap{{"valid", true}, {"transition", true}};
    req.context["itemInfo"] = meta;
    QVariantMap segment{
        {"name", "Bridge segment"},
        {"tiles", QVariantList{QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QVariantList{100}}}}}};
    QVariantMap lower{{"name", "Lower floor"},
                      {"tiles", QVariantList{QVariantMap{
                                    {"dx", 0}, {"dy", 0}, {"dz", 1}, {"items", QVariantList{100, 302}}}}}};
    QVariantMap recipe{
        {"name", "Bridge and lower floor"},
        {"parts", QVariantList{QVariantMap{{"prefab", "Bridge segment"},
                                           {"dx", 0},
                                           {"dy", 0},
                                           {"dz", 0},
                                           {"repeat", true},
                                           {"stepX", 1}},
                               QVariantMap{{"prefab", "Lower floor"}, {"dx", 0}, {"dy", 0}, {"dz", 0}}}},
        {"connections", QVariantList{QVariantMap{{"from", QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 0}}},
                                                 {"to", QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 1}}}}}}};
    req.model["prefabs"] = QVariantList{segment, lower};
    req.model["recipes"] = QVariantList{recipe};
    req.blueprint["pins"] = QVariantList{QVariantMap{
        {"name", recipe.value("name")}, {"recipe", true}, {"x", 27}, {"y", 27}, {"z", 7}, {"span", 3}}};
    auto structure = generate(req, cancel);
    if (!check(structure.success, "Whole multilevel structure assembly failed"))
    {
        qCritical() << structure.error;
        return false;
    }
    bool lowerFloor = false;
    for (const auto &v : structure.patch.value("tiles").toList())
        lowerFloor |= v.toMap().value("z").toInt() == 8;
    if (!check(lowerFloor, "Structure lost its lower floor"))
        return false;
    QVariantMap protectedLower{{"x", 27}, {"y", 27}, {"z", 8}, {"protected", true}, {"occupied", true}};
    protectedLower["items"] = QVariantList{QVariantMap{{"id", 100}, {"ground", true}}};
    req.context["tiles"] = QVariantList{protectedLower};
    if (!check(!generate(req, cancel).success, "Multilevel structure overwrote a protected lower floor"))
        return false;
    req.context["tiles"] = QVariantList{};
    req.blueprint.remove("pins");
    // Shrinking closes the selected area and rebuilds its surrounding inside boundary.
    QVariantList closeMask, context;
    for (int y = 20; y < 40; ++y)
        for (int x = 20; x < 40; ++x)
            context.append(cell(x, y, 7, 100));
    for (int y = 27; y < 30; ++y)
        for (int x = 27; x < 30; ++x)
            closeMask.append(QVariantMap{{"x", x}, {"y", y}, {"z", 7}});
    req.context["tiles"] = context;
    req.blueprint["mask"] = closeMask;
    req.blueprint["brush"] = QVariantMap{{"procedural", true}, {"closeArea", true}};
    auto closed = generate(req, cancel);
    if (!check(closed.success, "Procedural shrinking failed"))
    {
        qCritical() << closed.error;
        return false;
    }
    for (const auto &v : closed.patch.value("tiles").toList())
    {
        auto t = v.toMap();
        int x = t.value("x").toInt(), y = t.value("y").toInt();
        if (x >= 27 && x < 30 && y >= 27 && y < 30 &&
            !check(t.value("items").toList().first().toMap().value("id").toInt() == 101,
                   "Closing brush left floor inside the closed area"))
            return false;
    }
    // Persisted plans pin their style, package import remaps references atomically.
    QTemporaryDir tmp;
    GenerationLibrary library(nullptr, tmp.path());
    library.selectProfile("procedural-test");
    QVariantList exampleIds;
    for (const auto &v : samples)
    {
        auto saved = library.capture("Source", {{"success", true}, {"tiles", v.toMap().value("tiles")}});
        exampleIds.append(saved.value("id"));
    }
    if (!check(library.learnStyle("Cave", exampleIds, {segment, lower}, {{"procedural", true}}) &&
                   wait(library),
               "Procedural library learning failed"))
        return false;
    auto styleId = library.result().value("id").toString();
    auto savedRecipe = library.saveRecipe("Bridge", recipe);
    auto attached = library.attachRecipe(styleId, savedRecipe.value("id").toString(), {segment, lower});
    if (!check(attached.value("success").toBool(), "Recipe/style attachment failed"))
        return false;
    auto plan = library.saveBlueprint("Plan", {{"styleId", styleId},
                                               {"mask", mask},
                                               {"seed", 15},
                                               {"replace", true},
                                               {"brush", QVariantMap{{"procedural", true}}}});
    auto frozen = library.load(plan.value("id").toString()).value("blueprint").toMap();
    if (!check(!frozen.value("styleSnapshot").toMap().isEmpty(), "Saved plan did not pin a style snapshot"))
        return false;
    auto revised = library.reviseStyle(styleId, {{"focusGround", 101}});
    if (!check(revised.value("success").toBool() && revised.value("id").toString() != styleId,
               "Style corrections mutated an old revision"))
        return false;
    QString file = tmp.filePath("styles.json");
    int before = library.entries().size();
    if (!check(library.exportPackage(file).value("success").toBool() &&
                   library.importPackage(file).value("success").toBool() &&
                   library.entries().size() == before * 2,
               "Library package round trip failed"))
        return false;
    library.selectProfile("other-client");
    if (!check(!library.importPackage(file).value("success").toBool() && library.entries().isEmpty(),
               "Cross-client package corrupted a profile"))
        return false;
    qInfo() << "Procedural geometry / RAW boundaries / motifs / protection / determinism checks passed";
    return true;
}

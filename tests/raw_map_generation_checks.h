#pragma once
static bool testRawMapGeneration()
{
    using namespace MapGeneration;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    QVariantList sample, mask;
    QVariantMap metadata;
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
        {
            int id = 1000 + y * 5 + x;
            metadata[QString::number(id)] =
                QVariantMap{{"valid", true}, {"ground", true}, {"terrain", "irrelevant"}};
            QVariantList items{QVariantMap{{"id", id}, {"count", 1}, {"ground", true}, {"structural", true}}};
            if (x == 2 && y == 2)
            {
                items.append(QVariantMap{{"id", 300}, {"count", 7}, {"ground", false}, {"actionId", 1234}});
                items.append(QVariantMap{{"id", 200}, {"count", 3}, {"ground", false}, {"structural", true}});
            }
            sample.append(QVariantMap{{"x", x}, {"y", y}, {"z", 0}, {"items", items}});
            mask.append(QVariantMap{{"x", 50 + x}, {"y", 60 + y}, {"z", 7}});
        }
    metadata["200"] = QVariantMap{{"valid", true}, {"blocked", true}};
    metadata["300"] = QVariantMap{{"valid", true}};
    auto model = learnRaw({QVariantMap{{"tiles", sample}}}, {}, cancel);
    GenerationRequest request{
        model,
        {{"mask", mask}, {"seed", 17}, {"replace", true}, {"brush", QVariantMap{{"rawLayout", true}}}},
        {{"width", 256}, {"height", 256}, {"itemInfo", metadata}}};
    auto a = generate(request, cancel);
    if (!check(a.success && a.patch.value("rawLayout").toBool() && !a.patch.value("autoBorders").toBool(),
               "RAW generator fell back to terrain brushes"))
        return false;
    auto tiles = a.patch.value("tiles").toList();
    if (!check(tiles.size() == 25, "RAW patch lost cells"))
        return false;
    for (const auto &v : tiles)
    {
        auto t = v.toMap();
        int x = t.value("x").toInt() - 50, y = t.value("y").toInt() - 60;
        auto items = t.value("items").toList();
        if (!check(items.first().toMap().value("id").toInt() == 1000 + y * 5 + x,
                   "RAW source arrangement was randomized per tile"))
            return false;
        if (x == 2 && y == 2)
        {
            if (!check(items.size() == 3 && items[1].toMap().value("id").toInt() == 300 &&
                           items[1].toMap().value("count").toInt() == 7 &&
                           items[2].toMap().value("id").toInt() == 200 &&
                           !items[1].toMap().contains("actionId"),
                       "RAW stack order, counts or structural pieces were lost"))
                return false;
        }
    }
    auto altered = metadata;
    for (auto it = altered.begin(); it != altered.end(); ++it)
    {
        auto m = it->toMap();
        m["terrain"] = "different brush";
        m["border"] = true;
        m["structural"] = true;
        it.value() = m;
    }
    request.context["itemInfo"] = altered;
    if (!check(generate(request, cancel).patch == a.patch, "RAW output depends on brush classification"))
        return false;
    request.context["itemInfo"] = metadata;
    auto protectedTile = mask.first().toMap();
    protectedTile["occupied"] = true;
    protectedTile["items"] = sample.first().toMap().value("items");
    request.context["tiles"] = QVariantList{protectedTile};
    request.blueprint["replace"] = false;
    if (!check(generate(request, cancel).patch.value("tiles").toList().size() == 24,
               "RAW synthesis overwrote protected content"))
        return false;
    request.blueprint["replace"] = true;
    request.context["tiles"] = QVariantList{};
    request.blueprint["paths"] = QVariantList{mask[12]};
    if (!check(!generate(request, cancel).success, "RAW synthesis cut a path through a copied obstacle"))
        return false;
    request.blueprint.remove("paths");
    cancel->store(true);
    if (!check(!generate(request, cancel).success, "RAW synthesis ignored cancellation"))
        return false;
    cancel->store(false);
    QVariantMap prefab{
        {"name", "Two floors"},
        {"tiles",
         QVariantList{QVariantMap{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QVariantList{1000}}},
                      QVariantMap{{"dx", 1}, {"dy", 0}, {"dz", -1}, {"items", QVariantList{1000, 300}}}}}};
    request.model["prefabs"] = QVariantList{prefab};
    request.blueprint["pins"] =
        QVariantList{QVariantMap{{"name", "Two floors"}, {"x", 50}, {"y", 60}, {"z", 7}}};
    if (!check(generate(request, cancel).success, "RAW styles no longer support complete prefabs"))
        return false;
    request.blueprint["pins"] =
        QVariantList{QVariantMap{{"name", "Two floors"}, {"x", 54}, {"y", 60}, {"z", 7}}};
    if (!check(!generate(request, cancel).success, "RAW synthesis cut a prefab at the footprint edge"))
        return false;
    qInfo() << "RAW layouts / exact stacks / brush independence / prefab checks passed";
    return true;
}

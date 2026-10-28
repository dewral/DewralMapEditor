#include "generationlibrary.h"

#include "otbmreader.h"

#include <QCryptographicHash>

#include <QDir>

#include <QFile>

#include <QJsonDocument>

#include <QSaveFile>

#include <QStandardPaths>

#include <QUrl>

#include <QUuid>

#include <QtConcurrent>

#include <algorithm>

#include <limits>

namespace

{

QVariantMap fail(const QString &s)

{

    return {{"success", false}, {"error", s}};
}

QString profileDirectory(const QString &root, const QString &profile)

{

    return QDir(root).filePath(

        QString::fromLatin1(QCryptographicHash::hash(profile.toUtf8(), QCryptographicHash::Sha256).toHex()));
}

} // namespace

GenerationLibrary::GenerationLibrary(QObject *parent, const QString &directory)

    : QObject(parent),

      m_root(directory.isEmpty()

                 ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/generation-library"

                 : directory)

{

    connect(&m_worker, &QFutureWatcher<QVariantMap>::finished, this, [this] {
        m_result = m_worker.result();

        if (m_cancel && m_cancel->load())

            m_result = fail("Cancelled.");

        if (m_result.value("success").toBool() && m_result.contains("entry"))

        {

            const auto saved = save(m_result.take("entry").toMap());

            if (!saved.value("success").toBool())

                m_result = saved;

            else

                m_result["id"] = saved.value("id");
        }

        m_busy = false;

        m_status =

            m_result.value("success").toBool() ? QStringLiteral("Ready") : m_result.value("error").toString();

        if (m_result.value("success").toBool())

            m_progress = 100;

        emit stateChanged();

        emit resultChanged();
    });
}

GenerationLibrary::~GenerationLibrary()

{

    cancel();

    m_worker.waitForFinished();
}

QVariantList GenerationLibrary::entries() const

{

    QVariantList out;

    for (const auto &v : m_entries)

    {

        auto e = v.toMap();

        e.remove("tiles");

        e.remove("model");

        e.remove("blueprint");

        out.append(e);
    }

    return out;
}

bool GenerationLibrary::selectProfile(const QString &profile)

{

    if (busy() || profile.isEmpty())

        return false;

    if (profile == m_profile)

        return true;

    QVariantList loaded;

    QFile f(QDir(profileDirectory(m_root, profile)).filePath("library.json"));

    if (f.exists())

    {

        if (!f.open(QIODevice::ReadOnly))

        {

            m_status = f.errorString();

            emit stateChanged();

            return false;
        }

        QJsonParseError parse;

        auto doc = QJsonDocument::fromJson(f.readAll(), &parse);

        auto root = doc.toVariant().toMap();

        if (parse.error != QJsonParseError::NoError || root.value("version").toInt() != 1 ||

            root.value("profile").toString() != profile)

        {

            m_status = "Unsupported or incompatible generation library. The file was "

                       "left intact.";

            emit stateChanged();

            return false;
        }

        loaded = root.value("entries").toList();

        QSet<QString> seen;

        for (const auto &v : loaded)

        {

            auto e = v.toMap();

            auto id = e.value("id").toString();

            if (id.isEmpty() || seen.contains(id) || e.value("profile").toString() != profile)

            {

                m_status = "Invalid library entry; the file was left intact.";

                emit stateChanged();

                return false;
            }

            seen.insert(id);
        }
    }

    m_profile = profile;

    m_entries = loaded;

    m_result.clear();

    m_status = "Library ready";

    emit entriesChanged();

    emit stateChanged();

    emit resultChanged();

    return true;
}

QVariantMap GenerationLibrary::load(const QString &id) const

{

    for (const auto &v : m_entries)

        if (v.toMap().value("id") == id)

            return v.toMap();

    return {};
}

bool GenerationLibrary::persist(const QVariantList &data, QString &error) const

{

    if (m_profile.isEmpty())

    {

        error = "Load a client profile first.";

        return false;
    }

    const auto dir = profileDirectory(m_root, m_profile);

    if (!QDir().mkpath(dir))

    {

        error = "Cannot create the generation library directory.";

        return false;
    }

    QSaveFile f(QDir(dir).filePath("library.json"));

    if (!f.open(QIODevice::WriteOnly))

    {

        error = f.errorString();

        return false;
    }

    auto bytes =

        QJsonDocument::fromVariant(QVariantMap{{"version", 1}, {"profile", m_profile}, {"entries", data}})

            .toJson();

    if (f.write(bytes) != bytes.size() || !f.commit())

    {

        error = f.errorString();

        return false;
    }

    return true;
}

QVariantMap GenerationLibrary::save(QVariantMap entry, const QString &id)

{

    if (entry.value("name").toString().trimmed().isEmpty() || entry.value("name").toString().size() > 120)

        return fail("Enter a name of 1–120 characters.");

    QString uid = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;

    if (!id.isEmpty() && load(id).isEmpty())

        return fail("This resource no longer exists.");

    entry["id"] = uid;

    entry["profile"] = m_profile;

    entry["version"] =
        entry.value("model").toMap().value("styleVersion").toInt() == 2 || entry.value("kind") == "recipe"
            ? 2
            : 1;

    if (entry.value("kind") == "style")
        entry["revision"] = QString::fromLatin1(
            QCryptographicHash::hash(
                QJsonDocument::fromVariant(entry.value("model")).toJson(QJsonDocument::Compact),
                QCryptographicHash::Sha256)
                .toHex());

    auto next = m_entries;

    bool replaced = false;

    for (auto &v : next)

        if (v.toMap().value("id") == uid)

        {

            v = entry;

            replaced = true;

            break;
        }

    if (!replaced)

        next.append(entry);

    QString error;

    if (!persist(next, error))

        return fail(error);

    m_entries = next;

    emit entriesChanged();

    return {{"success", true}, {"id", uid}};
}

QVariantMap GenerationLibrary::capture(const QString &name, const QVariantMap &snapshot)

{

    if (busy())

        return fail("Wait for the current operation.");

    if (!snapshot.value("success").toBool())

        return fail(snapshot.value("error", "Select a map fragment first.").toString());

    auto tiles = snapshot.value("tiles").toList();

    if (tiles.isEmpty() || tiles.size() > 100000)

        return fail("Capture between 1 and 100000 selected cells.");

    return save(

        {{"kind", "example"}, {"name", name.trimmed()}, {"tiles", tiles}, {"tileCount", tiles.size()}});
}

QVariantMap GenerationLibrary::saveBlueprint(const QString &name, const QVariantMap &blueprint,

                                             const QString &id)

{

    if (busy())

        return fail("Wait for the current operation.");

    if (load(blueprint.value("styleId").toString()).value("kind") != "style")

        return fail("Choose a style from this client profile.");

    if (blueprint.value("mask").toList().isEmpty() || blueprint.value("mask").toList().size() > 100000)

        return fail("Paint a mask first (up to 100000 cells).");

    if (!id.isEmpty() && load(id).value("kind") != "blueprint")

        return fail("This resource is not a blueprint.");

    auto plan = blueprint;

    auto style = load(plan.value("styleId").toString());

    if (plan.value("styleSnapshot").toMap().isEmpty())
        plan["styleSnapshot"] = style;

    plan["styleRevision"] = plan.value("styleSnapshot").toMap().value("revision");

    plan["version"] = 2;

    return save({{"kind", "blueprint"}, {"name", name.trimmed()}, {"blueprint", plan}}, id);
}

QVariantMap GenerationLibrary::remove(const QString &id)

{

    if (busy())

        return fail("Wait for the current operation.");

    for (const auto &v : m_entries)

    {

        auto e = v.toMap();

        if (e.value("kind") == "style" && e.value("examples").toList().contains(id))

            return fail("An existing style references this example.");

        if (e.value("kind") == "blueprint" && e.value("blueprint").toMap().value("styleId") == id)

            return fail("An existing blueprint references this style.");
    }

    auto next = m_entries;

    for (int i = 0; i < next.size(); ++i)

        if (next[i].toMap().value("id") == id)

        {

            next.removeAt(i);

            QString error;

            if (!persist(next, error))

                return fail(error);

            m_entries = next;

            emit entriesChanged();

            return {{"success", true}};
        }

    return fail("Resource not found.");
}

bool GenerationLibrary::start(const QString &operation,

                              std::function<QVariantMap(const MapGeneration::Progress &)> work)

{

    if (busy() || m_profile.isEmpty())

        return false;

    m_busy = true;

    m_cancel = std::make_shared<std::atomic_bool>(false);

    m_operation = operation;

    m_status = operation;

    m_progress = 0;

    m_result.clear();

    auto update = [this](int value) {
        QMetaObject::invokeMethod(

            this,

            [this, value] {
                m_progress = value;

                emit stateChanged();
            },

            Qt::QueuedConnection);
    };

    m_worker.setFuture(QtConcurrent::run([work, update] { return work(update); }));

    emit stateChanged();

    emit resultChanged();

    return true;
}

void GenerationLibrary::cancel()

{

    if (m_cancel)

        m_cancel->store(true);
}

bool GenerationLibrary::learnStyle(const QString &name, const QVariantList &exampleIds,

                                   const QVariantList &prefabs, const QVariantMap &parameters)

{

    if (busy())

        return false;

    QVariantList examples;

    for (const auto &id : exampleIds)

    {

        auto e = load(id.toString());

        if (e.value("kind") != "example")

        {

            m_status = "Choose saved examples from this profile.";

            emit stateChanged();

            return false;
        }

        examples.append(e);
    }

    // Cancellation handle is captured after start creates it; the worker owns its

    // copy.

    return start("Learning local neighborhoods",

                 [this, name, examples, exampleIds, prefabs, parameters](const auto &progress) {
                     auto model = parameters.value("procedural").toBool()

                                      ? MapGeneration::learnProcedural(examples, prefabs, parameters,
                                                                       m_cancel, progress)

                                      : parameters.value("rawLayout").toBool()

                                            ? MapGeneration::learnRaw(examples, prefabs, m_cancel, progress)

                                            : MapGeneration::learn(examples, prefabs, m_cancel, progress);

                     if (!model.value("success").toBool())

                         return model;

                     return QVariantMap{{"success", true},

                                        {"operation", "learn"},

                                        {"entry", QVariantMap{{"kind", "style"},

                                                              {"name", name.trimmed()},

                                                              {"examples", exampleIds},

                                                              {"parameters", parameters},

                                                              {"model", model}}}};
                 });
}

bool GenerationLibrary::generate(const QString &styleId, const QVariantMap &blueprint,

                                 const QVariantMap &context)

{

    if (busy())

        return false;

    auto style = load(styleId);

    if (style.value("kind") != "style")

    {

        m_status = "Choose a learned style from this client profile.";

        emit stateChanged();

        return false;
    }

    auto options = style.value("parameters").toMap();

    for (auto it = blueprint.cbegin(); it != blueprint.cend(); ++it)

        options[it.key()] = it.value();

    auto model = style.value("model").toMap();

    const auto frozen = options.value("styleSnapshot").toMap();

    if (!frozen.isEmpty())
    {

        if (frozen.value("profile") != m_profile || frozen.value("id") != styleId)
        {
            m_status = "Saved plan belongs to another style or profile.";
            emit stateChanged();
            return false;
        }

        model = frozen.value("model").toMap();
    }

    // Old saved styles still retain their examples. The center of the first

    // source identifies the terrain being painted; its surroundings teach

    // decoration only.

    if (options.value("brush").toMap().value("terrainPaint").toBool())

    {

        const auto examples = style.value("examples").toList();

        if (!examples.isEmpty())

        {

            const auto tiles = load(examples.first().toString()).value("tiles").toList();

            int minX = 65535, minY = 65535, maxX = 0, maxY = 0;

            for (const auto &v : tiles)

            {

                auto t = v.toMap();

                minX = std::min(minX, t.value("x").toInt());

                minY = std::min(minY, t.value("y").toInt());

                maxX = std::max(maxX, t.value("x").toInt());

                maxY = std::max(maxY, t.value("y").toInt());
            }

            qint64 nearest = std::numeric_limits<qint64>::max();

            int focus = 0;

            for (const auto &v : tiles)

            {

                auto t = v.toMap();

                int g = 0;

                for (const auto &iv : t.value("items").toList())

                    if (iv.toMap().value("ground").toBool())

                    {

                        g = iv.toMap().value("id").toInt();

                        break;
                    }

                if (!g)

                    continue;

                qint64 dx = qint64(t.value("x").toInt()) * 2 - minX - maxX,

                       dy = qint64(t.value("y").toInt()) * 2 - minY - maxY;

                const qint64 distance = dx * dx + dy * dy;

                if (distance < nearest)

                {

                    nearest = distance;

                    focus = g;
                }
            }

            if (focus)

                model["focusGround"] = focus;
        }
    }

    QVariantList refreshedExamples;

    if (options.value("brush").toMap().value("terrainPaint").toBool())

    {

        const auto metadata = context.value("itemInfo").toMap();

        for (const auto &id : style.value("examples").toList())

        {

            auto e = load(id.toString());

            QVariantList tiles;

            for (const auto &tv : e.value("tiles").toList())

            {

                auto t = tv.toMap();

                QVariantList items;

                for (const auto &iv : t.value("items").toList())

                {

                    auto i = iv.toMap();

                    auto m = metadata.value(QString::number(i.value("id").toInt())).toMap();

                    if (!m.isEmpty())

                    {

                        i["structural"] = !i.value("ground").toBool() && m.value("structural").toBool();

                        i["border"] = m.value("border");
                    }

                    items.append(i);
                }

                t["items"] = items;

                tiles.append(t);
            }

            e["tiles"] = tiles;

            refreshedExamples.append(e);
        }
    }

    QVariantList rawExamples;

    if (frozen.isEmpty() && (options.value("brush").toMap().value("rawLayout").toBool() ||
                             options.value("brush").toMap().value("procedural").toBool()))

        for (const auto &id : style.value("examples").toList())

            rawExamples.append(load(id.toString()));

    MapGeneration::GenerationRequest request{model, options, context};

    return start("Generating preview locally", [this, request, refreshedExamples,

                                                rawExamples](const auto &progress) mutable {
        if (request.blueprint.value("brush").toMap().value("procedural").toBool())
        {

            if (request.model.value("algorithm") != "procedural-v2")
            {

                auto model = MapGeneration::learnProcedural(
                    rawExamples, request.model.value("prefabs").toList(), {}, m_cancel);

                if (!model.value("success").toBool())
                    return QVariantMap{
                        {"success", false}, {"operation", "generate"}, {"error", model.value("error")}};

                request.model = model;
            }
        }

        else if (request.blueprint.value("brush").toMap().value("rawLayout").toBool())

        {

            if (!rawExamples.isEmpty() || request.model.value("algorithm") != "raw-patch-v1")

            {

                auto model =

                    MapGeneration::learnRaw(rawExamples, request.model.value("prefabs").toList(), m_cancel);

                if (!model.value("success").toBool())

                    return QVariantMap{

                        {"success", false}, {"operation", "generate"}, {"error", model.value("error")}};

                request.model = model;
            }
        }

        else if (!refreshedExamples.isEmpty())

        {

            auto model = MapGeneration::learn(refreshedExamples, request.model.value("prefabs").toList(),

                                              m_cancel, progress);

            if (!model.value("success").toBool())

                return QVariantMap{

                    {"success", false}, {"operation", "generate"}, {"error", model.value("error")}};

            model["focusGround"] = request.model.value("focusGround");

            request.model = model;
        }

        auto r = MapGeneration::generate(request, m_cancel, progress);

        return QVariantMap{{"success", r.success},

                           {"operation", "generate"},

                           {"patch", r.patch},

                           {"error", r.error},

                           {"diagnostics", r.diagnostics}};
    });
}

bool GenerationLibrary::importExample(const QString &name, const QString &path, const QVariantMap &region,

                                      const QVariantMap &materials)

{

    return start("Importing selected OTBM fragment", [this, name, path, region,

                                                      materials](const auto &progress) {
        const int x = region.value("x", -1).toInt(), y = region.value("y", -1).toInt(),

                  z = region.value("z", -1).toInt(), w = region.value("width").toInt(),

                  h = region.value("height").toInt(), floors = region.value("floors", 1).toInt();

        if (x < 0 || y < 0 || z < 0 || w < 1 || h < 1 || floors < 1 || z + floors > 16 ||

            qint64(w) * h * floors > 100000 || x + w > 65536 || y + h > 65536)

            return fail("Invalid fragment bounds (maximum 100000 cells, floors 0–15).");

        OtbmReader map;

        const auto info = materials.value("itemInfo").toMap();

        OtbmItemTypes types;

        for (auto it = info.cbegin(); it != info.cend(); ++it)

        {

            bool ok = false;

            const int id = it.key().toInt(&ok);

            if (!ok || id < 1 || id > 65535)

                continue;

            const auto metadata = it.value().toMap();

            types.known.set(id);

            types.subtype.set(id, metadata.value("subtype").toBool());

            types.fluidContainers.set(id, metadata.value("fluid").toBool());
        }

        map.setItemTypes(types);

        QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;

        if (!map.loadFileDetached(

                local, [&](int n, const QString &) { progress(n / 2); }, [this] { return m_cancel->load(); }))

            return fail(m_cancel->load() ? "Cancelled." : map.errorString());

        if (map.otbItemsMajorVersion() != materials.value("otbMajor").toInt() ||

            map.otbItemsMinorVersion() != materials.value("otbMinor").toInt())

            return fail("The source map uses a different item profile.");

        QVariantList tiles;

        for (int zz = 0; zz < floors; ++zz)

            for (int yy = 0; yy < h; ++yy)

                for (int xx = 0; xx < w; ++xx)

                {

                    if (m_cancel->load())

                        return fail("Cancelled.");

                    const auto *t = map.tileAt(x + xx, y + yy, z + zz);

                    QVariantList items;

                    if (t)

                        for (const auto &i : t->items)

                        {

                            auto m = info.value(QString::number(i.server_id)).toMap();

                            if (!m.value("valid").toBool())

                                return fail(QString("Item %1 is missing from this client.").arg(i.server_id));

                            items.append(
                                QVariantMap{{"id", i.server_id},

                                            {"count", i.count},

                                            {"ground", m.value("ground")},

                                            {"structural", m.value("structural")},

                                            {"border", m.value("border")},
                                            {"blocked", m.value("blocked")},
                                            {"surface", m.value("surface", QString::number(i.server_id))}});
                        }

                    QVariantMap cell{{"x", xx}, {"y", yy}, {"z", zz}, {"items", items}};

                    if (t && !t->creature_is_npc && !t->creature_name.isEmpty())

                    {

                        cell["monster"] = QString(t->creature_name);

                        cell["radius"] = std::max(1, t->spawn_radius);

                        cell["spawntime"] = t->creature_spawntime;
                    }

                    tiles.append(cell);
                }

        progress(100);

        return QVariantMap{{"success", true},

                           {"operation", "import"},

                           {"entry", QVariantMap{{"kind", "example"},

                                                 {"name", name.trimmed()},

                                                 {"tiles", tiles},

                                                 {"tileCount", tiles.size()}}}};
    });
}

QVariantMap GenerationLibrary::saveRecipe(const QString &name, const QVariantMap &recipe)

{

    if (busy())
        return fail("Wait for the current operation.");

    const auto parts = recipe.value("parts").toList();

    if (parts.isEmpty() || parts.size() > 64)
        return fail("A structure needs 1–64 complete prefab parts.");

    for (const auto &v : parts)
    {
        const auto p = v.toMap();
        if (p.value("prefab").toString().isEmpty() || abs(p.value("dx").toInt()) > 1024 ||
            abs(p.value("dy").toInt()) > 1024 || abs(p.value("dz").toInt()) > 15)
            return fail("Invalid structure part.");
    }

    auto clean = recipe;
    clean["name"] = name.trimmed();
    clean["version"] = 1;

    return save({{"kind", "recipe"}, {"name", name.trimmed()}, {"recipe", clean}});
}

QVariantMap GenerationLibrary::reviseStyle(const QString &styleId, const QVariantMap &roles)

{

    if (busy())
        return fail("Wait for the current operation.");

    auto style = load(styleId);
    if (style.value("kind") != "style")
        return fail("Choose a saved style.");

    auto model = style.value("model").toMap();
    if (model.value("algorithm") != "procedural-v2")
        return fail("Save a new procedural style from the retained examples first.");

    model["roles"] = roles;
    style["model"] = model;
    style["name"] = style.value("name").toString() + " · poprawiony";

    return save(style); // New ID/revision: existing plans retain their original snapshot.
}

QVariantMap GenerationLibrary::exportPackage(const QString &path) const

{

    if (busy() || m_profile.isEmpty())
        return fail("Choose an idle client library.");

    QString local = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;

    QSaveFile file(local);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());

    auto bytes = QJsonDocument::fromVariant(QVariantMap{{"format", "dme-procedural-library"},
                                                        {"version", 2},
                                                        {"profile", m_profile},
                                                        {"entries", m_entries}})
                     .toJson();

    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(file.errorString());
    return {{"success", true}};
}

QVariantMap GenerationLibrary::importPackage(const QString &path)

{

    if (busy() || m_profile.isEmpty())
        return fail("Choose an idle client library.");

    QFile file(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());

    if (file.size() > 64 * 1024 * 1024)
        return fail("The package exceeds 64 MiB.");

    QJsonParseError parse;
    auto root = QJsonDocument::fromJson(file.readAll(), &parse).toVariant().toMap();

    if (parse.error != QJsonParseError::NoError || root.value("format") != "dme-procedural-library" ||
        root.value("version").toInt() != 2 || root.value("profile") != m_profile)
        return fail("The package is invalid or belongs to another client profile.");

    auto incoming = root.value("entries").toList();
    if (incoming.isEmpty() || incoming.size() > 10000)
        return fail("Invalid package entries.");

    QMap<QString, QString> ids;

    for (const auto &v : incoming)
    {
        auto e = v.toMap();
        auto id = e.value("id").toString();
        auto kind = e.value("kind").toString();
        if (id.isEmpty() || ids.contains(id) || e.value("profile") != m_profile ||
            !QStringList{"example", "style", "blueprint", "recipe"}.contains(kind) ||
            e.value("name").toString().trimmed().isEmpty())
            return fail("Invalid package resource.");
        ids[id] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    QVariantList next = m_entries;

    for (const auto &v : incoming)
    {
        auto e = v.toMap();
        e["id"] = ids.value(e.value("id").toString());

        if (e.value("kind") == "style")
        {
            QVariantList refs;
            for (const auto &ref : e.value("examples").toList())
            {
                if (!ids.contains(ref.toString()))
                    return fail("Missing example in package.");
                refs.append(ids.value(ref.toString()));
            }
            e["examples"] = refs;
        }

        if (e.value("kind") == "blueprint")
        {
            auto plan = e.value("blueprint").toMap();
            if (!ids.contains(plan.value("styleId").toString()))
                return fail("Missing style in package.");
            plan["styleId"] = ids.value(plan.value("styleId").toString());
            auto frozen = plan.value("styleSnapshot").toMap();
            if (!frozen.isEmpty())
            {
                frozen["id"] = plan.value("styleId");
                plan["styleSnapshot"] = frozen;
            }
            e["blueprint"] = plan;
        }

        next.append(e);
    }

    QString error;
    if (!persist(next, error))
        return fail(error);
    m_entries = next;
    emit entriesChanged();
    return {{"success", true}, {"imported", incoming.size()}};
}

QVariantMap GenerationLibrary::attachRecipe(const QString &styleId, const QString &recipeId,
                                            const QVariantList &prefabs)

{

    if (busy())
        return fail("Wait for the current operation.");

    auto style = load(styleId), resource = load(recipeId);
    if (style.value("kind") != "style" || resource.value("kind") != "recipe")
        return fail("Choose a style and a structure.");

    auto model = style.value("model").toMap();
    if (model.value("algorithm") != "procedural-v2")
        return fail("Save a procedural style from the examples first.");

    auto recipe = resource.value("recipe").toMap();
    auto allowed = model.value("prefabs").toList();

    for (const auto &v : recipe.value("parts").toList())
    {
        auto name = v.toMap().value("prefab");
        bool found = false;
        for (const auto &p : allowed)
            found |= p.toMap().value("name") == name;

        if (!found)
            for (const auto &p : prefabs)
                if (p.toMap().value("name") == name)
                {
                    allowed.append(p);
                    found = true;
                    break;
                }
        if (!found)
            return fail("A required prefab is unavailable.");
    }

    auto recipes = model.value("recipes").toList();
    for (const auto &r : recipes)
        if (r.toMap().value("name") == recipe.value("name"))
            return fail("A structure with this name is already in the style.");
    recipes.append(recipe);
    model["recipes"] = recipes;
    model["prefabs"] = allowed;
    style["model"] = model;
    style["name"] = style.value("name").toString() + " · " + recipe.value("name").toString();
    return save(style);
}

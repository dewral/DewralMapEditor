#include "tilemapgenbridge.h"
#include "backend.h"
#include "mapview.h"
#include "tilemapgenmaterials.h"
#include "tilemapgenassets.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <QtConcurrent>
#include <algorithm>

namespace {
QString fingerprint(const QJsonObject &catalog) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(catalog).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
}

TileMapGenBridge::TileMapGenBridge(Backend *backend) : QObject(backend), m_backend(backend) {
    connect(&m_helper, &TileMapGenProcess::finished, this, &TileMapGenBridge::helperFinished);
    connect(&m_watcher, &QFutureWatcher<TmgFormat::Result>::finished, this, [this] {
        auto result = m_watcher.result(); m_busy = false;
        if (m_cancel->load()) result.error = tr("Import cancelled");
        if (result.error.isEmpty() && fingerprint(catalogJson()) != m_importFingerprint)
            result.error = tr("The active client or materials changed during import. Import again.");
        if (result.error.isEmpty() && m_placement) {
            if (!m_view || !m_target || m_target != m_backend->otbmReader()) result.error = tr("The destination document changed during import.");
            else {
                const auto request = result.manifest.value("request").toObject();
                result.error = m_view->previewGeneratedMap(result.document.data(), m_x, m_y, m_z,
                    request["start_z"].toInt(), request["width"].toInt(), request["height"].toInt(), request["floors"].toInt());
                if (result.error.isEmpty()) {
                    m_preview = result;
                    m_view->centerOnPosition(m_x + request["width"].toInt() / 2,
                                            m_y + request["height"].toInt() / 2,
                                            m_z + result.manifest.value("surface_floor").toInt());
                }
            }
        } else if (result.error.isEmpty()) {
            const auto profile = m_backend->docMgr()->currentProfileKey();
            auto *target = m_backend->docMgr()->newDocument();
            target->adoptLoadedState(*result.document); target->finishLoading(true);
            m_backend->docMgr()->setCurrentProfileKey(profile);
        }
        m_status = result.error.isEmpty() ? (m_placement ? tr("Preview ready. Apply as one undoable edit or cancel.") : tr("Opened as a new unsaved map. Save in Dewral to write OTBM and spawns.")) : result.error;
        emit changed(); emit finished(result.error.isEmpty());
    });
    connect(&m_generator, &TileMapGenProcess::progressChanged, this, [this](int value, const QString &stage) {
        m_progress = value; m_status = stage; emit changed();
    });
    connect(&m_generator, &TileMapGenProcess::finished, this, [this](bool success, const QString &error) {
        m_generating = false;
        if (!success) { failGeneration(error); return; }
        if (fingerprint(catalogJson()) != m_importFingerprint) {
            failGeneration(tr("The active client or materials changed during generation. Generate again.")); return;
        }
        if (m_placement && (!m_target || m_target != m_backend->otbmReader())) {
            failGeneration(tr("The destination document changed during generation.")); return;
        }
        m_busy = false;
        importMap(QUrl::fromLocalFile(m_workDirectory->filePath(QStringLiteral("output/map.tmg"))),
                  m_placement, m_view, m_x, m_y, m_z);
    });
    connect(backend->docMgr(), &DocumentManager::currentChanged, this, [this] {
        if (busy()) cancel();
        else clearPreview();
    });
}
TileMapGenBridge::~TileMapGenBridge() {
    disconnect(&m_helper, nullptr, this, nullptr);
    m_helper.shutdown();
    disconnect(&m_generator, nullptr, this, nullptr);
    m_generator.shutdown();
    if (m_cancel) m_cancel->store(true);
    m_watcher.waitForFinished();
}

QJsonObject TileMapGenBridge::catalogJson() const {
    auto *otb = m_backend->otbReader(); auto *dat = m_backend->datReader();
    QJsonObject result = m_backend->brushStore()->generationCatalog();
    const auto sourceHash = [this](const QString &path) {
        const QFileInfo info(path);
        const QString key = path + QStringLiteral(":%1:%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
        if (m_fileHashes.contains(key)) return m_fileHashes.value(key);
        QFile file(path); if (!file.open(QIODevice::ReadOnly)) return QString();
        QCryptographicHash hash(QCryptographicHash::Sha256); if (!hash.addData(&file)) return QString();
        const auto value = QString::fromLatin1(hash.result().toHex()); m_fileHashes.insert(key, value); return value;
    };
    QJsonArray items, creatures;
    for (int row = 0; row < otb->rowCount(); ++row) {
        const int sid = otb->data(otb->index(row), OtbReader::ServerIdRole).toInt();
        const int cid = otb->clientIdForServerId(sid); const auto *item = dat->itemByClientId(cid);
        if (sid <= 0 || !item) continue;
        items.append(QJsonObject{{"id", sid}, {"name", otb->nameForServerId(sid)},
            {"ground", otb->isClientGroundForServerId(sid)},
            {"blocked", otb->isUnpassableForServerId(sid) || otb->isClientUnpassableForServerId(sid)},
            {"block_path", otb->blocksPathForServerId(sid)}, {"floor_change", qint64(otb->floorChangeFlagsForServerId(sid))},
            {"top_order", otb->topOrderForServerId(sid)}, {"sprite_width", item->width * 32}, {"sprite_height", item->height * 32},
            {"offset_x", item->offset_x}, {"offset_y", item->offset_y}});
    }
    for (const auto &c : m_backend->creatureStore()->creatureTypes())
        creatures.append(QJsonObject{{"name", c.name}, {"is_npc", c.isNpc}, {"look_type", c.lookType}, {"look_item", c.lookItem}});
    result["items"] = items; result["creatures"] = TileMapGenMaterials::uniqueCreatures(creatures);
    result["client"] = QJsonObject{{"key", m_backend->docMgr()->currentProfileKey()}, {"version", dat->clientVersion()}, {"otb_major", int(otb->majorVersion())}, {"otb_minor", int(otb->minorVersion())},
        {"dat_sha256", sourceHash(dat->sourceFilePath())}, {"spr_sha256", sourceHash(m_backend->sprReader()->sourceFilePath())}};
    return result;
}
TmgFormat::Catalog TileMapGenBridge::catalog() const {
    const auto data = catalogJson(); TmgFormat::Catalog out; out.fingerprint = fingerprint(data);
    const auto client = data["client"].toObject(); out.version = client["version"].toInt(); out.major = client["otb_major"].toInt(); out.minor = client["otb_minor"].toInt();
    for (const auto &value : data["items"].toArray()) { const auto i = value.toObject(); out.items.insert(i["id"].toInt(), {i["ground"].toBool(), i["blocked"].toBool(), quint32(i["floor_change"].toInteger())}); }
    for (const auto &value : data["creatures"].toArray()) { const auto c = value.toObject(); if (!c["is_npc"].toBool()) out.monsters.insert(c["name"].toString()); }
    return out;
}

QString TileMapGenBridge::exportAssets(const QUrl &path, int downId, int upId) {
    if (m_busy) return tr("Wait for the current import to finish.");
    return writeAssets(path, downId, upId, true);
}

QString TileMapGenBridge::writeAssets(const QUrl &path, int downId, int upId, bool sprites,
                                    const QVariantMap &palette, const QVariantMap &settlementMaterials) {
    if (!path.isLocalFile() || !m_backend->otbReader()->isLoaded()) return tr("Load client data and choose a local .tmgpack file.");
    QJsonObject pack = catalogJson();
    const auto sourceFingerprint = fingerprint(pack);
    if (!sprites) {
        // Only selected brushes are used by the CLI. Unrelated, stale material
        // definitions must not prevent generation with a valid palette.
        const auto keep = [](const QJsonArray &definitions, const QSet<QString> &names) {
            QJsonArray result;
            for (const auto &definition : definitions)
                if (names.contains(definition.toObject()["name"].toString())) result.append(definition);
            return result;
        };
        const auto grounds = TileMapGenMaterials::selectedGrounds(palette, settlementMaterials);
        QSet<QString> decorations;
        for (const auto &name : palette.value("decorations").toList()) decorations.insert(name.toString());
        pack["grounds"] = keep(pack["grounds"].toArray(), grounds);
        pack["walls"] = keep(pack["walls"].toArray(), {palette.value("wall").toString()});
        pack["prefabs"] = keep(pack["prefabs"].toArray(), decorations);
    }
    QJsonArray connectors;
    auto *otb = m_backend->otbReader();
    if (downId || upId) {
        const quint32 down = otb->floorChangeFlagsForServerId(downId), up = otb->floorChangeFlagsForServerId(upId);
        if (!otb->clientIdForServerId(downId) || !otb->clientIdForServerId(upId) || !(down & 256) || (up & 256) || !(up & 0x7e00))
            return tr("Choose a downward stair and a directional upward stair from this client.");
        const int dx = !!(up & (1 << 10)) - !!(up & (1 << 12)) + 2 * !!(up & (1 << 14));
        const int dy = !!(up & (1 << 11)) - !!(up & (1 << 9)) + 2 * !!(up & (1 << 13));
        connectors.append(QJsonObject{{"id", "stairs"}, {"name", QStringLiteral("Stairs %1 / %2").arg(downId).arg(upId)},
            {"activation", "step"}, {"server_action_confirmed", false},
            {"upper_entry", QJsonArray{0, 0, 0}}, {"lower_entry", QJsonArray{0, 0, 1}},
            {"upper_landing", QJsonArray{dx, dy, 0}}, {"lower_landing", QJsonArray{-dx, -dy, 1}},
            {"tiles", QJsonArray{QJsonObject{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QJsonArray{downId}}}, QJsonObject{{"dx", 0}, {"dy", 0}, {"dz", 1}, {"items", QJsonArray{upId}}}}}});
    }
    pack["schema_version"] = 1; pack["id"] = sourceFingerprint.left(16); pack["name"] = QStringLiteral("Dewral %1 materials").arg(m_backend->datReader()->clientVersion());
    pack["fingerprint"] = sourceFingerprint; pack["synthetic"] = false; pack["connectors"] = connectors;
    // Stale brushes are reported, rather than silently rewriting material definitions.
    const auto valid = catalog();
    QString floor;
    for (const auto &v : pack["grounds"].toArray()) {
        const auto brush = v.toObject(); bool usable = true;
        for (const auto &entry : brush["items"].toArray()) { const int id = entry.toObject()["id"].toInt(); const auto item = valid.items.value(id); usable &= valid.items.contains(id) && item.ground && !item.blocked && !item.floorChange; }
        if (usable) { floor = brush["name"].toString(); break; }
    }
    pack["palette"] = QJsonObject{{"floor", floor}, {"background", ""}, {"secondary", ""}, {"water", ""}, {"lava", ""}, {"wall", ""}, {"decorations", QJsonArray{}}, {"connector", connectors.isEmpty() ? "" : "stairs"}, {"creatures", QJsonArray{}}};
    const QString name = QUuid::createUuid().toString(), temporary = path.toLocalFile() + "." + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".partial";
    QString error;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name); db.setDatabaseName(temporary);
        if (!db.open()) error = tr("Cannot create the asset pack.");
        else {
            QSqlQuery q(db);
            for (const auto &sql : {"PRAGMA application_id=1414350641", "PRAGMA user_version=1", "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL)", "CREATE TABLE assets(id INTEGER PRIMARY KEY,png BLOB NOT NULL)", "CREATE TABLE sprite_variants(id INTEGER,pattern INTEGER,png BLOB NOT NULL,PRIMARY KEY(id,pattern))"}) if (!q.exec(QLatin1String(sql))) error = tr("Cannot initialize the asset pack.");
            if (error.isEmpty() && db.transaction()) {
                q.prepare("INSERT INTO metadata VALUES(?,?)"); q.addBindValue("kind"); q.addBindValue("asset-pack"); if (!q.exec()) error = tr("Cannot write asset metadata.");
                q.prepare("INSERT INTO metadata VALUES(?,?)"); q.addBindValue("pack"); q.addBindValue(QString::fromUtf8(QJsonDocument(pack).toJson(QJsonDocument::Compact))); if (!q.exec()) error = tr("Cannot write asset metadata.");
                QJsonObject renderItems;
                for (const auto &value : pack["items"].toArray()) {
                    if (!sprites || !error.isEmpty()) break;
                    const int sid = value.toObject()["id"].toInt();
                    const auto *item = m_backend->datReader()->itemByClientId(otb->clientIdForServerId(sid));
                    if (!item) continue;
                    renderItems[QString::number(sid)] = TileMapGenAssets::geometry(*item, otb->rotateToForServerId(sid));
                    error = TileMapGenAssets::writeItem(db, sid, *item, [this, item](int pattern) {
                        return m_backend->sprReader()->itemPatternImage(*item, pattern);
                    });
                    if (!error.isEmpty()) break;
                }
                if (sprites && error.isEmpty()) {
                    const QJsonObject render{{"schema_version", 1}, {"catalog_fingerprint", sourceFingerprint}, {"items", renderItems}};
                    q.prepare("INSERT INTO metadata VALUES(?,?)"); q.addBindValue("render");
                    q.addBindValue(QString::fromUtf8(QJsonDocument(render).toJson(QJsonDocument::Compact)));
                    if (!q.exec()) error = tr("Cannot write sprite geometry.");
                }
                if (error.isEmpty()) { if (!db.commit()) error = tr("Cannot commit the asset pack."); } else db.rollback();
            } else if (error.isEmpty()) error = tr("Cannot start asset export transaction.");
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    if (error.isEmpty()) {
        QFile input(temporary); QSaveFile output(path.toLocalFile());
        if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) error = tr("Cannot publish the asset pack.");
        else { while (!input.atEnd()) { const auto bytes = input.read(1024 * 1024); if (output.write(bytes) != bytes.size()) { error = tr("Cannot finish writing the asset pack."); break; } }
            if (error.isEmpty() && !output.commit()) error = tr("Cannot publish the asset pack."); }
    }
    QFile::remove(temporary); return error;
}

void TileMapGenBridge::importMap(const QUrl &path, bool placement, QObject *mapView, int x, int y, int z) {
    if (m_busy) return;
    clearPreview();
    if (!path.isLocalFile() || !m_backend->otbReader()->isLoaded()) { m_status = tr("Load client data and select a local .tmg file."); emit changed(); return; }
    m_view = qobject_cast<MapView *>(mapView); m_target = m_backend->otbmReader();
    m_placement = placement; m_x = x; m_y = y; m_z = z;
    const auto data = catalog(); m_importFingerprint = data.fingerprint;
    m_cancel = std::make_shared<std::atomic_bool>(false); const auto cancelled = m_cancel;
    m_busy = true; m_progress = 0; m_status = tr("Validating the generated map..."); emit changed();
    auto *ownerThread = thread();
    m_watcher.setFuture(QtConcurrent::run([this, data, path, cancelled, placement, ownerThread] {
        auto result = TmgFormat::load(path.toLocalFile(), data, [this](int progress, const QString &stage) {
            QMetaObject::invokeMethod(this, [this, progress, stage] { m_progress = progress; m_status = stage; emit changed(); }, Qt::QueuedConnection);
        }, [cancelled] { return cancelled->load(); }, placement);
        if (result.document) result.document->moveToThread(ownerThread);
        return result;
    }));
}
void TileMapGenBridge::cancel() {
    cancelAiPlan();
    if (m_generating) m_generator.cancel();
    if (m_cancel) m_cancel->store(true);
    clearPreview();
}
void TileMapGenBridge::clearPreview() {
    if (m_preview.document) {
        if (m_view) m_view->clearLuaPreview();
        m_status = tr("Preview discarded. Generate or import again to apply a map.");
    }
    m_preview = {}; emit changed();
}
QString TileMapGenBridge::applyPreview() {
    if (!m_preview.document || !m_view || m_target != m_backend->otbmReader()) return tr("No preview for the current document.");
    if (fingerprint(catalogJson()) != m_importFingerprint) return tr("The active assets changed. Import again.");
    const auto r = m_preview.manifest["request"].toObject();
    const auto error = m_view->applyGeneratedMap(m_preview.document.data(), m_x, m_y, m_z, r["start_z"].toInt(), r["width"].toInt(), r["height"].toInt(), r["floors"].toInt());
    if (error.isEmpty()) { clearPreview(); m_status = tr("Placed as one undoable edit."); emit changed(); }
    return error;
}

QVariantMap TileMapGenBridge::generationMaterials() const {
    const auto pack = catalogJson();
    QHash<int, QJsonObject> items;
    QSet<int> visibleIds;
    for (const auto &v : pack["items"].toArray()) items.insert(v.toObject()["id"].toInt(), v.toObject());
    for (auto it = items.cbegin(); it != items.cend(); ++it) {
        const auto *item = m_backend->datReader()->itemByClientId(m_backend->otbReader()->clientIdForServerId(it.key()));
        if (!item) continue;
        const auto count = std::min(item->sprite_ids.size(), size_t(item->width) * item->height * item->layers);
        if (std::any_of(item->sprite_ids.begin(), item->sprite_ids.begin() + count, [](auto id) { return id != 0; }))
            visibleIds.insert(it.key());
    }
    QStringList floors, backgrounds, walls, decorations, monsters;
    QHash<QString, int> previewIds;
    for (const auto &name : m_backend->brushStore()->groundBrushNames())
        previewIds.insert(name, m_backend->brushStore()->groundBrushLookId(name));
    auto grounds = TileMapGenMaterials::groundChoices(pack, previewIds);
    for (auto &v : grounds) {
        auto ground = v.toMap();
        const auto name = ground.value("name").toString();
        if (ground.value("walking").toBool()) floors.append(name);
        if (ground.value("blocking").toBool()) backgrounds.append(name);
        ground["clientId"] = m_backend->otbReader()->clientIdForServerId(ground.value("serverId").toInt());
        v = ground;
    }
    previewIds.clear();
    for (const auto &name : m_backend->brushStore()->wallBrushNames())
        previewIds.insert(name, m_backend->brushStore()->advancedBrushEdit("walls", name).value("lookid").toInt());
    auto wallChoices = TileMapGenMaterials::wallChoices(pack, previewIds, &visibleIds);
    for (auto &v : wallChoices) {
        auto wall = v.toMap();
        walls.append(wall.value("name").toString());
        wall["clientId"] = m_backend->otbReader()->clientIdForServerId(wall.value("serverId").toInt());
        v = wall;
    }
    for (const auto &v : pack["prefabs"].toArray()) {
        const auto prefab = v.toObject(); bool usable = true;
        for (const auto &variant : prefab["variants"].toArray()) for (const auto &tile : variant.toArray()) {
            const auto record = tile.toObject(); usable &= record["dz"].toInt() == 0;
            for (const auto &id : record["items"].toArray()) {
                const auto item = items.value(id.toInt());
                usable &= !item.isEmpty() && !item["ground"].toBool() && item["floor_change"].toInt() == 0;
            }
        }
        if (usable) decorations.append(prefab["name"].toString());
    }
    for (const auto &v : pack["creatures"].toArray()) if (!v.toObject()["is_npc"].toBool()) monsters.append(v.toObject()["name"].toString());
    return {{"grounds", grounds}, {"floors", floors}, {"backgrounds", backgrounds}, {"walls", walls}, {"wallChoices", wallChoices},
            {"decorations", decorations}, {"monsters", monsters}};
}

void TileMapGenBridge::failGeneration(const QString &error) {
    m_busy = false; m_generating = false;
    m_status = TileMapGenProcess::errorSummary(error);
    m_errorDetails = m_status == error ? QString() : error;
    emit changed(); emit finished(false);
}

bool TileMapGenBridge::writeJson(const QString &path, const QJsonObject &value) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(value).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

void TileMapGenBridge::refreshCapabilities() {
    if (busy() || !available()) return;
    m_helperMode = "capabilities";
    m_helper.startCommand(generatorDirectory(), {"capabilities"});
    emit changed();
}

void TileMapGenBridge::setAiCredentials(const QString &key, bool remember) {
    if (busy() || key.trimmed().isEmpty()) return;
    m_sessionApiKey = key.trimmed().toUtf8();
    m_aiStatus["configured"] = true;
    if (remember) {
        m_helperMode = "credentials";
        m_helper.startCommand(generatorDirectory(), {"credentials", "save"},
            QJsonDocument(QJsonObject{{"key", QString::fromUtf8(m_sessionApiKey)}, {"remember", true}}).toJson(QJsonDocument::Compact));
    }
    emit changed();
}

void TileMapGenBridge::forgetAiCredentials() {
    if (busy()) return;
    m_sessionApiKey.fill('\0'); m_sessionApiKey.clear();
    m_helperMode = "credentials";
    m_helper.startCommand(generatorDirectory(), {"credentials", "forget"}); emit changed();
}

void TileMapGenBridge::cancelAiPlan() {
    if (m_helperMode.isEmpty()) return;
    const bool preparing = m_helperMode == "prepare-generation";
    if (preparing) m_status = tr("Generation cancelled.");
    else if (aiBusy()) m_status = tr("AI planning cancelled.");
    m_helperMode.clear(); m_helper.cancel();
    if (preparing) m_busy = false;
    emit changed();
}

void TileMapGenBridge::requestAiPlan(const QString &prompt, const QVariantMap &options, const QString &model) {
    if (busy()) return;
    if (!m_settlementAvailable) { emit aiFailed(tr("Update TileMapGenerator to use settlement AI planning.")); return; }
    const auto recipeError = TileMapGenProfiles::validateRecipe(options);
    if (!recipeError.isEmpty()) { emit aiFailed(recipeError); return; }
    if (options.value("generator").toString() != "settlement" || prompt.trimmed().isEmpty() || prompt.size() > 8000) {
        emit aiFailed(tr("Describe a settlement in 1–8000 characters.")); return;
    }
    m_aiWorkDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/DME-SettlementAI-XXXXXX");
    if (!m_aiWorkDirectory->isValid()) { emit aiFailed(tr("Cannot create the AI planning directory.")); return; }
    m_aiFingerprint = fingerprint(catalogJson());
    auto input = QJsonObject::fromVariantMap(options); input.remove("blueprint");
    auto palette = input["palette"].toObject();
    palette["connector"] = options.value("floors").toInt() > 1 ? "stairs" : "";
    input["palette"] = palette;
    const auto pack = m_aiWorkDirectory->filePath("materials.tmgpack");
    const auto error = writeAssets(QUrl::fromLocalFile(pack), options.value("downId").toInt(), options.value("upId").toInt(), false,
                                  options.value("palette").toMap(), options.value("settlement").toMap().value("materials").toMap());
    if (!error.isEmpty()) { emit aiFailed(error); return; }
    const auto request = m_aiWorkDirectory->filePath("options.json");
    if (!writeJson(request, input)) { emit aiFailed(tr("Cannot write the AI planning options.")); return; }
    m_aiPrompt = prompt; m_aiModel = model;
    m_helperMode = "prepare-ai";
    m_helper.startCommand(generatorDirectory(), {"prepare", request, "--pack", pack, "--output", m_aiWorkDirectory->filePath("recipe.json")});
    emit changed();
}

void TileMapGenBridge::helperFinished(bool success, const QString &error) {
    const auto mode = m_helperMode;
    m_helperMode.clear();
    if (mode.isEmpty()) { emit changed(); return; }
    const auto result = QJsonDocument::fromJson(m_helper.output()).object();
    if (mode == "capabilities") {
        m_settlementAvailable = success && result["generators"].toArray().contains("settlement") && result["manifests"].toArray().contains(2);
        if (m_settlementAvailable) {
            m_helperMode = "credentials";
            m_helper.startCommand(generatorDirectory(), {"credentials", "status"});
        }
        emit changed(); return;
    }
    if (!success) {
        if (mode == "prepare-generation") failGeneration(error);
        else emit aiFailed(TileMapGenProcess::errorSummary(error));
        emit changed(); return;
    }
    if (mode == "credentials") {
        m_aiStatus = result.toVariantMap();
        if (!m_sessionApiKey.isEmpty()) m_aiStatus["configured"] = true;
    } else if (mode == "prepare-generation") {
        if (fingerprint(catalogJson()) != m_importFingerprint) { failGeneration(tr("The active assets changed. Generate again.")); return; }
        m_generating = true;
        m_generator.start(generatorDirectory(), m_workDirectory->filePath("prepared.json"),
                          m_workDirectory->filePath("materials.tmgpack"), m_workDirectory->filePath("output"));
    } else if (mode == "prepare-ai") {
        QFile file(m_aiWorkDirectory->filePath("recipe.json"));
        if (!file.open(QIODevice::ReadOnly)) { emit aiFailed(tr("Cannot read the prepared settlement recipe.")); emit changed(); return; }
        const auto recipe = QJsonDocument::fromJson(file.readAll()).object();
        const auto path = m_aiWorkDirectory->filePath("request.json");
        const auto selectedModel = m_aiModel.trimmed().isEmpty() ? m_aiStatus.value("model", "gpt-6.1-sol").toString() : m_aiModel.trimmed();
        if (!writeJson(path, {{"recipe", recipe}, {"prompt", m_aiPrompt}, {"model", selectedModel}})) {
            emit aiFailed(tr("Cannot write the AI planning request.")); emit changed(); return;
        }
        m_helperMode = "plan-ai";
        m_helper.startCommand(generatorDirectory(), {"ai-plan", path, "--pack", m_aiWorkDirectory->filePath("materials.tmgpack")},
            QJsonDocument(QJsonObject{{"key", QString::fromUtf8(m_sessionApiKey)}}).toJson(QJsonDocument::Compact));
    } else if (mode == "plan-ai") {
        if (fingerprint(catalogJson()) != m_aiFingerprint) emit aiFailed(tr("The active assets changed during planning. Request a new plan."));
        else if (result["context_digest"].toString().size() != 64) emit aiFailed(tr("The AI service returned an invalid blueprint."));
        else emit aiPlanReady(result.toVariantMap());
    }
    emit changed();
}

void TileMapGenBridge::generateMap(const QVariantMap &options, QObject *mapView) {
    if (busy()) return;
    m_errorDetails.clear();
    clearPreview();
    const auto recipeError = TileMapGenProfiles::validateRecipe(options);
    if (!recipeError.isEmpty()) { failGeneration(recipeError); return; }
    if (!available()) { failGeneration(tr("Tile Map Generator was not found in %1.").arg(generatorDirectory())); return; }
    if (!m_backend->otbReader()->isLoaded() || !m_backend->otbmReader()->isLoaded()) {
        failGeneration(tr("Load client data and open a destination map first.")); return;
    }
    m_view = qobject_cast<MapView *>(mapView); m_target = m_backend->otbmReader();
    m_placement = options.value("placement", true).toBool();
    m_x = options.value("x", 1000).toInt(); m_y = options.value("y", 1000).toInt(); m_z = options.value("z", 7).toInt();
    const bool settlement = options.value("generator").toString() == "settlement";
    if (settlement) {
        if (!m_settlementAvailable) { failGeneration(tr("Update TileMapGenerator to use Ruins & settlements.")); return; }
        m_z -= options.value("settlement").toMap().value("upper_levels", 1).toInt();
    }
    const int width = options.value("width", 96).toInt(), height = options.value("height", 96).toInt(), floors = options.value("floors", 1).toInt();
    if (width < 24 || width > 65535 || height < 24 || height > 65535 || floors < 1 || floors > 16
        || m_z < 0 || m_z + floors > 16 || (m_placement && (m_x < 0 || m_y < 0 || qint64(m_x) + width > 65536 || qint64(m_y) + height > 65536))) {
        failGeneration(tr("Choose valid dimensions and an origin that fits within map coordinates and floors 0–15.")); return;
    }
    if (m_placement && (!m_view || qint64(width) * height * floors > 100000)) {
        failGeneration(tr("Placement supports up to 100,000 cells. Reduce the size or choose a new map document.")); return;
    }
    const QString style = options.value("generator", "hunting").toString();
    const QString seed = options.value("seed").toString().trimmed();
    if (!QStringList{"hunting", "cave", "linear", "settlement"}.contains(style) || seed.isEmpty() || seed.size() > 256) {
        failGeneration(tr("Choose a generator style and a seed of 1–256 characters.")); return;
    }
    const auto materialError = TileMapGenMaterials::paletteError(catalogJson(), options.value("palette").toMap());
    if (!materialError.isEmpty()) { failGeneration(materialError); return; }
    const int down = options.value("downId").toInt(), up = options.value("upId").toInt();
    if (floors > 1 && (!down || !up)) { failGeneration(tr("Multiple floors require matching down and up stair IDs.")); return; }
    m_workDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/DME-TileMapGen-XXXXXX"));
    if (!m_workDirectory->isValid()) { failGeneration(tr("Cannot create the temporary generation directory.")); return; }
    const auto packPath = m_workDirectory->filePath(QStringLiteral("materials.tmgpack"));
    m_importFingerprint = fingerprint(catalogJson());
    m_status = tr("Preparing the active client's materials..."); m_progress = 0; m_busy = true; emit changed();
    // The CLI consumes material definitions; sprite PNGs are only needed by the external studio.
    const auto error = writeAssets(QUrl::fromLocalFile(packPath), down, up, false, options.value("palette").toMap(),
                                  settlement ? options.value("settlement").toMap().value("materials").toMap() : QVariantMap{});
    if (!error.isEmpty()) { failGeneration(error); return; }
    auto palette = QJsonObject::fromVariantMap(options.value("palette").toMap());
    palette["connector"] = floors > 1 ? QStringLiteral("stairs") : QString();
    QJsonObject request{{"schema_version", 1}, {"generator", style}, {"width", width}, {"height", height},
        {"floors", floors}, {"start_z", m_z}, {"seed", seed}, {"asset_pack", m_importFingerprint.left(16)},
        {"asset_fingerprint", m_importFingerprint}, {"palette", palette},
        {"settings", QJsonObject::fromVariantMap(options.value("settings").toMap())}};
    const auto requestPath = m_workDirectory->filePath(QStringLiteral("request.json"));
    if (settlement) {
        auto input = QJsonObject::fromVariantMap(options);
        input["palette"] = palette;
        if (!writeJson(requestPath, input)) { failGeneration(tr("Cannot write the settlement options.")); return; }
        m_helperMode = "prepare-generation";
        m_status = tr("Preparing settlement materials..."); emit changed();
        m_helper.startCommand(generatorDirectory(), {"prepare", requestPath, "--pack", packPath,
                             "--output", m_workDirectory->filePath("prepared.json")});
        return;
    }
    QSaveFile file(requestPath);
    const auto bytes = QJsonDocument(request).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        failGeneration(tr("Cannot write the generation request.")); return;
    }
    m_generating = true; m_cancel.reset();
    m_status = tr("Generating with Tile Map Generator..."); emit changed();
    m_generator.start(generatorDirectory(), requestPath, packPath, m_workDirectory->filePath(QStringLiteral("output")));
}

#include "documentmanager.h"

#include "otbmreader.h"

#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QDateTime>
#include <QVariantMap>

#include <algorithm>

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
{
    loadPreviousSession();
    newDocument();
    m_initializing = false;
    m_autosaveTimer.setInterval(15000);
    connect(&m_autosaveTimer, &QTimer::timeout,
            this, &DocumentManager::autosaveDueDocument);
    m_autosaveTimer.start();
    writeSession(false);
}

void DocumentManager::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_docs.size() || i == m_current) return;
    m_current = i;
    emit currentChanged();
    writeSession(false);
}

OtbmReader *DocumentManager::current() const
{
    return m_docs.value(m_current, nullptr);
}

QVariantList DocumentManager::tabs() const
{
    QVariantList out;
    for (const OtbmReader *doc : m_docs) {
        QVariantMap t;
        const QString path = doc->filePath();
        t.insert(QStringLiteral("title"),
                 path.isEmpty() ? QStringLiteral("(new map)") : QFileInfo(path).fileName());
        t.insert(QStringLiteral("dirty"), doc->isDirty());
        t.insert(QStringLiteral("loaded"), doc->isLoaded());
        out.push_back(t);
    }
    return out;
}

OtbmReader *DocumentManager::newDocument()
{
    auto *doc = new OtbmReader(this);
    hookDocument(doc);
    m_docs.push_back(doc);
    m_current = static_cast<int>(m_docs.size()) - 1;
    emit tabsChanged();
    emit currentChanged();
    writeSession(false);
    return doc;
}

bool DocumentManager::closeDocument(int i)
{
    if (i < 0 || i >= m_docs.size()) return false;

    OtbmReader *cur = m_docs.value(m_current, nullptr);

    OtbmReader *doc = m_docs.takeAt(i);
    const QString id = m_documentIds.take(doc);
    m_lastEditMs.remove(doc);
    m_lastRecoveryMs.remove(doc);
    m_profileKeys.remove(doc);
    removeRecoveryFiles(id);

    doc->deleteLater();

    if (m_docs.isEmpty()) {
        newDocument();
        return true;
    }

    const int idx = (cur && cur != doc) ? static_cast<int>(m_docs.indexOf(cur)) : -1;
    m_current = idx >= 0 ? idx
                         : std::min(m_current, static_cast<int>(m_docs.size()) - 1);
    emit tabsChanged();
    emit currentChanged();
    writeSession(false);

    for (const OtbmReader *d : m_docs)
        if (d->isLoaded()) return false;
    return true;
}

int DocumentManager::indexOfPath(const QString &path) const
{
    if (path.isEmpty()) return -1;
    for (int i = 0; i < m_docs.size(); ++i)
        if (m_docs[i]->filePath() == path) return i;
    return -1;
}

void DocumentManager::hookDocument(OtbmReader *doc)
{
    documentId(doc);
    connect(doc, &OtbmReader::filePathChanged, this, &DocumentManager::tabsChanged);
    connect(doc, &OtbmReader::dirtyChanged, this, &DocumentManager::tabsChanged);
    connect(doc, &OtbmReader::loadedChanged, this, &DocumentManager::tabsChanged);
    connect(doc, &OtbmReader::mapChanged, this, [this, doc] {
        m_lastEditMs[doc] = QDateTime::currentMSecsSinceEpoch();
    });
    connect(doc, &OtbmReader::filePathChanged, this, [this] { writeSession(false); });
    connect(doc, &OtbmReader::loadedChanged, this, [this] { writeSession(false); });
    connect(doc, &OtbmReader::dirtyChanged, this, [this, doc] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (doc->isDirty()) {
            m_lastEditMs[doc] = now;
            if (!m_lastRecoveryMs.contains(doc)) m_lastRecoveryMs[doc] = now;
        } else {
            removeRecoveryFiles(documentId(doc));
            m_lastRecoveryMs.remove(doc);
        }
        writeSession(false);
    });
}

QString DocumentManager::recoveryDirectory() const
{
    const QString overridePath = qEnvironmentVariable("DME_RECOVERY_DIR");
    if (!overridePath.isEmpty()) return QDir::cleanPath(overridePath);
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/recovery");
}

QString DocumentManager::recoveryPath(const QString &id) const
{
    return QDir(recoveryDirectory()).filePath(id + QStringLiteral(".otbm"));
}

QString DocumentManager::documentId(OtbmReader *doc)
{
    auto it = m_documentIds.constFind(doc);
    if (it != m_documentIds.constEnd()) return it.value();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_documentIds.insert(doc, id);
    return id;
}

void DocumentManager::removeRecoveryFiles(const QString &id)
{
    if (id.isEmpty()) return;
    const QString path = recoveryPath(id);
    const QString base = QFileInfo(path).completeBaseName();
    QDir dir = QFileInfo(path).dir();
    QFile::remove(path);
    QFile::remove(dir.filePath(base + QStringLiteral(".spawn.xml")));
    QFile::remove(dir.filePath(base + QStringLiteral(".house.xml")));
}

void DocumentManager::loadPreviousSession()
{
    const QString sessionPath = QDir(recoveryDirectory()).filePath(
        QStringLiteral("session.json"));
    QFile file(sessionPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) return;
    const QJsonObject root = json.object();
    if (root.value(QStringLiteral("cleanExit")).toBool(true)) return;

    const QJsonArray documents = root.value(QStringLiteral("documents")).toArray();
    for (const QJsonValue &value : documents) {
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("dirty")).toBool()) continue;
        const QString id = object.value(QStringLiteral("id")).toString();
        const QString original = object.value(QStringLiteral("originalPath")).toString();
        const QString recovery = object.value(QStringLiteral("recoveryPath")).toString(
            recoveryPath(id));
        const QFileInfo recoveryInfo(recovery);
        if (id.isEmpty() || !recoveryInfo.exists()) continue;
        const QFileInfo originalInfo(original);
        if (originalInfo.exists()
            && recoveryInfo.lastModified() <= originalInfo.lastModified())
            continue;

        QVariantMap entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("originalPath"), original);
        entry.insert(QStringLiteral("recoveryPath"), recovery);
        entry.insert(QStringLiteral("title"), original.isEmpty()
            ? QStringLiteral("Unsaved map") : originalInfo.fileName());
        entry.insert(QStringLiteral("savedAt"), recoveryInfo.lastModified().toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        entry.insert(QStringLiteral("size"), recoveryInfo.size());
        entry.insert(QStringLiteral("profileKey"),
                     object.value(QStringLiteral("profileKey")).toString());
        entry.insert(QStringLiteral("spawnFile"),
                     object.value(QStringLiteral("spawnFile")).toString());
        entry.insert(QStringLiteral("houseFile"),
                     object.value(QStringLiteral("houseFile")).toString());
        m_recoveries.push_back(entry);
    }
}

void DocumentManager::writeSession(bool cleanExit)
{
    if (m_initializing || (m_shuttingDown && !cleanExit)) return;
    QDir dir(recoveryDirectory());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) return;

    QJsonArray documents;
    for (OtbmReader *doc : m_docs) {
        if (!doc || (!doc->isLoaded() && doc->filePath().isEmpty())) continue;
        const QString id = documentId(doc);
        QJsonObject object;
        object.insert(QStringLiteral("id"), id);
        object.insert(QStringLiteral("originalPath"), doc->filePath());
        object.insert(QStringLiteral("recoveryPath"), recoveryPath(id));
        object.insert(QStringLiteral("dirty"), doc->isDirty());
        object.insert(QStringLiteral("loaded"), doc->isLoaded());
        object.insert(QStringLiteral("profileKey"), m_profileKeys.value(doc));
        object.insert(QStringLiteral("spawnFile"), doc->spawnFile());
        object.insert(QStringLiteral("houseFile"), doc->houseFile());
        documents.append(object);
    }
    for (const QVariant &value : m_recoveries) {
        const QVariantMap entry = value.toMap();
        QJsonObject object;
        object.insert(QStringLiteral("id"), entry.value(QStringLiteral("id")).toString());
        object.insert(QStringLiteral("originalPath"),
                      entry.value(QStringLiteral("originalPath")).toString());
        object.insert(QStringLiteral("recoveryPath"),
                      entry.value(QStringLiteral("recoveryPath")).toString());
        object.insert(QStringLiteral("dirty"), true);
        object.insert(QStringLiteral("pending"), true);
        object.insert(QStringLiteral("profileKey"),
                      entry.value(QStringLiteral("profileKey")).toString());
        object.insert(QStringLiteral("spawnFile"),
                      entry.value(QStringLiteral("spawnFile")).toString());
        object.insert(QStringLiteral("houseFile"),
                      entry.value(QStringLiteral("houseFile")).toString());
        documents.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("cleanExit"), cleanExit);
    root.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("currentIndex"), m_current);
    root.insert(QStringLiteral("documents"), documents);
    QSaveFile output(dir.filePath(QStringLiteral("session.json")));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    output.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    output.commit();
}

void DocumentManager::configureAutosave(bool enabled, int intervalMinutes)
{
    m_autosaveEnabled = enabled;
    m_autosaveIntervalMinutes = std::clamp(intervalMinutes, 1, 60);
    if (enabled) m_autosaveTimer.start();
    else m_autosaveTimer.stop();
}

void DocumentManager::autosaveDueDocument()
{
    if (!m_autosaveEnabled || m_shuttingDown) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 interval = static_cast<qint64>(m_autosaveIntervalMinutes) * 60000;
    for (OtbmReader *doc : m_docs) {
        if (!doc || !doc->isLoaded() || !doc->isDirty()) continue;
        const qint64 lastRecovery = m_lastRecoveryMs.value(doc, now);
        const qint64 lastEdit = m_lastEditMs.value(doc, now);
        const bool due = now - lastRecovery >= interval;
        const bool idle = now - lastEdit >= 5000;
        const bool overdue = now - lastRecovery >= interval * 2;
        if (!due || (!idle && !overdue)) continue;

        const QString path = recoveryPath(documentId(doc));
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (doc->saveRecoveryFile(path)) {
            m_lastRecoveryMs[doc] = now;
            emit autosaveCompleted(QFileInfo(doc->filePath()).fileName());
            writeSession(false);
        } else {
            emit autosaveFailed(QFileInfo(doc->filePath()).fileName(), doc->errorString());
        }
        return;
    }
}

bool DocumentManager::autosaveNow()
{
    bool success = true;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (OtbmReader *doc : m_docs) {
        if (!doc || !doc->isLoaded() || !doc->isDirty()) continue;
        const QString path = recoveryPath(documentId(doc));
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (doc->saveRecoveryFile(path)) m_lastRecoveryMs[doc] = now;
        else success = false;
    }
    writeSession(false);
    return success;
}

void DocumentManager::discardRecoveries()
{
    for (const QVariant &value : m_recoveries)
        removeRecoveryFiles(value.toMap().value(QStringLiteral("id")).toString());
    m_recoveries.clear();
    emit recoveriesChanged();
    writeSession(false);
}

bool DocumentManager::adoptCurrentRecovery(const QString &recoveryId,
                                           const QString &originalPath)
{
    OtbmReader *doc = current();
    if (!doc || recoveryId.isEmpty()) return false;
    m_documentIds[doc] = recoveryId;
    QString spawnFile;
    QString houseFile;
    for (int i = 0; i < m_recoveries.size(); ++i) {
        const QVariantMap entry = m_recoveries[i].toMap();
        if (entry.value(QStringLiteral("id")).toString() == recoveryId) {
            spawnFile = entry.value(QStringLiteral("spawnFile")).toString();
            houseFile = entry.value(QStringLiteral("houseFile")).toString();
            m_recoveries.removeAt(i);
            break;
        }
    }
    doc->adoptRecoveryIdentity(originalPath, spawnFile, houseFile);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_lastEditMs[doc] = now;
    m_lastRecoveryMs[doc] = now;
    emit recoveriesChanged();
    emit tabsChanged();
    writeSession(false);
    return true;
}

void DocumentManager::setCurrentProfileKey(const QString &profileKey)
{
    OtbmReader *doc = current();
    if (!doc || m_profileKeys.value(doc) == profileKey) return;
    m_profileKeys[doc] = profileKey;
    writeSession(false);
}

void DocumentManager::markCleanShutdown()
{
    if (m_shuttingDown) return;
    m_autosaveTimer.stop();
    for (OtbmReader *doc : m_docs)
        removeRecoveryFiles(m_documentIds.value(doc));
    // Pending recovery from an earlier crash remains available until the user
    // explicitly restores or discards it. Closing the startup window must not
    // destroy the only copy of an unsaved map.
    writeSession(m_recoveries.isEmpty());
    m_shuttingDown = true;
}

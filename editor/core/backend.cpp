#include "backend.h"
#include "groundbrushexport.h"
#include "wallbrushexport.h"
#include "obdexport.h"

#include <QDir>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTemporaryFile>

Backend *Backend::s_instance = nullptr;

Backend::Backend(QObject *parent)
    : QObject(parent), m_updateService(&m_docMgr), m_workTimer(&m_docMgr), m_activity(&m_docMgr), m_tileMapGen(this)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_otbReader.setDatReader(&m_datReader);
    m_otbReader.setItemsXml(&m_itemsXml);
    m_tilesetStore.setItemSource(&m_otbReader);
    connect(&m_docMgr, &DocumentManager::currentChanged,
            this, &Backend::otbmReaderChanged);
}

Backend::~Backend()
{
    s_instance = nullptr;
}

Backend *Backend::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    Q_ASSERT(s_instance);
    Q_ASSERT(engine->thread() == s_instance->thread());
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

QUrl Backend::spriteExportUrl(int serverId) const
{
    const QDir downloads(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    return QUrl::fromLocalFile(downloads.filePath(QStringLiteral("sprite-%1.png").arg(serverId)));
}

QString Backend::exportSprite(int serverId, const QUrl &destination) const
{
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return tr("Choose a local PNG file to export the sprite.");

    const int clientId = m_otbReader.clientIdForServerId(serverId);
    const QImage image = m_sprReader.preloadedItemImage(clientId);
    if (image.isNull())
        return tr("No sprite image is available for item %1.").arg(serverId);

    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return tr("Could not save the sprite: %1").arg(file.errorString());

    QImageWriter writer(&file, "PNG");
    if (!writer.write(image))
        return tr("Could not encode the sprite as PNG: %1").arg(writer.errorString());
    if (!file.commit())
        return tr("Could not save the sprite: %1").arg(file.errorString());
    return {};
}

QString Backend::exportGroundBrush(const QVariantList &cells, const QUrl &destination)
{
    return GroundBrushExport::save(cells, destination, [this](int serverId, int variation) {
        const auto *item = m_datReader.itemByClientId(m_otbReader.clientIdForServerId(serverId));
        return item ? m_sprReader.itemPatternImage(*item, variation) : QImage();
    });
}

QString Backend::exportWallBrush(const QVariantList &cells, const QUrl &destination)
{
    return WallBrushExport::save(cells, destination, [this](int serverId) {
        const auto *item = m_datReader.itemByClientId(m_otbReader.clientIdForServerId(serverId));
        return item ? m_sprReader.itemPatternImage(*item, 0) : QImage();
    });
}

QString Backend::previewScreenshotPath() const
{
    QStringList folders;
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!pictures.isEmpty()) folders << pictures + QStringLiteral("/DME Screenshots");
    folders << QCoreApplication::applicationDirPath() + QStringLiteral("/Screenshots");
    const QString localData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!localData.isEmpty()) folders << localData + QStringLiteral("/Screenshots");
    const QString stem = QStringLiteral("DME-Preview-")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz"));
    for (const QString &folder : folders) {
        if (!QDir().mkpath(folder)) continue;
        QDir directory(folder);
        // A known folder may exist but still reject writes (redirected Pictures,
        // protected folders or portable installations in read-only locations).
        QTemporaryFile probe(directory.filePath(QStringLiteral(".dme-write-XXXXXX")));
        if (!probe.open()) continue;
        QString path = directory.filePath(stem + QStringLiteral(".png"));
        for (int suffix = 1; QFileInfo::exists(path); ++suffix)
            path = directory.filePath(stem + QStringLiteral("-%1.png").arg(suffix));
        return path;
    }
    return {};
}

QUrl Backend::obdExportUrl(int serverId) const
{
    const QDir downloads(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    return QUrl::fromLocalFile(downloads.filePath(QStringLiteral("item-%1-%2-v2.obd")
        .arg(serverId).arg(m_datReader.clientVersion())));
}

QString Backend::exportObd(int serverId, const QUrl &destination)
{
    return ObdExport::save(m_datReader, m_sprReader, m_otbReader.clientIdForServerId(serverId), destination);
}

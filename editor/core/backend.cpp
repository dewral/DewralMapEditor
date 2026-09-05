#include "backend.h"

#include <QDir>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>

Backend *Backend::s_instance = nullptr;

Backend::Backend(QObject *parent)
    : QObject(parent), m_updateService(&m_docMgr), m_workTimer(&m_docMgr)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_otbReader.setDatReader(&m_datReader);
    m_otbReader.setItemsXml(&m_itemsXml);
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

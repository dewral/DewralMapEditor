#include "documentmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QDebug>
#include <cstdio>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("DRT"));
    QCoreApplication::setApplicationName(QStringLiteral("DME"));
    QTemporaryDir recoveryDirectory;
    if (!require(recoveryDirectory.isValid(), "Could not create recovery directory")) return 1;
    qputenv("DME_RECOVERY_DIR", recoveryDirectory.path().toLocal8Bit());

    QString recoveryFile;
    {
        DocumentManager manager;
        OtbmReader *document = manager.current();
        if (!require(document != nullptr, "Document manager has no initial document")) return 1;
        if (!require(document->newMap(1024, 1024, 1098, 3, 57),
                     "Could not create recovery test map")) return 1;
        if (!require(document->setMapProperties(QStringLiteral("Recovery test"),
                                                1024, 1024,
                                                QStringLiteral("real-spawn.xml"),
                                                QStringLiteral("real-house.xml")),
                     "Could not set recovery sidecar names")) return 1;
        if (!require(document->addItem(321, 654, 7, 100),
                     "Could not modify recovery test map")) return 1;
        if (!require(document->isDirty(), "Modified test map is not dirty")) return 1;
        if (!manager.autosaveNow()) {
            std::fprintf(stderr, "Recovery autosave failed: %s\n",
                         document->errorString().toLocal8Bit().constData());
            return 1;
        }
        if (!require(document->isDirty(), "Recovery autosave cleared dirty state")) return 1;
        if (!require(document->filePath().isEmpty(),
                     "Recovery autosave changed the document path")) return 1;
    }

    {
        DocumentManager deferredSession;
        if (!require(deferredSession.recoveryCount() == 1,
                     "Interrupted session recovery was not detected")) return 1;
        deferredSession.markCleanShutdown();
    }

    {
        DocumentManager recoveredSession;
        if (!require(recoveredSession.recoveryCount() == 1,
                     "Closing recovery prompt discarded pending recovery")) return 1;
        const QVariantMap entry = recoveredSession.recoveries().front().toMap();
        recoveryFile = entry.value(QStringLiteral("recoveryPath")).toString();
        if (!require(QFileInfo::exists(recoveryFile), "Recovery OTBM does not exist")) return 1;

        OtbmReader *recovered = recoveredSession.current();
        if (!require(recovered && recovered->loadFile(recoveryFile),
                     "Recovery OTBM could not be loaded")) return 1;
        const OtbmTile *tile = recovered->tileAt(321, 654, 7);
        if (!require(tile && !tile->items.empty() && tile->items.front().server_id == 100,
                     "Recovery OTBM does not contain the unsaved edit")) return 1;
        if (!require(recoveredSession.adoptCurrentRecovery(
                         entry.value(QStringLiteral("id")).toString(), QString()),
                     "Could not adopt recovered document identity")) return 1;
        if (!require(recovered->spawnFile() == QStringLiteral("real-spawn.xml")
                     && recovered->houseFile() == QStringLiteral("real-house.xml"),
                     "Recovery kept its internal sidecar names")) return 1;

        recoveredSession.markCleanShutdown();
    }

    if (!require(!QFileInfo::exists(recoveryFile),
                 "Discarding recovery left its OTBM behind")) return 1;
    return 0;
}

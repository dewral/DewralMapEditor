#include "documentmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QDebug>
#include <cstdio>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

}

class DocumentRecoveryTest {
public:
    static bool drain(DocumentManager &manager) {
        QElapsedTimer timeout;timeout.start();int idlePasses=0;
        do {
            QCoreApplication::processEvents();QThread::msleep(1);
            idlePasses = (!manager.m_recoveryRunning && manager.m_forcedRecovery.isEmpty()) ? idlePasses+1 : 0;
        } while(timeout.elapsed()<10000 && idlePasses<3);
        return require(!manager.m_recoveryRunning && manager.m_forcedRecovery.isEmpty(), "Background recovery did not finish");
    }
    static int profile(const QString &mapPath) {
        QTemporaryDir temp;qputenv("DME_RECOVERY_DIR",temp.path().toUtf8());
        DocumentManager manager;manager.configureAutosave(false,1);
        if(!manager.current()->loadFile(mapPath)||!manager.current()->addItem(100,100,7,100))return 2;
        QElapsedTimer elapsed;elapsed.start();qint64 previous=0,maxGap=0;int ticks=0;
        QTimer heartbeat;heartbeat.setTimerType(Qt::PreciseTimer);heartbeat.setInterval(5);
        QObject::connect(&heartbeat,&QTimer::timeout,[&]{const auto now=elapsed.elapsed();maxGap=qMax(maxGap,now-previous);previous=now;++ticks;});heartbeat.start();
        manager.autosaveNow();const auto enqueueMs=elapsed.elapsed();
        if(!drain(manager))return 3;
        qInfo()<<"RECOVERY_PROFILE tiles="<<manager.current()->tileCount()<<"enqueue_ms="<<enqueueMs<<"total_ms="<<elapsed.elapsed()<<"gui_ticks="<<ticks<<"max_gui_gap_ms="<<maxGap;
        manager.markCleanShutdown();return ticks>0?0:4;
    }
    static bool run() {
        QTemporaryDir temp;qputenv("DME_RECOVERY_DIR",temp.path().toUtf8());
        DocumentManager manager;manager.configureAutosave(true,1);
        auto *first=manager.current();first->newMap(64,64,1098,3,57);first->addItem(1,1,7,100);
        const QString input=temp.filePath("broken-map.otbm");
        if(!first->saveFile(input))return false;
        QFile bad(temp.filePath(first->spawnFile()));
        // Ensure an unread XML sidecar makes just this document unsavable.
        if(first->spawnFile().isEmpty()) {
            if(!first->setMapProperties("Broken",256,256,"broken-spawn.xml","broken-house.xml"))return false;
            if(!first->saveFile(input))return false;
            bad.setFileName(temp.filePath("broken-spawn.xml"));
        }
        if(!bad.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;
        bad.write("<spawns><spawn broken='yes'/></spawns>");bad.close();
        if(!first->loadFile(input)||!first->addItem(1,2,7,100))return false;
        auto *second=manager.newDocument();second->newMap(64,64,1098,3,57);second->addItem(2,2,7,100);
        int failures=0,completed=0;
        QObject::connect(&manager,&DocumentManager::autosaveFailed,[&]{++failures;});
        QObject::connect(&manager,&DocumentManager::autosaveCompleted,[&]{++completed;});
        const qint64 due=manager.m_clock.elapsed()-180000;
        manager.m_lastRecoveryMs[first]=manager.m_lastRecoveryMs[second]=due;
        manager.m_lastEditMs[first]=manager.m_lastEditMs[second]=due;
        manager.autosaveDueDocument();
        if(!drain(manager)||!require(failures==1&&completed==1,"A failed map blocked another document's autosave"))return false;
        manager.autosaveDueDocument();
        if(!require(failures==1&&!manager.m_recoveryRunning,"Failed autosave ignored its retry backoff"))return false;
        manager.configureAutosave(false,1);
        // Snapshot isolation: editing after enqueue must not mutate that snapshot.
        manager.m_forcedRecovery.insert(second);manager.autosaveDueDocument();
        second->addItem(3,3,7,101);
        if(!drain(manager)||!require(second->isDirty(),"Recovery cleared a later edit"))return false;
        OtbmReader snapshot;
        if(!snapshot.loadFile(manager.m_recoveryPaths.value(second))
            ||!require(snapshot.tileAt(2,2,7)&&!snapshot.tileAt(3,3,7),"Worker serialized live map data"))return false;
        // A manual save invalidates the in-flight job, including its sidecars.
        manager.m_forcedRecovery.insert(second);manager.autosaveDueDocument();
        const QString stale=manager.m_recoveryJobPath;
        if(!second->saveFile(temp.filePath("manual.otbm"))||!drain(manager))return false;
        if(!require(!QFileInfo::exists(stale)&&!manager.m_recoveryPaths.contains(second),"Late recovery resurrected a manually saved document"))return false;
        // Completion after Close must not publish a stale document snapshot.
        second->addItem(4,4,7,102);manager.m_forcedRecovery.insert(second);manager.autosaveDueDocument();
        const QString closed=manager.m_recoveryJobPath;manager.closeDocument(1);
        if(!drain(manager)||!require(!QFileInfo::exists(closed),"Recovery survived a closed document"))return false;
        manager.markCleanShutdown();
        return true;
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    if (argc==3 && QString::fromLocal8Bit(argv[1])=="--profile-autosave") return DocumentRecoveryTest::profile(QString::fromLocal8Bit(argv[2]));
    QCoreApplication::setOrganizationName(QStringLiteral("DRT"));
    QCoreApplication::setApplicationName(QStringLiteral("DME"));
    QTemporaryDir recoveryDirectory;
    if (!require(recoveryDirectory.isValid(), "Could not create recovery directory")) return 1;
    qputenv("DME_RECOVERY_DIR", recoveryDirectory.path().toLocal8Bit());

    if (!DocumentRecoveryTest::run()) return 1;
    qputenv("DME_RECOVERY_DIR", recoveryDirectory.path().toUtf8());
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

// Source-only audit harness. All data lives in QTemporaryDir; no app source changes.
#include "documentmanager.h"
#include "otbmreader.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <functional>

static bool waitFor(const std::function<bool()> &condition) {
    QElapsedTimer timer; timer.start();
    while (!condition() && timer.elapsed() < 8000) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return condition();
}
static QByteArray readFile(const QString &path) {
    QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
static QJsonObject manifest(const QString &root) {
    return QJsonDocument::fromJson(readFile(root + "/session.json")).object();
}
static QString snapshotPath(const QString &root) {
    const auto docs = manifest(root).value("documents").toArray();
    return docs.isEmpty() ? QString() : docs.first().toObject().value("recoveryPath").toString();
}
static bool autosave(DocumentManager &manager) {
    bool done = false, failed = false;
    const auto ok = QObject::connect(&manager, &DocumentManager::autosaveCompleted,
                                     [&] { done = true; });
    const auto error = QObject::connect(&manager, &DocumentManager::autosaveFailed,
                                        [&] { failed = true; });
    const bool result = manager.autosaveNow() && waitFor([&] { return done || failed; }) && done;
    QObject::disconnect(ok); QObject::disconnect(error); return result;
}
static int child(const QString &role, const QString &root) {
    qputenv("DME_RECOVERY_DIR", root.toUtf8());
    DocumentManager manager; manager.configureAutosave(false, 1);
    if (role == "writer") {
        auto *doc = manager.current();
        if (!doc->newMap(256,256,1098,3,57) || !doc->addItem(10,10,7,100)
            || !autosave(manager)) return 2;
    }
    std::cout << "READY" << std::endl;
    std::string command; std::getline(std::cin, command);
    if (role == "blank") {
        manager.newDocument();
        manager.markCleanShutdown();
    }
    return 0;
}
static bool ready(QProcess &p, const QString &role, const QString &root) {
    p.start(QCoreApplication::applicationFilePath(), {"--child",role,root});
    if (!p.waitForStarted(5000)) return false;
    QByteArray output;
    QElapsedTimer timer; timer.start();
    while (!output.contains("READY") && timer.elapsed() < 8000) {
        p.waitForReadyRead(100); output += p.readAllStandardOutput();
        if (p.state() == QProcess::NotRunning) break;
    }
    return output.contains("READY");
}
static bool twoProcesses(const QString &root, QJsonObject &result) {
    QDir().mkpath(root);
    QProcess blank, writer;
    // Blank instance starts first; it cannot have discovered the future snapshot.
    if (!ready(blank,"blank",root) || !ready(writer,"writer",root)) return false;
    const QString snapshot = snapshotPath(root);
    const bool existed = QFileInfo::exists(snapshot);
    const bool dirtyBefore = !manifest(root).value("cleanExit").toBool(true);
    blank.write("publish\n"); blank.closeWriteChannel();
    if (!blank.waitForFinished(5000) || blank.exitCode() != 0) return false;
    const bool overwrittenClean = manifest(root).value("cleanExit").toBool(false);
    // Terminate ONLY this harness's child with synthetic temporary data.
    writer.kill(); if (!writer.waitForFinished(5000)) return false;
    qputenv("DME_RECOVERY_DIR",root.toUtf8());
    DocumentManager restored; restored.configureAutosave(false,1);
    result = {{"snapshotExistedBefore",existed},{"writerSessionDirty",dirtyBefore},
              {"otherInstanceMarkedClean",overwrittenClean},
              {"snapshotStillExists",QFileInfo::exists(snapshot)},
              {"offeredRecoveries",restored.recoveryCount()}};
    return existed && dirtyBefore && overwrittenClean
        && QFileInfo::exists(snapshot) && restored.recoveryCount() == 0;
}
static bool timestampCase(const QString &root, QJsonObject &result) {
    QDir().mkpath(root); qputenv("DME_RECOVERY_DIR",root.toUtf8());
    QString snapshot; const QString original = root + "/original.otbm";
    {
        DocumentManager manager; manager.configureAutosave(false,1);
        auto *doc = manager.current();
        if (!doc->newMap(256,256,1098,3,57) || !doc->addItem(1,1,7,100)
            || !doc->saveFile(original)) return false;
        QFile f(original);
        if (!f.open(QIODevice::ReadWrite)
            || !f.setFileTime(QDateTime::currentDateTimeUtc().addSecs(3600),QFileDevice::FileModificationTime)) return false;
        f.close();
        if (!doc->addItem(2,2,7,101) || !autosave(manager)) return false;
        snapshot = snapshotPath(root);
        // Destructor without markCleanShutdown simulates the persisted crash state.
    }
    OtbmReader saved, recovery;
    if (!saved.loadFile(original) || !recovery.loadFile(snapshot)) return false;
    DocumentManager restored; restored.configureAutosave(false,1);
    result = {{"originalHasNewTile",saved.tileAt(2,2,7)!=nullptr},
              {"snapshotHasNewTile",recovery.tileAt(2,2,7)!=nullptr},
              {"originalMtimeAfterSnapshot",QFileInfo(original).lastModified()>QFileInfo(snapshot).lastModified()},
              {"offeredRecoveries",restored.recoveryCount()},
              {"snapshotStillExists",QFileInfo::exists(snapshot)}};
    return !saved.tileAt(2,2,7) && recovery.tileAt(2,2,7)
        && restored.recoveryCount()==0 && QFileInfo::exists(snapshot);
}
static bool backupCase(const QString &root, QJsonObject &result) {
    QDir().mkpath(root); OtbmReader map;
    const QString path = root+"/world.otbm";
    if (!map.newMap(256,256,1098,3,57) || !map.addItem(1,1,7,100)
        || !map.setMapProperties("Backup alias",256,256,"world.otbm.bak",QString())
        || !map.saveFile(path)) return false;
    const QByteArray original = readFile(path);
    if (!map.addItem(2,2,7,101)) return false;
    const bool saved = map.saveFile(path);
    const QByteArray backup = readFile(path+".bak");
    OtbmReader primary, fallback;
    const bool primaryValid = primary.loadFile(path);
    const bool backupValid = fallback.loadFile(path+".bak");
    result = {{"saveAccepted",saved},{"primaryValidOtbm",primaryValid},
              {"backupEqualsPreviousMap",backup==original},{"backupValidOtbm",backupValid},
              {"backupContainsSpawnsXml",backup.contains("<spawns")}};
    return saved && primaryValid && backup!=original && !backupValid && backup.contains("<spawns");
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    const auto args = app.arguments();
    if (args.size()==4 && args[1]=="--child") return child(args[2],args[3]);
    QTemporaryDir fixture;
    if (!fixture.isValid()) return 1;
    QJsonObject processes,timestamps,backups;
    const bool a=twoProcesses(fixture.path()+"/processes",processes);
    const bool b=timestampCase(fixture.path()+"/timestamps",timestamps);
    const bool c=backupCase(fixture.path()+"/backups",backups);
    QJsonObject output{{"multiProcess",processes},{"timestamps",timestamps},{"backupAlias",backups},
                       {"allObserved",a&&b&&c}};
    std::cout << QJsonDocument(output).toJson().toStdString();
    return a&&b&&c ? 0 : 1;
}

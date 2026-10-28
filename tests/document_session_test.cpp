// Regression tests use only temporary synthetic data and their own child processes.
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
static QString dirtyManifest(const QString &root) {
    const auto files = QDir(root).entryInfoList({"session-*.json","session.json"}, QDir::Files, QDir::Time);
    for (const auto &f : files) {
        const auto object = QJsonDocument::fromJson(readFile(f.absoluteFilePath())).object();
        for (const auto &value : object.value("documents").toArray())
            if (value.toObject().value("dirty").toBool()
                && QFileInfo::exists(value.toObject().value("recoveryPath").toString()))
                return f.absoluteFilePath();
    }
    return {};
}
static QString snapshotPath(const QString &root) {
    const auto object = QJsonDocument::fromJson(readFile(dirtyManifest(root))).object();
    for (const auto &value : object.value("documents").toArray()) {
        const auto row = value.toObject();
        if (row.value("dirty").toBool() && QFileInfo::exists(row.value("recoveryPath").toString()))
            return row.value("recoveryPath").toString();
    }
    return {};
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
static bool testProcesses(const QString &root) {
    QDir().mkpath(root);
    QProcess blank, writer;
    if (!ready(blank,"blank",root) || !ready(writer,"writer",root)) return false;
    const QString snapshot = snapshotPath(root);
    const QString manifest = dirtyManifest(root);
    if (snapshot.isEmpty() || manifest.isEmpty()) return false;
    qputenv("DME_RECOVERY_DIR",root.toUtf8());
    {
        DocumentManager liveProbe; liveProbe.configureAutosave(false,1);
        if (liveProbe.recoveryCount()!=0) return false;
        liveProbe.discardRecoveries(); liveProbe.markCleanShutdown();
        if (!QFileInfo::exists(snapshot)) return false;
    }
    blank.write("publish\n"); blank.closeWriteChannel();
    if (!blank.waitForFinished(5000) || blank.exitCode()!=0) return false;
    const auto state = QJsonDocument::fromJson(readFile(manifest)).object();
    if (state.value("cleanExit").toBool(true) || !QFileInfo::exists(snapshot)) return false;
    writer.kill(); if (!writer.waitForFinished(5000)) return false;
    DocumentManager recovered; recovered.configureAutosave(false,1);
    if (recovered.recoveryCount()!=1 || !QFileInfo::exists(snapshot)) return false;
    OtbmReader copy;
    if (!copy.loadFile(recovered.recoveries().first().toMap().value("recoveryPath").toString())
        || !copy.tileAt(10,10,7)) return false;
    {
        DocumentManager competing; competing.configureAutosave(false,1);
        if (competing.recoveryCount()!=0) return false;
        competing.discardRecoveries(); competing.markCleanShutdown();
        if (!QFileInfo::exists(snapshot)) return false;
    }
    recovered.markCleanShutdown();
    return true;
}
static bool testTimestamp(const QString &root, bool equal) {
    QDir().mkpath(root); qputenv("DME_RECOVERY_DIR",root.toUtf8());
    QString snapshot; const QString original=root+"/original.otbm";
    {
        DocumentManager manager; manager.configureAutosave(false,1);
        auto *doc=manager.current();
        if (!doc->newMap(256,256,1098,3,57) || !doc->addItem(1,1,7,100)
            || !doc->saveFile(original) || !doc->addItem(2,2,7,101)
            || !autosave(manager)) return false;
        snapshot=snapshotPath(root);
        QFile file(original);
        const QDateTime time=equal ? QFileInfo(snapshot).lastModified()
                                  : QDateTime::currentDateTimeUtc().addSecs(3600);
        if (!file.open(QIODevice::ReadWrite)
            || !file.setFileTime(time,QFileDevice::FileModificationTime)) return false;
    }
    DocumentManager restored; restored.configureAutosave(false,1);
    OtbmReader saved,copy;
    const bool ok=restored.recoveryCount()==1 && saved.loadFile(original)
        && copy.loadFile(snapshot) && !saved.tileAt(2,2,7) && copy.tileAt(2,2,7);
    restored.markCleanShutdown();
    return ok;
}
static bool testLegacy(const QString &root) {
    QDir().mkpath(root); qputenv("DME_RECOVERY_DIR",root.toUtf8());
    OtbmReader map;
    const QString snapshot=root+"/legacy.otbm";
    if (!map.newMap(256,256,1098,3,57) || !map.addItem(9,9,7,100)
        || !map.saveRecoveryFile(snapshot)) return false;
    QJsonObject entry{{"id","legacy-fixture"},{"dirty",true},{"recoveryPath",snapshot},
                      {"profileKey","test-profile"},{"spawnFile","real-spawn.xml"}};
    QJsonObject session{{"version",1},{"cleanExit",false},{"documents",QJsonArray{entry}}};
    QFile legacy(root+"/session.json");
    if (!legacy.open(QIODevice::WriteOnly)) return false;
    legacy.write(QJsonDocument(session).toJson()); legacy.close();
    {
        DocumentManager restored; restored.configureAutosave(false,1);
        if (restored.recoveryCount()!=1 || legacy.exists()
            || restored.recoveries().first().toMap().value("profileKey")!="test-profile") return false;
        restored.markCleanShutdown();
    }
    {
        DocumentManager reopened; reopened.configureAutosave(false,1);
        if (reopened.recoveryCount()!=1) return false;
        const auto row=reopened.recoveries().first().toMap();
        if (!reopened.current()->loadFile(row.value("recoveryPath").toString())
            || !reopened.adoptCurrentRecovery(row.value("id").toString(),{})
            || !reopened.current()->tileAt(9,9,7)
            || reopened.current()->spawnFile()!="real-spawn.xml") return false;
        reopened.markCleanShutdown();
    }
    return !QFileInfo::exists(snapshot);
}
// Existing test friendship allows failure injection without production hooks.
class DocumentRecoveryTest {
public:
    static bool migrationFailure(const QString &root) {
        QDir().mkpath(root);qputenv("DME_RECOVERY_DIR",root.toUtf8());
        DocumentManager manager;manager.configureAutosave(false,1);
        OtbmReader map;
        const QString snapshot=root+"/pending.otbm",legacyPath=root+"/session.json";
        if (!map.newMap(256,256,1098,3,57)||!map.addItem(1,1,7,100)
            ||!map.saveRecoveryFile(snapshot)) return false;
        const QJsonObject entry{{"id","pending-fixture"},{"dirty",true},{"recoveryPath",snapshot}};
        QFile legacy(legacyPath);
        if (!legacy.open(QIODevice::WriteOnly)) return false;
        legacy.write(QJsonDocument(QJsonObject{{"version",1},{"cleanExit",false},
                                              {"documents",QJsonArray{entry}}}).toJson());legacy.close();
        manager.loadPreviousSession();
        if (manager.recoveryCount()!=1||manager.m_claimedSessions.empty()) return false;
        const QString sessionPath=manager.m_sessionPath;
        const QString blocked=root+"/directory-not-file";
        if (!QDir().mkpath(blocked)) return false;
        manager.m_sessionPath=blocked;
        manager.writeSession(false);
        if (!QFileInfo::exists(legacyPath)||!QFileInfo::exists(snapshot)
            ||manager.m_claimedSessions.empty()) return false;
        manager.m_sessionPath=sessionPath;
        manager.writeSession(false);
        if (QFileInfo::exists(legacyPath)||!manager.m_claimedSessions.empty()) return false;
        manager.markCleanShutdown();
        return QFileInfo::exists(snapshot);
    }
};

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if (args.size()==4 && args[1]=="--child") return child(args[2],args[3]);
    QTemporaryDir fixture;
    if (!fixture.isValid()) return 1;
    for (const auto &test : {std::make_pair("processes",testProcesses(fixture.path()+"/processes")),
                             std::make_pair("future-mtime",testTimestamp(fixture.path()+"/future",false)),
                             std::make_pair("equal-mtime",testTimestamp(fixture.path()+"/equal",true)),
                             std::make_pair("legacy",testLegacy(fixture.path()+"/legacy")),
                             std::make_pair("migration-failure",DocumentRecoveryTest::migrationFailure(fixture.path()+"/failure"))}) {
        if (!test.second) { std::cerr<<"Failed: "<<test.first<<'\n';return 1; }
    }
    std::cout<<"Session ownership, timestamps and legacy migration passed\n";
    return 0;
}

#include "backend.h"
#include "clientloadservice.h"
#include "client_fixture.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QElapsedTimer>

using namespace DmeClientFixture;
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("DME-Tests");
    app.setApplicationName("ClientLoader-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStandardPaths::setTestModeEnabled(true);
    if (app.arguments().size() == 3 && (app.arguments()[1] == "--write-fixture"
        || app.arguments()[1] == "--write-versioned-fixture")) {
        const QString output = app.arguments()[2];
        if (QFileInfo::exists(output) || !QDir().mkpath(output)) return 2;
        if (!writeFile(output + "/Tibia.dat", datFile()) || !writeFile(output + "/Tibia.spr", spriteFile())
            || !writeFile(output + "/items.otb", otbFile(1000, 9, app.arguments()[1] == "--write-versioned-fixture"))
            || !writeFile(output + "/client.otfi", "DatSpr\n  extended: false\n  transparency: true\n")) return 2;
        const auto node = [](int type, QByteArray bytes, const QByteArray &children) {
            bytes.prepend(char(type)); QByteArray result(1,char(0xfe));
            for (char byte : bytes) { if (quint8(byte)>=0xfd) result.append(char(0xfd)); result.append(byte); }
            return result + children + char(0xff);
        };
        QByteArray header, area, tiles;
        appendU32(header,2); appendU16(header,128); appendU16(header,128); appendU32(header,3); appendU32(header,20);
        appendU16(area,0); appendU16(area,0); area.append(char(7));
        for (int y=32;y<96;++y) for (int x=32;x<96;++x) {
            QByteArray tile;tile.append(char(x));tile.append(char(y));tile.append(char(9));appendU16(tile,1005);
            tiles+=node(5,tile,{});
        }
        if (!writeFile(output + "/fixture.otbm",QByteArray(4,'\0')+node(0,header,node(2,{},node(4,area,tiles))))) return 2;
        return 0;
    }
    QTemporaryDir fixture;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, fixture.path());
    qputenv("DME_RECOVERY_DIR", fixture.filePath("recovery").toUtf8());
    if (!writeFile(fixture.filePath("Tibia.dat"), datFile(20000))
        || !writeFile(fixture.filePath("Tibia.spr"), spriteFile())
        || !writeFile(fixture.filePath("items.otb"), otbFile())
        || !writeFile(fixture.filePath("client.otfi"), "DatSpr\n  extended: false\n  transparency: true\n")) return 1;
    {
        Backend backend(nullptr);
        auto *service = backend.clientLoader();
        auto *originalDat = backend.datReader();
        auto *originalSpr = backend.sprReader();
        auto *originalOtb = backend.otbReader();
        QHash<quint64, bool> results;
        QObject::connect(service, &ClientLoadService::finished, &app,
            [&](quint64 id, const QString &, const QString &, bool success, const QString &) { results.insert(id, success); });
        int heartbeats = 0;
        QTimer heartbeat; heartbeat.setInterval(1);
        QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] { ++heartbeats; }); heartbeat.start();
        const auto wait = [&](quint64 id) {
            QElapsedTimer elapsed; elapsed.start();
            while (!results.contains(id) && elapsed.elapsed() < 15000) {
                app.processEvents(); QThread::msleep(1);
            }
            return results.contains(id);
        };
        const auto id = service->request("860", 860, fixture.path(), false);
        if (!require(wait(id) && results.value(id), "Could not publish prepared client")) return 1;
        if (!require(heartbeats > 0, "Client preparation blocked event processing")) return 1;
        if (!require(backend.datReader() == originalDat && backend.sprReader() == originalSpr
            && backend.otbReader() == originalOtb, "Client publication replaced QML model objects")) return 1;
        if (!require(originalDat->itemCount() == 20009 && originalOtb->rowCount() == 9
            && !originalSpr->preloadedItemImage(105).isNull(), "Prepared state lost definitions or palette images")) return 1;
        const auto revision = originalSpr->itemImagesRevision();
        const auto cached = service->request("860", 860, fixture.path(), false);
        if (!require(wait(cached) && results.value(cached) && originalSpr->itemImagesRevision() == revision,
                     "Cache hit rebuilt palette data")) return 1;
        const auto cancelled = service->request("cancelled", 860, fixture.path(), false);
        service->cancel();
        if (!require(wait(cancelled) && !results.value(cancelled), "Cancellation did not finish the request")) return 1;
        const auto replacement = service->request("860", 860, fixture.path(), false);
        if (!require(wait(replacement) && results.value(replacement), "Cancelled generation replaced a newer result")) return 1;
        if (!writeFile(fixture.filePath("Tibia.dat"), "invalid")) return 1;
        const auto bad = service->request("bad", 860, fixture.path(), false);
        if (!require(wait(bad) && !results.value(bad) && originalDat->itemCount() == 20009
            && originalSpr->itemImagesRevision() == revision, "Failure damaged the previous client")) return 1;
        if (!writeFile(fixture.filePath("Tibia.dat"), datFile(20000))) return 1;
        const auto changing = service->request("changing", 860, fixture.path(), false);
        bool changed = false;
        QElapsedTimer changeDeadline; changeDeadline.start();
        while (!results.contains(changing) && changeDeadline.elapsed() < 15000) {
            if (!changed && service->progress() >= 45) {
                QFile source(fixture.filePath("Tibia.dat"));
                if (!source.open(QIODevice::Append) || source.write("changed") != 7) return 1;
                source.close(); changed = true;
            }
            app.processEvents(); QThread::msleep(1);
        }
        if (!require(changed && results.contains(changing) && !results.value(changing)
            && originalSpr->itemImagesRevision() == revision, "Changed source files were published")) return 1;
        // Finite worker owns no Backend/QObject pointer after it leaves this scope.
        service->request("closing", 860, fixture.path(), false);
    }
    QThreadPool::globalInstance()->waitForDone();
    qInfo() << "Client load: publication, GUI event processing, identity, cache, cancellation, failure and shutdown passed";
    return 0;
}

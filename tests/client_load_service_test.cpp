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
// The shared atlas fixture deliberately contains sprite 999 outside its SPR.
// Client validation needs a matching pair; preserve that negative fixture elsewhere.
QByteArray matchingDatFile(int extra=0) {
    auto bytes=datFile(extra);
    QByteArray invalid;appendU16(invalid,999);QByteArray valid;appendU16(valid,5);
    const int at=bytes.indexOf(invalid,12);if(at>=0)bytes.replace(at,2,valid);
    return bytes;
}
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
        if (!writeFile(output + "/Tibia.dat", matchingDatFile()) || !writeFile(output + "/Tibia.spr", spriteFile())
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
    if (!writeFile(fixture.filePath("Tibia.dat"), matchingDatFile(20000))
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
            [&](quint64 id, const QString &, const QString &, bool success, const QString &error) { if(!success)qInfo()<<error;results.insert(id, success); });
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
        QHash<quint64,QVariantMap> validations;
        QObject::connect(service,&ClientLoadService::validationFinished,&app,[&](quint64 request,const QVariantMap &report){validations.insert(request,report);});
        const auto validate=[&](const QVariantMap &profile) {
            const auto request=service->validateProfile(profile);QElapsedTimer elapsed;elapsed.start();
            while(!validations.contains(request) && elapsed.elapsed()<15000){app.processEvents();QThread::msleep(1);}
            return validations.value(request);
        };
        QVariantMap profile{{"id","explicit"},{"version",860},{"files",QVariantMap{
            {"dat",fixture.filePath("Tibia.dat")},{"spr",fixture.filePath("Tibia.spr")},
            {"otb",fixture.filePath("items.otb")},{"otfi",fixture.filePath("client.otfi")},{"xml","-"}}}};
        const auto valid=validate(profile);
        if(!require(valid.value("success").toBool() && originalSpr->itemImagesRevision()==revision
            && originalDat->itemCount()==20009,"Validation mutated active models or rejected explicit paths"))return 1;
        auto files=profile.value("files").toMap();
        if(!writeFile(fixture.filePath("invalid.xml"),"<items><item>"))return 1;
        files["xml"]=fixture.filePath("invalid.xml");profile["files"]=files;
        if(!require(!validate(profile).value("success").toBool(),"Malformed optional XML was accepted"))return 1;
        files["xml"]="-";profile["files"]=files;
        if(!require(validate(profile).value("success").toBool(),"Disabling optional XML did not recover validation"))return 1;
        if(!writeFile(fixture.filePath("invalid.otfi"),"unrecognized text"))return 1;
        files["otfi"]=fixture.filePath("invalid.otfi");profile["files"]=files;
        if(!require(!validate(profile).value("success").toBool(),"Malformed OTFI was accepted"))return 1;
        QFile::remove(fixture.filePath("invalid.otfi"));
        QFile::remove(fixture.filePath("invalid.xml"));
        QTemporaryDir ambiguous;
        if(!writeFile(ambiguous.filePath("first.dat"),matchingDatFile()) || !writeFile(ambiguous.filePath("second.dat"),matchingDatFile()))return 1;
        const auto report=service->resolveProfile({{"version",860},{"folder",ambiguous.path()}});
        if(!require(report.value("errors").toMap().value("dat").toString().contains("Several"),"Ambiguous DAT was silently chosen"))return 1;
        if (!writeFile(fixture.filePath("Tibia.dat"), "invalid")) return 1;
        const auto bad = service->request("bad", 860, fixture.path(), false);
        if (!require(wait(bad) && !results.value(bad) && originalDat->itemCount() == 20009
            && originalSpr->itemImagesRevision() == revision, "Failure damaged the previous client")) return 1;
        if (!writeFile(fixture.filePath("Tibia.dat"), matchingDatFile(20000))) return 1;
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

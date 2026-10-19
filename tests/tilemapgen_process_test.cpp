#include "tilemapgenprocess.h"
#include "tmgformat.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>

static bool check(bool condition, const QString &message) {
    if (!condition) qCritical().noquote() << message;
    return condition;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString traceback = "Tile Map Generator failed (exit 1).\nTraceback (most recent call last):\n  File \"launcher.py\", line 12\n  File \"tilemapgen/cli.py\", line 48\ntilemapgen.engine.GenerationError: background needs blocking ground";
    if (!check(TileMapGenProcess::errorSummary(traceback) == "Tile Map Generator failed (exit 1).\nbackground needs blocking ground", "The actual generation error must be visible before traceback details")) return 1;
    if (!check(TileMapGenProcess::errorSummary(traceback + "\n[PYI-1234:ERROR] Failed to execute script 'launcher' due to unhandled exception!")
               == TileMapGenProcess::errorSummary(traceback), "PyInstaller's footer must not hide the useful error")) return 1;
    if (argc != 3) return 1;
    const QString generator = QString::fromLocal8Bit(argv[1]), fixture = QString::fromLocal8Bit(argv[2]);
    QTemporaryDir directory(QDir::tempPath() + "/DME generation test XXXXXX");
    if (!check(directory.isValid(), "Cannot create test directory")) return 2;
    TileMapGenProcess runner;
    bool done = false, success = false, timedOut = false;
    QString error;
    int progressEvents = 0;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; runner.cancel(); loop.quit(); });
    QObject::connect(&runner, &TileMapGenProcess::finished, &loop, [&](bool ok, const QString &message) {
        done = true; success = ok; error = message; loop.quit();
    });
    QObject::connect(&runner, &TileMapGenProcess::progressChanged, &loop, [&](int value, const QString &stage) {
        if (value >= 0 && value <= 100 && !stage.isEmpty()) ++progressEvents;
    });
    const auto run = [&](const QString &root, const QString &request, const QString &output, bool cancel = false) {
        done = success = timedOut = false; error.clear(); progressEvents = 0;
        runner.start(root, request, fixture + "/materials.tmgpack", output);
        if (cancel) QTimer::singleShot(50, &runner, &TileMapGenProcess::cancel);
        timeout.start(60000);
        if (!done) loop.exec();
        timeout.stop();
        return done && !timedOut;
    };
    if (!check(run(directory.path(), "missing.json", directory.filePath("missing")) && !success && !error.isEmpty(),
               "Missing installation should fail immediately")) return 3;
    if (!check(run(generator, directory.filePath("missing.json"), directory.filePath("invalid")) && !success && !error.isEmpty(),
               "CLI failure should report its error")) return 4;
    QFile request(directory.filePath("request with spaces.json"));
    const QByteArray json = R"({"schema_version":1,"generator":"hunting","width":96,"height":96,"floors":2,"start_z":7,"seed":"dewral-forest","asset_fingerprint":"tilemapgen-cross-language-test","settings":{"spawn_density":0.03}})";
    if (!request.open(QIODevice::WriteOnly) || request.write(json) != json.size()) return 5;
    request.close();
    if (!check(run(generator, request.fileName(), directory.filePath("cancelled"), true) && !success && error.contains("cancel", Qt::CaseInsensitive),
               "Cancel should discard generation")) return 6;
    if (!check(!QFileInfo::exists(directory.filePath("cancelled/map.tmg")), "Cancellation published a result")) return 7;
    const auto output = directory.filePath("output with spaces");
    if (!check(run(generator, request.fileName(), output) && success && progressEvents > 0, "Generation failed: " + error)) return 8;
    QFile catalogFile(fixture + "/catalog.json");
    if (!catalogFile.open(QIODevice::ReadOnly)) return 9;
    const auto pack = QJsonDocument::fromJson(catalogFile.readAll()).object();
    TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
    for (const auto &v : pack["items"].toArray()) {
        const auto i = v.toObject(); catalog.items.insert(i["id"].toInt(), {i["ground"].toBool(), i["blocked"].toBool(), quint32(i["floor_change"].toInteger())});
    }
    for (const auto &v : pack["creatures"].toArray()) catalog.monsters.insert(v.toObject()["name"].toString());
    auto result = TmgFormat::load(output + "/map.tmg", catalog, {}, {}, true);
    if (!check(result.document && result.error.isEmpty(), "Generated map failed DME validation: " + result.error)) return 10;
    OtbmReader destination; destination.newMap(512, 512, 860, 2, 22);
    const auto placement = TmgFormat::placeEmpty(destination, *result.document, 100, 100, 5, 7, 96, 96, 2);
    if (!check(placement.isEmpty() && destination.undoCount() == 1, "Generated placement failed: " + placement)) return 11;
    const int items = destination.itemCount();
    if (!check(items > 0 && destination.undo() && destination.itemCount() == 0
               && destination.redo() && destination.itemCount() == items, "Generated map undo/redo failed")) return 12;
    const auto command = [&](const QStringList &arguments, const QByteArray &input = QByteArray{}) {
        done = success = timedOut = false; error.clear();
        runner.startCommand(generator, arguments, input);
        timeout.start(60000);
        if (!done) loop.exec();
        timeout.stop();
        return done && !timedOut && success;
    };
    if (!check(command({"capabilities"})
               && QJsonDocument::fromJson(runner.output()).object()["generators"].toArray().contains("settlement"),
               "Settlement capability reporting failed: " + error)) return 13;
    if (!check(command({"credentials", "save"}, R"({"key":"stdin-only-test-key","remember":false})")
               && QJsonDocument::fromJson(runner.output()).object()["configured"].toBool()
               && !runner.output().contains("stdin-only-test-key"), "Session credentials must use stdin and stay out of output")) return 14;
    const auto optionsPath = directory.filePath("settlement options.json"), prepared = directory.filePath("settlement recipe.json");
    QFile options(optionsPath);
    if (!options.open(QIODevice::WriteOnly)) return 15;
    options.write(QJsonDocument(QJsonObject{{"width",96},{"height",96},{"z",7},{"seed","dme-settlement"},{"palette",pack["palette"]}}).toJson());
    options.close();
    if (!check(command({"prepare", optionsPath, "--pack", fixture + "/materials.tmgpack", "--output", prepared}),
               "Shared settlement kit binding failed: " + error)) return 16;
    const auto settlementOutput = directory.filePath("settlement output");
    if (!check(run(generator, prepared, settlementOutput) && success, "Settlement v2 CLI generation failed: " + error)) return 17;
    const auto settlementResult = TmgFormat::load(settlementOutput + "/map.tmg", catalog);
    if (!check(settlementResult.document && settlementResult.manifest["schema_version"].toInt() == 2,
               "Settlement v2 CLI import failed: " + settlementResult.error)) return 18;
    qInfo() << "Local generation, cancellation, v2 kit binding, stdin credentials, validated placement and undo/redo passed";
    return 0;
}

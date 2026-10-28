// Diagnostic entry point linked to the production app objects and QRC.
// Offscreen measures UI creation; native mode can await the complete QRhi map frame.
#include "backend.h"
#include "paletteimageprovider.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QResource>
#include <QSurfaceFormat>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QUuid>
#include <QThread>
#include <iostream>
#include <functional>
#include <memory>
#include <QElapsedTimer>

static double ms(const QElapsedTimer &timer) { return timer.nsecsElapsed()/1000000.0; }
static void countObjects(QObject *root,QSet<QObject*> &seen,QJsonObject &types) {
    if (!root||seen.contains(root)) return;
    seen.insert(root);const QString type=QString::fromLatin1(root->metaObject()->className());
    types[type]=types[type].toInt()+1;
    for (auto *child:root->children()) countObjects(child,seen,types);
    if (auto *item=qobject_cast<QQuickItem*>(root))
        for (auto *child:item->childItems()) countObjects(child,seen,types);
    if (auto *window=qobject_cast<QQuickWindow*>(root)) countObjects(window->contentItem(),seen,types);
}
int main(int argc,char **argv) {
    QElapsedTimer clock;clock.start();
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    const bool native = qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_NATIVE");
    if (native) {
        qputenv("QSG_RHI_BACKEND","opengl");
        QSurfaceFormat format=QSurfaceFormat::defaultFormat();
        format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
        format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);format.setSwapInterval(1);
        QSurfaceFormat::setDefaultFormat(format);
    } else QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc,argv);
    const double guiMs=ms(clock);
    const auto args=app.arguments();
    if (args.size()<6) return 2;
    const QString theme=args[2],seed=args[3];
    QCoreApplication::setOrganizationName("DME-StartupAudit");
    const QString name="DME-StartupAudit-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    QCoreApplication::setApplicationName(name);
    QCoreApplication::setApplicationVersion(args[4]);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir fixture(QDir(args[1]).filePath("run-XXXXXX"));
    if (!fixture.isValid()) return 3;
    qputenv("DME_RECOVERY_DIR",(fixture.path()+"/recovery").toUtf8());
    qputenv("DME_UI_STYLE_OVERRIDE",theme.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,fixture.path()+"/settings");
    QSet<QString> dataPaths;
    dataPaths.insert(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath());
    dataPaths.insert(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absolutePath());
    for (const auto &path:dataPaths) if (QFileInfo::exists(path)) return 4;
    const QString data=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (seed!="-") {
        if (!QDir().mkpath(data)||!QFile::copy(seed,data+"/work-timer.json")) return 5;
    }
    // Only this local listener receives traffic; no update/API request leaves
    // the diagnostic process. We measure offline UI startup, not network latency.
    QTcpServer blockedNetwork;
    if (!blockedNetwork.listen(QHostAddress::LocalHost,0)) return 6;
    QObject::connect(&blockedNetwork,&QTcpServer::newConnection,[&] {
        while (auto *socket=blockedNetwork.nextPendingConnection()) {
            socket->disconnectFromHost();socket->deleteLater();
        }
    });
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",blockedNetwork.serverPort()));
    if (args.size()>7 && !QResource::registerResource(args[7])) return 11;
    QElapsedTimer fontClock;fontClock.start();
    int fonts=0;
    for (const auto &file:{"segoeui.ttf","segoeuib.ttf","verdana.ttf","verdanab.ttf"}) {
        const QString path=qEnvironmentVariable("WINDIR")+"/Fonts/"+file;
        if (QFontDatabase::addApplicationFont(path)>=0) ++fonts;
    }
    QGuiApplication::setFont(QFont("Segoe UI",9));
    QJsonObject report{{"theme",theme},{"qguiMs",guiMs},{"mode",native?"native-windows-opengl-qrhi":"offscreen-software-ui"},
                       {"fontSetupMs",ms(fontClock)},{"registeredSystemFonts",fonts}};
    int code=0;
    {
        clock.restart(); Backend backend(nullptr);report["backendMs"]=ms(clock);
        clock.restart();
        {
            QQmlApplicationEngine engine;
            QJsonArray warnings;
            QObject::connect(&engine,&QQmlApplicationEngine::warnings,[&](const QList<QQmlError> &errors) {
                for (const auto &error:errors) warnings.append(error.toString());
            });
            engine.addImageProvider("tibiaui",new UiThemeImageProvider(backend.uiTheme()));
            engine.addImageProvider("paletteitem",new PaletteImageProvider(backend.sprReader()));
            const QUrl entry = args.size()>6 && args[6]!="-"
                ? (args[6].startsWith("qrc:") ? QUrl(args[6]) : QUrl::fromLocalFile(args[6])) : QUrl("qrc:/qml/Main.qml");
            QQmlComponent compiled(&engine);
            compiled.loadUrl(entry, QQmlComponent::PreferSynchronous);
            report["qmlCompileMs"]=ms(clock);
            report["compiledReady"]=compiled.isReady();
            const double compiledMs=ms(clock);
            engine.load(entry);
            report["qmlInstantiateMs"]=ms(clock)-compiledMs;
            report["qmlCreateMs"]=ms(clock);
            if (engine.rootObjects().isEmpty()) code=7;
            else {
                auto firstFrame=std::make_shared<double>(-1);
                for (auto *window:QGuiApplication::allWindows()) if (auto *quick=qobject_cast<QQuickWindow*>(window))
                    QObject::connect(quick,&QQuickWindow::frameSwapped,&app,[firstFrame,&clock] {
                        if (*firstFrame<0) *firstFrame=ms(clock);
                    },Qt::QueuedConnection);
                QElapsedTimer settle;settle.start();
                while (settle.elapsed()<300) { QCoreApplication::processEvents();QThread::msleep(1); }
                report["startupFrameFromQmlBeginMs"]=*firstFrame;
                QSet<QObject*> objects;QJsonObject types;
                for (auto *root:engine.rootObjects()) countObjects(root,objects,types);
                report["createdObjects"]=objects.size();
#ifdef Q_OS_WIN
                PROCESS_MEMORY_COUNTERS memory{};
                if (K32GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory))) {
                    report["uiWorkingSetBytes"]=double(memory.WorkingSetSize);
                    report["uiPeakWorkingSetBytes"]=double(memory.PeakWorkingSetSize);
                }
#endif
                int loaders=0,inactive=0;
                QObject *profiles=nullptr;
                for (auto *object:objects) {
                    const QString type=QString::fromLatin1(object->metaObject()->className());
                    if (type.startsWith("QQuickLoader")) { ++loaders;if (!object->property("active").toBool()) ++inactive; }
                    if (object->property("knownVersions").isValid()) profiles=object;
                }
                report["loaders"]=loaders;report["inactiveLoaders"]=inactive;
                report["objectTypes"]=types;
                if (qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_WELCOME_DIALOGS")) {
                    bool dialogsOk = true;
                    for (auto *object : objects) {
                        const QString name = object->objectName();
                        if (name != "startupNewMapHost" && name != "startupOpenMapHost" && name != "startupFolderHost") continue;
                        QVariant ready;
                        dialogsOk = QMetaObject::invokeMethod(object, "ensureLoaded", Q_RETURN_ARG(QVariant, ready)) && ready.toBool() && dialogsOk;
                        auto *dialog = object->property("item").value<QObject*>();
                        if (!dialog || !dialog->property("startupWindow").value<QObject*>()) { dialogsOk = false; continue; }
                        if (name == "startupNewMapHost") {
                            dialogsOk = QMetaObject::invokeMethod(dialog, "open") && dialogsOk;
                            QElapsedTimer frame; frame.start();
                            while (frame.elapsed() < 100) { app.processEvents(); QThread::msleep(1); }
                            dialogsOk = dialog->property("visible").toBool() && dialogsOk;
                            QMetaObject::invokeMethod(dialog, "close");
                        }
                    }
                    report["welcomeDialogsOk"] = dialogsOk;
                    if (!dialogsOk) code = 17;
                }
                const QString client=args[5];
                if (client!="-") {
                    const double openRequestFromQmlBeginMs = ms(clock);
                    report["openRequestFromQmlBeginMs"] = openRequestFromQmlBeginMs;
                    clock.restart();
                    const QString map = qEnvironmentVariable("DME_STARTUP_AUDIT_MAP");
                    if (!map.isEmpty()) {
                        const bool invoked = QMetaObject::invokeMethod(engine.rootObjects().first(), "profileOpen",
                            Q_ARG(QVariant, map), Q_ARG(QVariant, client), Q_ARG(QVariant, QString("860")));
                        bool presented = false;
                        double maxEventGap = 0;
                        QElapsedTimer eventGap; eventGap.start();
                        while (invoked && clock.elapsed() < 30000) {
                            QCoreApplication::processEvents();
                            maxEventGap = qMax(maxEventGap, ms(eventGap)); eventGap.restart();
                            for (auto *window : QGuiApplication::allWindows())
                                if (window->property("editorPresented").toBool()) presented = true;
                            if (presented) break;
                            QThread::msleep(1);
                        }
                        report["completeMapMs"] = ms(clock);
                        report["completeMapFromQmlBeginMs"] = openRequestFromQmlBeginMs + ms(clock);
                        report["mapPresented"] = presented;
                        report["maxGuiEventGapMs"] = maxEventGap;
                        QSet<QObject*> editorObjects; QJsonObject editorTypes;
                        for (auto *root : engine.rootObjects()) countObjects(root, editorObjects, editorTypes);
                        report["editorObjects"] = editorObjects.size();
                        report["editorTypes"] = editorTypes;
                        if (!invoked || !presented) code = 12;
                        if (presented && qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_CREATURE_ZOOM")) {
                            QObject *overlay = nullptr, *preferencesHost = nullptr;
                            for (auto *object : editorObjects) {
                                if (object->property("creatureNamesMinimumZoom").isValid() && object->property("mapCtrl").isValid()) overlay = object;
                                if (object->property("toolSource").toUrl().path().endsWith("PreferencesDialogTool.qml")) preferencesHost = object;
                            }
                            bool zoomOk = overlay && preferencesHost;
                            const auto settleBindings = [&] {
                                QElapsedTimer deadline; deadline.start();
                                while (deadline.elapsed() < 40) { app.processEvents(); QThread::msleep(1); }
                            };
                            if (zoomOk) {
                                auto *settings = overlay->property("settings").value<QObject*>();
                                auto *view = overlay->property("mapCtrl").value<QObject*>();
                                zoomOk = settings && view;
                                if (zoomOk) {
                                    settings->setProperty("creatureNamesMinimumZoom",25); view->setProperty("showCreatures",true);
                                    view->setProperty("tileSize",2); settleBindings();
                                    zoomOk = !overlay->property("creaturesVisible").toBool() && view->property("showCreatures").toBool();
                                    view->setProperty("tileSize",8); settleBindings();
                                    zoomOk = overlay->property("creaturesVisible").toBool() && zoomOk;
                                    QVariant ready;
                                    zoomOk = QMetaObject::invokeMethod(preferencesHost,"ensureLoaded",Q_RETURN_ARG(QVariant,ready)) && ready.toBool() && zoomOk;
                                    auto *dialog = preferencesHost->property("item").value<QObject*>();
                                    auto *slider = dialog ? dialog->findChild<QObject*>("creatureNamesMinimumZoomSlider") : nullptr;
                                    zoomOk = slider && zoomOk;
                                    if (slider) {
                                        settings->setProperty("creatureNamesMinimumZoom",0);settleBindings();
                                        zoomOk = slider->property("value").toInt() == 0 && zoomOk;
                                        slider->setProperty("value",50);QMetaObject::invokeMethod(slider,"moved");settleBindings();
                                        zoomOk = settings->property("creatureNamesMinimumZoom").toInt() == 50
                                            && !overlay->property("creaturesVisible").toBool() && zoomOk;
                                    }
                                }
                            }
                            report["creatureZoomOk"] = zoomOk;
                            if (!zoomOk) code = 19;
                        }
                        if (presented && qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_DOCUMENTS")) {
                            QObject *controller = nullptr;
                            for (auto *window : QGuiApplication::allWindows())
                                if (window->property("editorPresented").toBool()) controller = window->property("appController").value<QObject*>();
                            auto *original = backend.docMgr()->current();
                            const auto waitUntil = [&](const std::function<bool()> &ready) {
                                QElapsedTimer deadline; deadline.start();
                                do { app.processEvents(); QThread::msleep(1); }
                                while (!ready() && deadline.elapsed() < 15000);
                                return ready();
                            };
                            bool documentsOk = controller && QMetaObject::invokeMethod(controller, "createNewMap",
                                Q_ARG(QVariant, QString("860")), Q_ARG(QVariant, 128), Q_ARG(QVariant, 128));
                            documentsOk = documentsOk && waitUntil([&] {
                                return backend.docMgr()->current() != original && backend.docMgr()->current()->isLoaded() && !backend.clientLoader()->busy();
                            });
                            report["newDocumentPassed"] = documentsOk;
                            report["documentAfterNew"] = backend.docMgr()->current()->filePath();
                            bool feedbackVisible = false;
                            int manualSaves = 0;
                            const auto saveProbe = QObject::connect(backend.docMgr()->current(), &OtbmReader::activitySave,
                                &app, [&](bool automatic, bool) {
                                    if (automatic) return;
                                    ++manualSaves;
                                    QSet<QObject*> feedbackObjects; QJsonObject feedbackTypes;
                                    for (auto *root : engine.rootObjects()) countObjects(root, feedbackObjects, feedbackTypes);
                                    for (auto *object : feedbackObjects)
                                        if (object->objectName() == "saveProgressDialog") feedbackVisible = object->property("visible").toBool();
                                });
                            const QString saved = fixture.filePath("new-map.otbm");
                            if (documentsOk) documentsOk = QMetaObject::invokeMethod(controller, "handleSaveAsAccepted", Q_ARG(QVariant, QUrl::fromLocalFile(saved)))
                                && waitUntil([&] { return backend.docMgr()->current()->filePath() == saved && QFileInfo::exists(saved); });
                            if (documentsOk) documentsOk = QMetaObject::invokeMethod(controller, "saveMap")
                                && waitUntil([&] { return manualSaves == 2 && !controller->property("saving").toBool(); });
                            report["manualSaves"] = manualSaves;
                            report["saveNewDocumentPassed"] = documentsOk;
                            report["documentAfterSave"] = backend.docMgr()->current()->filePath();
                            if (documentsOk) {
                                QVariant accepted;
                                documentsOk = QMetaObject::invokeMethod(controller, "loadEverything", Q_RETURN_ARG(QVariant, accepted),
                                    Q_ARG(QVariant, map), Q_ARG(QVariant, QString("860"))) && accepted.toBool()
                                    && waitUntil([&] { return backend.docMgr()->current() == original && !backend.clientLoader()->busy(); });
                            }
                            QObject::disconnect(saveProbe);
                            report["saveFeedbackVisible"] = feedbackVisible;
                            if (!feedbackVisible) code = 18;
                            report["newSaveSwitchPassed"] = documentsOk;
                            if (!documentsOk) code = 16;
                        }
                        if (presented && qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_TOOLS")) {
                            QJsonArray tools;
                            for (auto *object : editorObjects) {
                                const QUrl source = object->property("toolSource").toUrl();
                                if (!source.path().endsWith("Tool.qml")) continue;
                                bool passed = true;
                                for (int attempt = 0; attempt < 2; ++attempt) {
                                    QVariant ready;
                                    passed = QMetaObject::invokeMethod(object, "ensureLoaded", Q_RETURN_ARG(QVariant, ready)) && ready.toBool() && passed;
                                    auto *item = object->property("item").value<QObject*>();
                                    if (!item) { passed = false; break; }
                                    passed = QMetaObject::invokeMethod(item, "open") && passed;
                                    QElapsedTimer frame; frame.start();
                                    while (frame.elapsed() < 40) { app.processEvents(); QThread::msleep(1); }
                                    passed = item->property("visible").toBool() && passed;
                                    QMetaObject::invokeMethod(item, "close");
                                    app.processEvents(); app.processEvents();
                                }
                                tools.append(QJsonObject{{"source", QFileInfo(source.path()).fileName()}, {"passed", passed}});
                                if (!passed) code = 13;
                            }
                            report["tools"] = tools;
                            if (tools.size() != 30) code = 14;
                        }
                        if (presented && qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_TIMER")) {
                            bool opened = false;
                            for (auto *object : editorObjects) {
                                auto *item = qobject_cast<QQuickItem*>(object);
                                if (item && QString::fromLatin1(object->metaObject()->className()).startsWith("WorkTimerStatus")
                                    && item->isVisible() && item->window() && item->window()->isVisible()) {
                                    opened = QMetaObject::invokeMethod(object, "openPanel"); break;
                                }
                            }
                            QSet<QObject*> timerObjects; QJsonObject timerTypes;
                            for (auto *root : engine.rootObjects()) countObjects(root, timerObjects, timerTypes);
                            for (auto *object : timerObjects)
                                if (object->objectName() == "workTimerHistory") QMetaObject::invokeMethod(object, "clicked");
                            QElapsedTimer historyFrame; historyFrame.start();
                            while (historyFrame.elapsed() < 100) { app.processEvents(); QThread::msleep(1); }
                            timerObjects.clear(); timerTypes = {};
                            for (auto *root : engine.rootObjects()) countObjects(root, timerObjects, timerTypes);
                            int rows = -1, contents = 0;
                            for (auto *object : timerObjects) {
                                if (object->objectName() == "workTimerHistoryList") rows = object->property("count").toInt();
                                if (object->objectName() == "workTimerContent") ++contents;
                            }
                            report["timerHistoryRows"] = rows; report["timerPopups"] = contents;
                            if (!opened || contents != 1 || rows != backend.workTimer()->historyModel()->rowCount()) code = 15;
                        }
                        if (presented && qEnvironmentVariableIsSet("DME_STARTUP_AUDIT_SCREENSHOT")) {
                            for (auto *window : QGuiApplication::allWindows())
                                if (auto *quick = qobject_cast<QQuickWindow*>(window); quick && window->property("editorPresented").toBool())
                                    quick->grabWindow().save(qEnvironmentVariable("DME_STARTUP_AUDIT_SCREENSHOT"));
                        }
                    } else {
                        bool done = false, ok = false;
                        QObject::connect(backend.clientLoader(), &ClientLoadService::finished, &app,
                            [&](quint64, const QString &, const QString &, bool success, const QString &) {
                                done = true; ok = success;
                            });
                        backend.clientLoader()->request("1098", 1098, client, false);
                        while (!done && clock.elapsed() < 30000) {
                            QCoreApplication::processEvents(); QThread::msleep(1);
                        }
                        report["clientLoadMs"] = ms(clock); report["clientOk"] = done && ok;
                        if (!done || !ok) code = 9;
                    }
                    report["clientDefinitions"] = backend.datReader()->itemCount();
                }
            }
            report["warningCount"]=warnings.size();report["warnings"]=warnings;
            clock.restart();
        }
        report["engineTeardownMs"]=ms(clock);
        backend.docMgr()->markCleanShutdown();clock.restart();
    }
    report["backendTeardownMs"]=ms(clock);
    bool cleaned=true;
    for (const auto &path:dataPaths) {
        const QFileInfo info(path);
        // These exact, freshly-created UUID directories are the only external
        // paths owned by this run. Never recursively remove their parents.
        if (info.fileName()!=name||info.isSymLink()) { cleaned=false;continue; }
        if (info.exists()) cleaned=QDir(info.absoluteFilePath()).removeRecursively()&&cleaned;
    }
    report["testDataCleaned"]=cleaned;
    std::cout<<QJsonDocument(report).toJson().toStdString();
    return cleaned?code:10;
}

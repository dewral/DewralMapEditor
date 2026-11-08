#include <QMouseEvent>
#include <QKeyEvent>
#include <QImage>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QUuid>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTimer>

#include <cstdio>

#include "backend.h"
#include "dmewindow.h"
#include "mapview.h"
#include "maprhiview.h"
#include "minimapview.h"
#include "palettefilter.h"
#include "paletteimageprovider.h"

namespace {
void profilePalette(QObject *root, Backend *backend, QGuiApplication *app, bool exitAfter)
{
    struct ScrollMeasurement {
        QElapsedTimer clock;
        qint64 previous = 0, maxGap = 0;
        int page = 0;
    };
    auto measurement = std::make_shared<ScrollMeasurement>();
    auto *timer = new QTimer(app);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(16);
    measurement->clock.start();
    LoadProfile::record(QStringLiteral("palette_scroll_begin"));
    QMetaObject::invokeMethod(root, "profilePalettePage", Q_ARG(QVariant, 0));
    QObject::connect(timer, &QTimer::timeout, root, [=] {
        const qint64 now = measurement->clock.elapsed();
        measurement->maxGap = qMax(measurement->maxGap, now - measurement->previous);
        measurement->previous = now;
        if (measurement->page < 4 && now >= (measurement->page + 1) * 500) {
            ++measurement->page;
            QMetaObject::invokeMethod(root, "profilePalettePage", Q_ARG(QVariant, measurement->page * 200));
        }
        if (now < 3000) return;
        timer->stop();
        int ready = 0, pending = 0, errors = 0;
        // GridView delegates have visual parents independent of QObject ownership.
        auto *window = qobject_cast<QQuickWindow *>(root);
        QList<QQuickItem *> items;
        if (window) items.append(window->contentItem());
        while (!items.isEmpty()) {
            auto *item = items.takeLast();
            items.append(item->childItems());
            if (item->objectName() != QStringLiteral("paletteItemImage") || !item->isVisible()) continue;
            const int status = item->property("status").toInt();
            ready += status == 1; pending += status == 2; errors += status == 3;
        }
        LoadProfile::record(QStringLiteral("palette_scroll_complete"), now,
            QStringLiteral("max_gui_gap_ms=%1 ready=%2 pending=%3 errors=%4 thumbnail_cache_bytes=%5")
                .arg(measurement->maxGap).arg(ready).arg(pending).arg(errors)
                .arg(backend->sprReader()->thumbnailCacheBytes()));
        timer->deleteLater();
        if (exitAfter) app->exit(ready > 0 && pending == 0 && errors == 0 ? 0 : 3);
    });
    timer->start();
}
}

int main(int argc, char *argv[])
{
    bool profile = false;
    for (int i = 1; i < argc; ++i)
        if (QByteArray(argv[i]) == "--load-profile") profile = true;
    LoadProfile::start(profile);
    LoadProfile::record(QStringLiteral("process_start"));
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));

    QSettings startupSettings(QSettings::NativeFormat, QSettings::UserScope,
                              QStringLiteral("Dewral"),
                              QStringLiteral("DewralMapEditor"));
    const bool vsyncEnabled =
        startupSettings.value(QStringLiteral("vsyncEnabled"), true).toBool();

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
#else
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
#endif
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(vsyncEnabled ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    // D3D11 can lose the device when the first map frame is presented on
    // Windows, leaving the entire Qt Quick window black. Use the OpenGL QRhi
    // backend by default, while preserving explicit backend overrides.
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("opengl"));
#endif
    // VSync applies to the swapchain and takes effect at application startup.
    if (!vsyncEnabled) qputenv("QSG_NO_VSYNC", QByteArrayLiteral("1"));

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/ui/app-icon.png")));

    QCoreApplication::setOrganizationName(QStringLiteral("Dewral"));
    QCoreApplication::setApplicationName(QStringLiteral("DewralMapEditor"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DME_VERSION));
    // Automated timing runs use separate settings and session state.
    if (profile && app.arguments().contains(QStringLiteral("--profile-map"))) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                          QCoreApplication::applicationDirPath() + QStringLiteral("/profile-settings"));
        QCoreApplication::setApplicationName(QStringLiteral("DME-LoadProfile"));
    }

    // Smoke tests must never load or save the user's profiles, timer or recovery files.
    std::unique_ptr<QTemporaryDir> smokeState;
    if (app.arguments().contains(QStringLiteral("--smoke-test"))) {
        smokeState = std::make_unique<QTemporaryDir>();
        if (!smokeState->isValid()) return 6;
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("DME-Smoke-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, smokeState->path());
        qputenv("DME_RECOVERY_DIR", smokeState->filePath(QStringLiteral("recovery")).toUtf8());
        const QString ownedData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString ownedLocalData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString ownedName = QCoreApplication::applicationName();
        QObject::connect(&app, &QObject::destroyed, [ownedData, ownedLocalData, ownedName] {
            for (const auto &path : {ownedData, ownedLocalData}) {
                const QFileInfo info(path);
                if (info.isAbsolute() && info.fileName() == ownedName && !info.isSymLink())
                    QDir(info.absoluteFilePath()).removeRecursively();
            }
        });
    }

    Backend backend(nullptr);
    LoadProfile::record(QStringLiteral("backend_ready"));
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     backend.docMgr(), &DocumentManager::markCleanShutdown);

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings) {
            const QByteArray message = warning.toString().toLocal8Bit();
            std::fprintf(stderr, "%s\n", message.constData());
        }
        std::fflush(stderr);
    });

    engine.addImageProvider(QStringLiteral("tibiaui"),
                            new UiThemeImageProvider());
    engine.addImageProvider(QStringLiteral("paletteitem"),
                            new PaletteImageProvider(backend.sprReader()));

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    {
        LoadProfile::Scope timing(QStringLiteral("qml_startup"));
        engine.load(url);
    }

    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "DME: QML engine did not create a root window.\n");
        std::fflush(stderr);
        return -1;
    }

    if (profile) {
        const QStringList args = app.arguments();
        auto option = [&args](const QString &name) {
            const int index = args.indexOf(name);
            return index >= 0 && index + 1 < args.size() ? args[index + 1] : QString();
        };
        const QString path = option(QStringLiteral("--profile-map"));
        const QString folder = option(QStringLiteral("--profile-client"));
        const QString version = option(QStringLiteral("--profile-version"));
        if (!path.isEmpty() && !folder.isEmpty() && !version.isEmpty()) {
            QObject *root = engine.rootObjects().first();
            QMetaObject::invokeMethod(root, "prepareProfileEditor");
            const int repeats = qMax(1, option(QStringLiteral("--profile-repeat")).toInt());
            if (args.contains(QStringLiteral("--profile-exit")) || repeats > 1
                    || args.contains(QStringLiteral("--profile-scroll"))) {
                auto remaining = std::make_shared<int>(repeats);
                if (auto *view = root->findChild<MapView *>())
                    QObject::connect(view, &MapView::initialViewReady, &app,
                        [&app, &backend, root, path, folder, version, remaining, args] {
                        if (--*remaining > 0) {
                            QTimer::singleShot(0, root, [root, path, folder, version] {
                                LoadProfile::record(QStringLiteral("repeat_map_begin"));
                                QMetaObject::invokeMethod(root, "profileReload", Q_ARG(QVariant, path),
                                    Q_ARG(QVariant, folder), Q_ARG(QVariant, version));
                            });
                        } else if (args.contains(QStringLiteral("--profile-scroll"))) {
                            QTimer::singleShot(0, root, [&app, &backend, root, args] {
                                profilePalette(root, &backend, &app, args.contains(QStringLiteral("--profile-exit")));
                            });
                        } else if (args.contains(QStringLiteral("--profile-exit"))) {
                            QTimer::singleShot(0, &app, [&app] {
                                LoadProfile::record(QStringLiteral("profile_complete"));
                                app.exit(0);
                            });
                        }
                    });
                if (args.contains(QStringLiteral("--profile-exit"))) QTimer::singleShot(60000, &app, [&app] {
                    LoadProfile::record(QStringLiteral("readiness_timeout"));
                    app.exit(2);
                });
            }
            QTimer::singleShot(0, root, [root, path, folder, version] {
                QMetaObject::invokeMethod(root, "profileOpen", Q_ARG(QVariant, path),
                    Q_ARG(QVariant, folder), Q_ARG(QVariant, version));
            });
        }
    }

    if (app.arguments().contains(QStringLiteral("--smoke-test"))) {
        const int shutdownOption = app.arguments().indexOf(QStringLiteral("--smoke-shutdown"));
        const bool shutdownSmoke = shutdownOption >= 0;
        if (shutdownSmoke) {
            const QString mode = app.arguments().value(shutdownOption + 1);
            if (mode != QStringLiteral("startup") && mode != QStringLiteral("editor")
                && mode != QStringLiteral("native") && mode != QStringLiteral("hidden")) return 11;
            QVariant ready;
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "prepareProfileEditor",
                                          Q_RETURN_ARG(QVariant, ready)) || !ready.toBool()) return 11;
            DmeWindow *editor = nullptr;
            for (auto *window : QGuiApplication::allWindows())
                if (auto *candidate = qobject_cast<DmeWindow *>(window)) editor = candidate;
            if (!editor) return 11;
            auto *controller = editor->property("appController").value<QObject *>();
            auto *settings = editor->property("prefs").value<QObject *>();
            if (!controller || !settings) return 11;
            controller->setProperty("started", true);
            editor->setProperty("editorPresented", true);
            // The profile manager retains a native window after being hidden,
            // and rejects close events. QGuiApplication::quit can be vetoed by it.
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "smokeClientProfiles",
                    Q_RETURN_ARG(QVariant, ready), Q_ARG(QVariant, QVariant(QString())),
                    Q_ARG(QVariant, QVariant(true))) || !ready.toBool()) return 11;
            QWindow *profiles = nullptr;
            for (auto *window : QGuiApplication::allWindows())
                if (window->objectName() == QStringLiteral("clientProfilesWindow")) profiles = window;
            if (!profiles || !profiles->isVisible()) return 11;
            profiles->hide();
            // A real independent preview must not keep the editor process alive.
            settings->setProperty("ingamePreviewDockSide", QStringLiteral("window"));
            settings->setProperty("showIngamePreviewWindow", mode != QStringLiteral("hidden"));
            if (mode == QStringLiteral("startup")) {
                controller->setProperty("started", false);
                editor->setProperty("editorPresented", false);
            }
            QTimer::singleShot(500, &app, [editor, controller, mode, &app] {
                bool previewVisible = false;
                for (auto *window : QGuiApplication::allWindows())
                    if (window->title() == QStringLiteral("In-game Preview - Dewral Map Editor"))
                        previewVisible = window->isVisible() && !window->transientParent();
                if (previewVisible != (mode != QStringLiteral("hidden"))) { app.exit(11); return; }
                std::fprintf(stderr, "SHUTDOWN_CLOSE_REQUESTED=%s\n", qPrintable(mode));
                std::fflush(stderr);
                if (mode == QStringLiteral("startup")) {
                    auto *startup = qobject_cast<QWindow *>(controller->property("startupWindow").value<QObject *>());
                    if (!startup || !startup->isVisible()) { app.exit(11); return; }
                    startup->close();
                } else if (mode == QStringLiteral("native")) editor->close();
                else QMetaObject::invokeMethod(controller, "requestAppClose");
            });
        }
        const bool weatherSmoke=app.arguments().contains(QStringLiteral("--smoke-weather-regions"));
        if(weatherSmoke) {
            QVariant ready;
            if(!QMetaObject::invokeMethod(engine.rootObjects().first(),"smokeWeatherRegions",
                Q_RETURN_ARG(QVariant,ready),Q_ARG(QVariant,smokeState->filePath(QStringLiteral("weather-map.otbm")))) || !ready.toBool()) return 7;
        }
        if (app.arguments().contains(QStringLiteral("--smoke-search-results"))) {
            QVariant ready;
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "smokeSearchResults",
                                          Q_RETURN_ARG(QVariant, ready)) || !ready.toBool()) return 7;
        }
        if (app.arguments().contains(QStringLiteral("--smoke-map-brush"))) {
            QVariant ready;
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "smokeMapBrush",
                                          Q_RETURN_ARG(QVariant, ready)) || !ready.toBool()) return 4;
        }
        if (app.arguments().contains(QStringLiteral("--smoke-generation-library"))) {
            QVariant ready;
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "smokeGenerationLibrary",
                                          Q_RETURN_ARG(QVariant, ready)) || !ready.toBool()) return 3;
        }
        if (app.arguments().contains(QStringLiteral("--smoke-tilemapgen"))) {
            QVariant ready;
            if (!QMetaObject::invokeMethod(engine.rootObjects().first(), "smokeTileMapGen",
                                          Q_RETURN_ARG(QVariant, ready)) || !ready.toBool())
                return 2;
        }
        const bool profilesSmoke=app.arguments().contains(QStringLiteral("--smoke-client-profiles"));
        if(profilesSmoke) {
            const int option=app.arguments().indexOf(QStringLiteral("--document-client"));
            const QString folder=option>=0 && option+1<app.arguments().size() ? app.arguments()[option+1] : QString();
            auto *root=engine.rootObjects().first();
            QMetaObject::invokeMethod(root,"smokeClientProfiles",Q_ARG(QVariant,QVariant(folder)),Q_ARG(QVariant,QVariant(false)));
            QTimer::singleShot(500,&app,[root,&app]{
                for(auto *w:QGuiApplication::allWindows())if(auto *q=qobject_cast<QQuickWindow*>(w))if(q->isVisible())q->grabWindow().save(QCoreApplication::applicationDirPath()+"/startup-native.png");
                QMetaObject::invokeMethod(root,"smokeClientProfiles",Q_ARG(QVariant,QVariant(QString())),Q_ARG(QVariant,QVariant(true)));
                QTimer::singleShot(500,&app,[&app]{
                    bool found=false;
                    for(auto *w:QGuiApplication::allWindows())if(auto *q=qobject_cast<QQuickWindow*>(w))if(q->title()=="Client profiles" && q->isVisible()){found=q->grabWindow().save(QCoreApplication::applicationDirPath()+"/profiles-native.png");}
                    app.exit(found ? 0 : 10);
                });
            });
        }
        const bool documentSmoke=app.arguments().contains(QStringLiteral("--smoke-documents"));
        if(documentSmoke) {
            const auto smokeOption=[&app](const QString &name) {
                const int index=app.arguments().indexOf(name);
                return index>=0 && index+1<app.arguments().size() ? app.arguments()[index+1] : QString();
            };
            const QString mapPath=smokeOption(QStringLiteral("--document-map"));
            if(!mapPath.isEmpty()) QMetaObject::invokeMethod(engine.rootObjects().first(),"profileOpen",
                Q_ARG(QVariant,QVariant(mapPath)),Q_ARG(QVariant,QVariant(smokeOption(QStringLiteral("--document-client")))) ,
                Q_ARG(QVariant,QVariant(smokeOption(QStringLiteral("--document-version")))));
            const QString secondPath=mapPath.isEmpty() ? QString() : smokeState->filePath(QStringLiteral("second.otbm"));
            const QString secondSource=smokeOption(QStringLiteral("--document-second"));
            if(!mapPath.isEmpty() && !QFile::copy(secondSource.isEmpty() ? mapPath : secondSource,secondPath))return 9;
            auto *timer=new QTimer(&app);
            auto step=std::make_shared<int>(0);
            QObject::connect(timer,&QTimer::timeout,&app,[&engine,&app,&backend,step,mapPath,secondPath] {
                if(backend.otbmReader()->isLoading() || backend.clientLoader()->busy()
                    || (!mapPath.isEmpty() && *step==0 && !backend.otbmReader()->isLoaded()))return;
                std::fprintf(stderr,"DOCUMENT_STEP=%d\n",*step);
                std::fflush(stderr);
                QVariant ok;
                if(!QMetaObject::invokeMethod(engine.rootObjects().first(),"smokeDocumentStep",
                    Q_RETURN_ARG(QVariant,ok),Q_ARG(QVariant,QVariant(*step)),Q_ARG(QVariant,QVariant(secondPath))) || !ok.toBool()) {
                    app.exit(9);return;
                }
                if((*step)++==8) app.exit(0);
            });
            timer->start(300);
        }
        for (auto *window : QGuiApplication::allWindows())
            if (auto *quick = qobject_cast<QQuickWindow*>(window)) {
                if(weatherSmoke) {
                    if(!quick->property("editorPresented").isValid())continue;
                    auto *root=engine.rootObjects().first();
                    auto captured=std::make_shared<bool>(false);
                    QObject::connect(quick,&QQuickWindow::frameSwapped,&app,[quick,captured,root,&app] {
                        if(*captured)return;*captured=true;
                        QTimer::singleShot(250,&app,[quick,root,&app]{
                            QVariant position;
                            if(!QMetaObject::invokeMethod(root,"prepareWeatherInput",Q_RETURN_ARG(QVariant,position))){app.exit(8);return;}
                            // Let popup closing and the map's camera update reach the scene graph.
                            QTimer::singleShot(250,&app,[quick,root,&app]{
                            QVariant position;
                            if(!QMetaObject::invokeMethod(root,"prepareWeatherInput",Q_RETURN_ARG(QVariant,position))){app.exit(8);return;}
                            const auto coordinates=position.toMap();
                            const QPointF point(coordinates.value("x").toDouble(),coordinates.value("y").toDouble());
                            const QPointF global=quick->mapToGlobal(point.toPoint());
                            QMouseEvent press(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                            QCoreApplication::sendEvent(quick,&press);
                            QMouseEvent release(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                            QCoreApplication::sendEvent(quick,&release);
                            auto verify=[root](int phase){QVariant valid;return QMetaObject::invokeMethod(root,"verifyWeatherInput",Q_RETURN_ARG(QVariant,valid),Q_ARG(QVariant,phase)) && valid.toBool();};
                            if(!verify(0)){const int option=app.arguments().indexOf(QStringLiteral("--weather-screenshot"));if(option>=0 && option+1<app.arguments().size())quick->grabWindow().save(app.arguments()[option+1]);std::fprintf(stderr,"Weather brush input failed\n");app.exit(8);return;}
                            QKeyEvent undo(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier);QCoreApplication::sendEvent(quick,&undo);
                            if(!verify(1)){std::fprintf(stderr,"Weather undo input failed\n");app.exit(8);return;}
                            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(quick,&escape);
                            if(!verify(2)){std::fprintf(stderr,"Weather Escape input failed\n");app.exit(8);return;}
                            QTimer::singleShot(1400,&app,[quick,root,&app]{
                            QVariant timeoutValid;
                            if(!QMetaObject::invokeMethod(root,"verifyWeatherTimeout",Q_RETURN_ARG(QVariant,timeoutValid)) || !timeoutValid.toBool()){std::fprintf(stderr,"Weather test timeout failed\n");app.exit(8);return;}
                            const int option=app.arguments().indexOf(QStringLiteral("--weather-screenshot"));
                            if(option>=0 && option+1<app.arguments().size())quick->grabWindow().save(app.arguments()[option+1]);
                            // The normal editor close handler may open a confirmation dialog.
                            // This test owns only a temporary map and must terminate directly.
                            app.exit(0);
                            });
                            });
                        });
                    },Qt::QueuedConnection);
                } else if(!documentSmoke && !profilesSmoke && !shutdownSmoke) QObject::connect(quick,&QQuickWindow::frameSwapped,&app,&QCoreApplication::quit,Qt::QueuedConnection);
            }
        QTimer::singleShot(documentSmoke ? 60000 : 10000, &app, [] { QCoreApplication::exit(5); });
    }

    return app.exec();
}

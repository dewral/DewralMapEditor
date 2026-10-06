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

    // QRhi chooses the platform backend. QSG_RHI_BACKEND can override it.
    // VSync applies to the swapchain and takes effect at application startup.
    if (!vsyncEnabled) qputenv("QSG_NO_VSYNC", QByteArrayLiteral("1"));

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/ui/github/app-icon.png")));

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
                            new UiThemeImageProvider(backend.uiTheme()));
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

    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        QTimer::singleShot(0, &app, &QCoreApplication::quit);

    return app.exec();
}

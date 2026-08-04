#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSurfaceFormat>

#include <cstdio>

#include "backend.h"
#include "mapview.h"
#include "mapglview.h"
#include "minimapview.h"
#include "palettefilter.h"
#include "paletteimageprovider.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Basic"));

    QSettings startupSettings(QSettings::NativeFormat, QSettings::UserScope,
                              QStringLiteral("Dewral"),
                              QStringLiteral("DewralMapEditor"));
    const bool vsyncEnabled =
        startupSettings.value(QStringLiteral("vsyncEnabled"), true).toBool();

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(vsyncEnabled ? 1 : 0);
    QSurfaceFormat::setDefaultFormat(format);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/ui/github/app-icon.png")));

    QCoreApplication::setOrganizationName(QStringLiteral("Dewral"));
    QCoreApplication::setApplicationName(QStringLiteral("DewralMapEditor"));

    Backend backend(nullptr);
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
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "DME: QML engine did not create a root window.\n");
        std::fflush(stderr);
        return -1;
    }

    return app.exec();
}

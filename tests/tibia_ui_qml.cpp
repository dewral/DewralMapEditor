#include "uitheme.h"
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

class TibiaUiSetup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        auto *theme=new UiTheme(engine);
        engine->addImageProvider(QStringLiteral("tibiaui"),new UiThemeImageProvider(theme));
    }
};
QUICK_TEST_MAIN_WITH_SETUP(tibia_ui, TibiaUiSetup)
#include "tibia_ui_qml.moc"

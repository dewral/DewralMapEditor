#include "uitheme.h"
#include "uiscale.h"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

static void expect(bool condition, const char *message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir settingsDir(QDir::currentPath() + "/ui-colorize-test-XXXXXX");
    expect(settingsDir.isValid(), "Cannot create isolated settings directory");
    QCoreApplication::setOrganizationName("DMEColorizeTest");
    QCoreApplication::setApplicationName("UIColors");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    qunsetenv("DME_UI_STYLE_OVERRIDE");
    QSettings().setValue("ui/style", "gray-dark");
    UiTheme theme;
    expect(theme.style() == "tibia-ui", "A retired saved theme must fall back to Tibia-UI");
    const auto styles = theme.styles();
    expect(styles.size() == 2, "Only the two supported themes must be listed");
    expect(styles.at(0).toMap().value("id") == "fluent-dark"
        && styles.at(1).toMap().value("id") == "tibia-ui", "Theme selector must list Fluent Dark and Tibia-UI");
    int changes = 0;
    QObject::connect(&theme, &UiTheme::colorsChanged, [&] { ++changes; });
    expect(!theme.setUiColor("selected", QColor("invalid")), "Invalid colors must be rejected");
    expect(theme.setUiColor("selected", QColor("#355066")), "Valid colors must be accepted");
    expect(changes == 1, "Color edits must notify live bindings");
    theme.setUiColor("selected", QColor("#355066"));
    expect(changes == 1, "Unchanged colors must not emit changes");
    UiTheme reopened;
    expect(QColor(reopened.colorOverrides().value("selected").toString()) == QColor("#355066"), "Colors must survive reopening");
    theme.setUiColor("lightingOn", QColor("#123456"));
    theme.resetUiColor("lightingOn");
    expect(!theme.colorOverrides().contains("lightingOn") && theme.colorOverrides().contains("selected"), "Reset must affect only the requested role");
    int highlights = 0;
    QObject::connect(&theme, &UiTheme::highlightedColorChanged, [&] { ++highlights; });
    theme.setHighlightedColor("selected"); theme.setHighlightedColor("selected");
    expect(highlights == 2, "Clicking the same role must restart its highlight");
    theme.resetUiColors();
    UiTheme reset;
    expect(reset.colorOverrides().isEmpty(), "Reset all must clear persisted overrides");
    bool listed=false;
    for (const auto &entry:theme.styles()) {
        const auto item=entry.toMap();
        if(item.value("id")=="tibia-ui") listed=item.value("name")=="Tibia-UI";
    }
    expect(listed,"Tibia-UI must be available in the theme selector");
    theme.setStyle("tibia-ui");
    expect(theme.style()=="tibia-ui","Tibia-UI must be accepted at runtime");
    UiTheme savedTheme;
    expect(savedTheme.style()=="tibia-ui","Tibia-UI must survive restarting the editor");
    theme.setStyle("fluent-dark");
    expect(theme.style()=="fluent-dark","Switching back to Fluent must remain supported");
    theme.setStyle("gray-dark");
    expect(theme.style() == "fluent-dark", "A retired theme must not replace the active style");
    qputenv("DME_UI_STYLE_OVERRIDE", "gray-dark");
    UiTheme ignoredOverride;
    expect(ignoredOverride.style() == "fluent-dark", "A retired override must preserve a supported saved theme");
    int scaleChanges = 0;
    QObject::connect(&theme, &UiTheme::uiScaleChanged, [&] { ++scaleChanges; });
    expect(theme.uiScalePercent() == 100 && !theme.uiScaleRestartRequired(), "UI scale must default to 100%");
    theme.setUiScalePercent(80);
    expect(theme.uiScalePercent() == 80 && theme.activeUiScalePercent() == 100
        && theme.uiScaleRestartRequired() && scaleChanges == 1, "Scaling must persist a pending restart without changing active UI");
    theme.setUiScalePercent(80);
    theme.setUiScalePercent(0);
    theme.setUiScalePercent(999);
    expect(scaleChanges == 1 && theme.uiScalePercent() == 80, "Invalid or unchanged scales must not notify or overwrite preferences");
    app.setProperty("dmeUiScalePercent", 80);
    UiTheme scaledRestart;
    expect(scaledRestart.uiScalePercent() == 80 && !scaledRestart.uiScaleRestartRequired(), "UI scale must survive restart and clear the notice");
    scaledRestart.setUiScalePercent(100);
    expect(scaledRestart.uiScaleRestartRequired(), "Restoring 100% must request restart too");
    QSettings().setValue("ui/scalePercent", -10);
    UiTheme invalidScale;
    expect(invalidScale.uiScalePercent() == 100, "Corrupt UI scales must fall back safely");
    qunsetenv("DME_UI_SCALE_PERCENT");
    qunsetenv("DME_QT_BASE_SCALE_FACTOR");
    qputenv("QT_SCALE_FACTOR", "1.5");
    expect(UiScale::configure(80, false) == 80 && qgetenv("QT_SCALE_FACTOR") == "1.2", "UI scale must compose with an external Qt factor");
    expect(UiScale::configure(80, false) == 80 && qgetenv("QT_SCALE_FACTOR") == "1.2", "Updater/restart inheritance must not compound scaling");
    expect(UiScale::configure(125, false) == 125 && qgetenv("QT_SCALE_FACTOR") == "1.875", "A later restart must use the newly selected percentage");
    expect(UiScale::configure(80, true) == 100, "Isolated smoke tests must not inherit user preferences");
    qputenv("DME_UI_SCALE_PERCENT", "75");
    expect(UiScale::configure(100, true) == 75, "Smoke validation must support an explicit scale override");
    qunsetenv("DME_UI_SCALE_PERCENT");
    qunsetenv("DME_QT_BASE_SCALE_FACTOR");
    qunsetenv("QT_SCALE_FACTOR");
    return EXIT_SUCCESS;
}

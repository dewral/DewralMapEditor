#include "uitheme.h"
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
    UiTheme theme;
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
    return EXIT_SUCCESS;
}

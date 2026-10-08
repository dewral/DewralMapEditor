#include "hotkeysettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QSet>
#include <cstdlib>
#include <iostream>

static void expect(bool condition, const char *message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir settingsDir(QDir::currentPath() + "/hotkey-test-XXXXXX");
    expect(settingsDir.isValid(), "Cannot isolate hotkey preferences");
    QCoreApplication::setOrganizationName("DMEHotkeyTest");
    QCoreApplication::setApplicationName("Hotkeys");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    HotkeySettings hotkeys;
    const auto defaults = hotkeys.bindings();
    expect(defaults.value("go_to_position") == "Ctrl+G", "Keep the Go to Position default");
    expect(defaults.value("redo_alternative") == "Ctrl+Y", "Keep alternative bindings");
    expect(defaults.value("floor_up") == "+", "Keep map input defaults");
    expect(defaults.value("preview_left") == "A", "Keep preview walking defaults");
    QSet<QString> ids;
    for (const auto &value : hotkeys.commands()) {
        const auto command = value.toMap();
        const auto id = command.value("id").toString();
        expect(!ids.contains(id), "Each command must have one catalog entry");
        expect(!command.value("label").toString().isEmpty(), "Every command needs a label");
        ids.insert(id);
    }
    expect(ids.size() == defaults.size(), "Catalog and dispatch must agree");

    int changes = 0;
    QObject::connect(&hotkeys, &HotkeySettings::bindingsChanged, [&] { ++changes; });
    expect(hotkeys.setShortcut("go_to_position", "Ctrl+Alt+G").isEmpty(), "Rebind Go to Position");
    expect(changes == 1, "Rebinding must update menus immediately");
    expect(hotkeys.matches("go_to_position", Qt::Key_G, Qt::ControlModifier | Qt::AltModifier),
           "The new key combination must match");
    expect(!hotkeys.matches("go_to_position", Qt::Key_G, Qt::ControlModifier),
           "The former binding must stop matching");
    HotkeySettings reopened;
    expect(reopened.bindings().value("go_to_position") == "Ctrl+Alt+G", "Persist the edited shortcut");
    expect(reopened.bindings().value("save") == "Ctrl+S", "Preserve unrelated shortcuts");
    expect(!hotkeys.setShortcut("go_to_position", "Ctrl+S").isEmpty(), "Reject command conflicts");
    expect(!hotkeys.setShortcut("go_to_position", "Left").isEmpty(), "Protect fixed navigation keys");
    expect(!hotkeys.setShortcut("go_to_position", "Ctrl+K, Ctrl+G").isEmpty(), "Reject multichord input");
    expect(!hotkeys.setShortcut("go_to_position", "NotAKey").isEmpty(), "Reject invalid input");
    expect(!hotkeys.setShortcut("go_to_position", "Ctrl").isEmpty(), "Reject modifier-only input");
    expect(!hotkeys.setShortcut("move_left", "F6").isEmpty(), "Fixed controls cannot be rebound");
    expect(changes == 1, "Failed edits must leave bindings unchanged");
    expect(hotkeys.shortcutFromKey(Qt::Key_Control, Qt::ControlModifier).isEmpty(),
           "Wait for a non-modifier key when recording");
    expect(hotkeys.shortcutFromKey(Qt::Key_Plus, Qt::ControlModifier | Qt::ShiftModifier) == "Ctrl++",
           "Normalize plus without a redundant Shift modifier");
    expect(hotkeys.shortcutFromKey(Qt::Key_G, Qt::ControlModifier | Qt::KeypadModifier) == "Ctrl+G",
           "Ignore keypad modifier when recording");

    expect(hotkeys.setShortcut("preview_left", "F").isEmpty(), "Preview and editor keys have separate scopes");
    hotkeys.setPreviewActive(true);
    expect(hotkeys.activeBindings().value("show_creatures").toString().isEmpty(), "Walking takes precedence over an overlapping editor key");
    expect(hotkeys.activeBindings().value("go_to_position") == "Ctrl+Alt+G", "Unrelated editor commands remain available during preview");
    hotkeys.setPreviewActive(false);
    expect(hotkeys.activeBindings().value("show_creatures") == "F", "Editor keys resume when preview closes");
    expect(!hotkeys.setShortcut("preview_left", "Ctrl+S").isEmpty(), "Window commands must work in all scopes");
    hotkeys.setCapturing(true);
    expect(hotkeys.activeBindings().value("save").toString().isEmpty(), "Recording suspends window shortcuts too");
    expect(!hotkeys.matches("preview_left", Qt::Key_F, 0), "Capture must suspend command dispatch");
    hotkeys.setCapturing(false);
    expect(hotkeys.matches("preview_left", Qt::Key_F, 0), "Dispatch must resume after capture");

    expect(hotkeys.setShortcut("go_to_position", "").isEmpty(), "Support clearing a binding");
    HotkeySettings cleared;
    expect(cleared.bindings().value("go_to_position").toString().isEmpty(), "Persist explicit unbinding");
    expect(!cleared.matches("go_to_position", Qt::Key_G, Qt::ControlModifier), "Unbound commands must not dispatch");
    expect(hotkeys.setShortcut("preferences", "Ctrl+G").isEmpty(), "A freed key can be reassigned");
    expect(!hotkeys.resetShortcut("go_to_position").isEmpty(), "Reset must report a custom-binding conflict");
    expect(hotkeys.resetShortcut("preferences").isEmpty(), "Reset just one command");
    expect(hotkeys.resetShortcut("go_to_position").isEmpty(), "Restore Go to Position");
    expect(hotkeys.bindings().value("preview_left") == "F", "Individual reset leaves other overrides intact");
    expect(hotkeys.setShortcut("borderize_selection", "Ctrl+Alt+B").isEmpty(), "Rebind the original Ctrl+B pair");
    expect(hotkeys.resetShortcut("borderize_selection").isEmpty(), "Individual reset preserves original contextual defaults");
    hotkeys.resetAll();
    HotkeySettings reset;
    expect(reset.bindings() == defaults, "Reset all must restore persisted defaults including aliases");
    return EXIT_SUCCESS;
}

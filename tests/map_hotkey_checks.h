#pragma once

#include <QKeyEvent>

// Exercise canvas dispatch separately from QML menu shortcut activation.
static bool testMapHotkeys()
{
    class HotkeyView : public MapView {
    public:
        using MapView::keyPressEvent;
    };
    HotkeyView view;
    view.setFloor(7);
    const auto press = [&view](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        view.keyPressEvent(&event);
    };
    view.setProperty("hotkeyBindings", QVariantMap{
        {"floor_up", "F6"}, {"floor_up_alternative", ""},
        {"floor_down", "Ctrl+Alt+D"}, {"show_creatures", "Ctrl+Alt+F"}
    });
    press(Qt::Key_Plus);
    press(Qt::Key_Equal);
    if (!require(view.floor() == 7, "Former floor bindings remained active after rebinding")) return false;
    press(Qt::Key_F6);
    if (!require(view.floor() == 6, "New floor shortcut did not activate")) return false;
    press(Qt::Key_Plus, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(view.floor() == 6, "Zoom modifiers unexpectedly changed the floor")) return false;
    press(Qt::Key_D, Qt::ControlModifier | Qt::AltModifier);
    if (!require(view.floor() == 7, "Modifier combinations must work for canvas commands")) return false;
    const bool creaturesVisible = view.showCreatures();
    press(Qt::Key_F);
    if (!require(view.showCreatures() == creaturesVisible, "Former view-toggle binding remained active")) return false;
    press(Qt::Key_F, Qt::ControlModifier | Qt::AltModifier);
    if (!require(view.showCreatures() != creaturesVisible, "New view-toggle shortcut did not activate")) return false;
    view.setProperty("commandHotkeysEnabled", false);
    press(Qt::Key_F6);
    if (!require(view.floor() == 7, "Canvas commands must pause while recording or walking")) return false;
    view.setProperty("commandHotkeysEnabled", true);
    view.setProperty("hotkeyBindings", QVariantMap{{"floor_up", ""}, {"floor_up_alternative", ""}});
    press(Qt::Key_F6);
    press(Qt::Key_Plus);
    if (!require(view.floor() == 7, "Clearing a shortcut must disable canvas dispatch")) return false;
    return true;
}

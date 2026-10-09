#include "hotkeysettings.h"

#include <QKeySequence>

namespace {
struct Command {
    const char *id;
    const char *label;
    const char *category;
    const char *shortcut;
    const char *scope = "editor";
    bool editable = true;
};

const Command commands[] = {
    {"new", "New", "File", "Ctrl+N", "window"},
    {"open", "Open", "File", "Ctrl+O", "window"},
    {"import_map", "Import Map", "File", "", "editor"},
    {"export_minimap", "Export Minimap", "File", "", "editor"},
    {"save", "Save", "File", "Ctrl+S", "window"},
    {"save_as", "Save As", "File", "Ctrl+Shift+S", "window"},
    {"preferences", "Preferences", "File", "", "window"},
    {"close_map", "Close map", "File", "Ctrl+Q", "window"},
    {"exit", "Exit", "File", "", "window"},
    {"undo", "Undo", "Edit", "Ctrl+Z", "editor"},
    {"redo", "Redo", "Edit", "Ctrl+Shift+Z", "editor"},
    {"find_item", "Find Item", "Edit", "Ctrl+F", "editor"},
    {"replace_items", "Replace Items", "Edit", "Ctrl+Shift+F", "editor"},
    {"border_automagic", "Border Automagic", "Edit", "A", "editor"},
    {"borderize_selection", "Borderize Selection", "Edit", "Ctrl+B", "editor"},
    {"borderize_map", "Borderize Map", "Edit", "", "editor"},
    {"randomize_selection", "Randomize Selection", "Edit", "", "editor"},
    {"randomize_map", "Randomize Map", "Edit", "", "editor"},
    {"remove_items_by_id", "Remove Items by ID", "Edit", "", "editor"},
    {"go_to_previous_position", "Go to Previous Position", "Edit", "P", "editor"},
    {"go_to_position", "Go to Position", "Edit", "Ctrl+G", "editor"},
    {"cut", "Cut", "Edit", "Ctrl+X", "editor"},
    {"copy", "Copy", "Edit", "Ctrl+C", "editor"},
    {"paste", "Paste", "Edit", "Ctrl+V", "editor"},
    {"ai_map_assistant", "AI Map Assistant", "Edit", "", "editor"},
    {"find_unique_ids", "Find Unique IDs", "Search", "", "editor"},
    {"find_action_ids", "Find Action IDs", "Search", "", "editor"},
    {"find_containers", "Find Containers", "Search", "", "editor"},
    {"find_writable_items", "Find Writable Items", "Search", "", "editor"},
    {"find_everything", "Find Everything", "Search", "", "editor"},
    {"edit_towns", "Edit Towns", "Map", "Ctrl+T", "editor"},
    {"edit_waypoints", "Edit Waypoints", "Map", "", "editor"},
    {"edit_monsters", "Edit Monsters", "Map", "", "editor"},
    {"cleanup", "Cleanup", "Map", "", "editor"},
    {"map_properties", "Map properties", "Map", "Ctrl+P", "editor"},
    {"map_analyzer", "Map Analyzer", "Map", "F8", "editor"},
    {"replace_items_on_selection", "Replace Items on Selection", "Select", "", "editor"},
    {"find_item_on_selection", "Find Item on Selection", "Select", "", "editor"},
    {"remove_item_on_selection", "Remove Item on Selection", "Select", "", "editor"},
    {"find_everything_on_selection", "Find Everything on Selection", "Select", "", "editor"},
    {"compensate_selection", "Compensate Selection", "Select", "", "editor"},
    {"current_floor", "Current Floor", "Select", "", "editor"},
    {"lower_floors", "Lower Floors", "Select", "", "editor"},
    {"visible_floors", "Visible Floors", "Select", "", "editor"},
    {"clear_selection", "Clear Selection", "Select", "", "editor"},
    {"dungeon_generator", "Dungeon Generator", "Tools", "", "editor"},
    {"terrain_generator", "Terrain Generator", "Tools", "", "editor"},
    {"ground_prefab_generator", "Ground Prefab Generator", "Tools", "", "editor"},
    {"tileset_brush_manager", "Tileset & Brush Manager", "Tools", "", "editor"},
    {"show_palette", "Show palette", "View", "Ctrl+B", "window"},
    {"show_fps", "Show FPS", "View", "", "window"},
    {"show_work_timer", "Show Work Timer", "View", "", "window"},
    {"zoom_in", "Zoom In", "View", "Ctrl++", "editor"},
    {"zoom_out", "Zoom Out", "View", "Ctrl+-", "editor"},
    {"zoom_normal", "Zoom Normal", "View", "Ctrl+0", "editor"},
    {"show_animation", "Show animation", "View", "L", "editor"},
    {"show_light", "Show light", "View", "Shift+L", "editor"},
    {"show_light_sources", "Show light sources", "View", "", "editor"},
    {"show_minimap", "Show minimap", "View", "M", "editor"},
    {"in_game_preview_window", "In-game preview window", "View", "Ctrl+Shift+I", "window"},
    {"show_shade", "Show shade", "View", "Q", "editor"},
    {"show_lower_floors", "Show lower floors", "View", "Ctrl+W", "editor"},
    {"placement_effect", "Placement effect", "View", "", "editor"},
    {"show_grid", "Show grid", "View", "Shift+G", "editor"},
    {"show_client_box", "Show client box", "View", "Shift+I", "editor"},
    {"show_tooltips", "Show tooltips", "View", "Y", "editor"},
    {"show_waypoints", "Show waypoints", "View", "Shift+W", "editor"},
    {"show_wall_outlines", "Show wall outlines", "View", "", "editor"},
    {"show_pathing", "Show pathing", "View", "O", "editor"},
    {"show_creatures", "Show creatures", "View", "F", "editor"},
    {"show_spawns", "Show spawns", "View", "S", "editor"},
    {"show_houses", "Show houses", "View", "Ctrl+H", "editor"},
    {"show_special_zones", "Show special zones", "View", "E", "editor"},
    {"always_show_zones", "Always show zones", "View", "", "editor"},
    {"about", "About", "Help", "", "window"},
    {"save_as_alternative", "Save As (alternative)", "File", "Ctrl+Alt+S", "window"},
    {"redo_alternative", "Redo (alternative)", "Edit", "Ctrl+Y", "editor"},
    {"zoom_in_alternative", "Zoom In (alternative)", "View", "Ctrl+=", "editor"},
    {"browse_field", "Browse field", "Edit", "Alt+A", "editor"},
    {"floor_up", "Floor up", "Map navigation", "+", "editor"},
    {"floor_up_alternative", "Floor up (alternative)", "Map navigation", "=", "editor"},
    {"floor_down", "Floor down", "Map navigation", "-", "editor"},
    {"rotate", "Rotate selection / prefab", "Selection and brushes", "Z", "editor"},
    {"next_doodad_variant", "Next doodad variant", "Selection and brushes", "R", "editor"},
    {"delete_selected_top", "Delete selected top items", "Selection and brushes", "Del", "editor"},
    {"preview_left", "Walk left", "In-game preview", "A", "preview"},
    {"preview_right", "Walk right", "In-game preview", "D", "preview"},
    {"preview_up", "Walk up", "In-game preview", "W", "preview"},
    {"preview_down", "Walk down", "In-game preview", "S", "preview"},
    {"preview_fullscreen", "Toggle fullscreen", "In-game preview", "F11", "preview"},
    {"preview_northwest", "Walk northwest", "Fixed controls", "Home", "preview", false},
    {"preview_northeast", "Walk northeast", "Fixed controls", "PgUp", "preview", false},
    {"preview_southwest", "Walk southwest", "Fixed controls", "End", "preview", false},
    {"preview_southeast", "Walk southeast", "Fixed controls", "PgDown", "preview", false},
    {"pan_map", "Pan map (hold)", "Fixed controls", "Space", "window", false},
    {"toggle_selection_tool", "Toggle selection tool", "Fixed controls", "Alt", "window", false},
    {"cancel_operation", "Cancel operation / close dialog", "Fixed controls", "Esc", "window", false},
    {"apply_path_preview", "Apply path preview", "Fixed controls", "Return", "window", false},
    {"apply_path_preview_keypad", "Apply path preview (keypad)", "Fixed controls", "Enter", "window", false},
    {"move_left", "Pan map / walk left", "Fixed controls", "Left", "window", false},
    {"move_right", "Pan map / walk right", "Fixed controls", "Right", "window", false},
    {"move_up", "Pan map / walk up", "Fixed controls", "Up", "window", false},
    {"move_down", "Pan map / walk down", "Fixed controls", "Down", "window", false},
    {"pan_fast_left", "Fast map pan left", "Fixed controls", "Shift+Left", "editor", false},
    {"pan_fast_right", "Fast map pan right", "Fixed controls", "Shift+Right", "editor", false},
    {"pan_fast_up", "Fast map pan up", "Fixed controls", "Shift+Up", "editor", false},
    {"pan_fast_down", "Fast map pan down", "Fixed controls", "Shift+Down", "editor", false},
};

const Command *findCommand(const QString &id)
{
    for (const auto &command : commands)
        if (id == QLatin1String(command.id)) return &command;
    return nullptr;
}

QString normalized(const QString &text)
{
    return QKeySequence::fromString(text, QKeySequence::PortableText)
        .toString(QKeySequence::PortableText);
}

bool validSequence(const QString &text)
{
    if (text.isEmpty()) return true;
    const auto sequence = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (sequence.count() != 1) return false;
    const auto key = sequence[0].key();
    return key != Qt::Key_unknown && key != 0 && key != Qt::Key_Control
        && key != Qt::Key_Shift && key != Qt::Key_Alt && key != Qt::Key_Meta;
}

bool scopesOverlap(const Command &left, const Command &right)
{
    return QLatin1String(left.scope) == QLatin1String(right.scope)
        || QLatin1String(left.scope) == "window" || QLatin1String(right.scope) == "window";
}
}

HotkeySettings::HotkeySettings(QObject *parent) : QObject(parent)
{
    connect(this, &HotkeySettings::bindingsChanged, this, &HotkeySettings::activeBindingsChanged);
    connect(this, &HotkeySettings::capturingChanged, this, &HotkeySettings::activeBindingsChanged);
    connect(this, &HotkeySettings::previewActiveChanged, this, &HotkeySettings::activeBindingsChanged);
    m_settings.beginGroup(QStringLiteral("hotkeys"));
    for (const auto &command : ::commands) {
        const QString id = QLatin1String(command.id);
        if (!command.editable || !m_settings.contains(id)) continue;
        const auto saved = m_settings.value(id).toString();
        if (validSequence(saved)) m_overrides.insert(id, normalized(saved));
    }
    m_settings.endGroup();
}

QVariantMap HotkeySettings::bindings() const
{
    QVariantMap result;
    for (const auto &command : ::commands)
        result.insert(QLatin1String(command.id),
                      m_overrides.value(QLatin1String(command.id), QString::fromLatin1(command.shortcut)));
    return result;
}

QVariantMap HotkeySettings::activeBindings() const
{
    auto result = bindings();
    QStringList walkingKeys;
    if (m_previewActive) {
        for (const auto &command : ::commands)
            if (QLatin1String(command.scope) == "preview")
                walkingKeys.append(result.value(QLatin1String(command.id)).toString());
    }
    for (const auto &command : ::commands) {
        const QString id = QLatin1String(command.id);
        if (m_capturing || (QLatin1String(command.scope) == "editor"
                          && walkingKeys.contains(result.value(id).toString())))
            result.insert(id, QString());
    }
    return result;
}

QVariantList HotkeySettings::commands() const
{
    QVariantList result;
    const auto current = bindings();
    for (const auto &command : ::commands) {
        const QString id = QLatin1String(command.id);
        result.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), QString::fromLatin1(command.label)},
            {QStringLiteral("category"), QString::fromLatin1(command.category)},
            {QStringLiteral("scope"), QString::fromLatin1(command.scope)},
            {QStringLiteral("shortcut"), current.value(id)},
            {QStringLiteral("defaultShortcut"), QString::fromLatin1(command.shortcut)},
            {QStringLiteral("editable"), command.editable}
        });
    }
    return result;
}

void HotkeySettings::setCapturing(bool capturing)
{
    if (m_capturing == capturing) return;
    m_capturing = capturing;
    emit capturingChanged();
}

void HotkeySettings::setPreviewActive(bool active)
{
    if (m_previewActive == active) return;
    m_previewActive = active;
    emit previewActiveChanged();
}

QString HotkeySettings::setShortcut(const QString &id, const QString &shortcut)
{
    const auto *command = findCommand(id);
    if (!command || !command->editable) return tr("This control cannot be changed.");
    const QString text = shortcut.trimmed();
    if (!validSequence(text)) return tr("Press a single key combination, such as Ctrl+G.");
    const QString value = normalized(text);
    const auto current = bindings();
    if (value == current.value(id).toString()) return {};
    if (!value.isEmpty()) {
        for (const auto &other : ::commands) {
            if (id == QLatin1String(other.id) || !scopesOverlap(*command, other)) continue;
            const auto otherBinding = normalized(current.value(QLatin1String(other.id)).toString());
            // Preserve the original contextual Ctrl+B pair when restoring defaults.
            const bool originalPair = value == normalized(QLatin1String(command->shortcut))
                && otherBinding == normalized(QLatin1String(other.shortcut));
            if (value == otherBinding && !originalPair)
                return tr("%1 is already used by %2 (%3).")
                    .arg(value, QLatin1String(other.label), QLatin1String(other.category));
        }
    }
    const QString key = QStringLiteral("hotkeys/") + id;
    if (value == QLatin1String(command->shortcut)) {
        m_overrides.remove(id);
        m_settings.remove(key);
    } else {
        m_overrides.insert(id, value);
        m_settings.setValue(key, value);
    }
    m_settings.sync();
    emit bindingsChanged();
    return {};
}

QString HotkeySettings::resetShortcut(const QString &id)
{
    const auto *command = findCommand(id);
    if (!command) return tr("Unknown command.");
    return setShortcut(id, QLatin1String(command->shortcut));
}

void HotkeySettings::resetAll()
{
    m_overrides.clear();
    m_settings.remove(QStringLiteral("hotkeys"));
    m_settings.sync();
    emit bindingsChanged();
}

QString HotkeySettings::shortcutFromKey(int key, int modifiers) const
{
    if (key == 0 || key == Qt::Key_unknown || key == Qt::Key_Control
        || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) return {};
    auto flags = Qt::KeyboardModifiers(modifiers)
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    // '+' already expresses Shift on keyboards where it shares the '=' key.
    if (key == Qt::Key_Plus) flags &= ~Qt::ShiftModifier;
    return QKeySequence(QKeyCombination(flags, Qt::Key(key))).toString(QKeySequence::PortableText);
}

bool HotkeySettings::matches(const QString &id, int key, int modifiers) const
{
    if (m_capturing) return false;
    const auto *command = findCommand(id);
    if (!command) return false;
    const auto binding = activeBindings().value(id).toString();
    return !binding.isEmpty() && binding == shortcutFromKey(key, modifiers);
}

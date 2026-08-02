
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QGuiApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

bool MapView::setContextSpawnRadius(int radius)
{
    if (!m_otbm) return false;
    radius = std::clamp(radius, 1, 15);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        if (!t || t->spawn_radius <= 0) return false;
        if (t->spawn_radius == radius) return false;
        changed = m_otbm->setSpawnAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), radius);
        if (changed) {
            m_spawnIndex.upsert(m_itemController.contextX(), m_itemController.contextY(), radius);
            ++m_metadataOverlayVersion;
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::setContextCreatureSpawntime(int seconds)
{
    if (!m_otbm) return false;
    seconds = std::clamp(seconds, 1, 86400);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        if (!t || t->creature_name.isEmpty()) return false;
        if (t->creature_spawntime == seconds) return false;

        changed = m_otbm->setCreatureAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                        t->creature_name, seconds, t->creature_is_npc);
    }
    return changed;
}

bool MapView::setContextItemCount(int count)
{
    if (!m_otbm) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        if (!t || m_itemController.contextItemIndex() < 0
            || m_itemController.contextItemIndex() >= static_cast<int>(t->items.size())) return false;

        const int cid = m_otb
                            ? m_otb->clientIdForServerId(
                                  t->items[static_cast<size_t>(m_itemController.contextItemIndex())].server_id)
                            : 0;
        const ClientItem *ci = (m_dat && cid > 0)
                                   ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
        const OtbmMapItem &item =
            t->items[static_cast<size_t>(m_itemController.contextItemIndex())];
        const int group = m_otb ? m_otb->groupForServerId(item.server_id) : 0;
        const bool editable = (ci && ci->is_stackable)
                              || group == static_cast<int>(OtbItemGroup::Splash)
                              || group == static_cast<int>(OtbItemGroup::Fluid)
                              || item.has_subtype_attribute;
        if (!editable) return false;
        const bool charges =
            item.subtype_attribute == static_cast<uint8_t>(OtbmAttribute::Charges);
        const int minimum = ci && ci->is_stackable ? 1 : 0;
        const int maximum = charges ? 65535 : (ci && ci->is_stackable ? 100 : 255);
        count = std::clamp(count, minimum, maximum);

        changed = m_otbm->setItemCountAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                         m_itemController.contextItemIndex(),
                                         static_cast<uint16_t>(count));
        if (changed) {

            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::applyContextItemProperties(const QVariantMap &props)
{
    if (!m_otbm) return false;

    bool changed = false;
    bool spriteDirty = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

        m_otbm->beginUndoGroup();
        const OtbmTile *selectedTile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        const int selectedIndex = selectedTile && m_itemController.contextItemIndex() >= 0
                                      && m_itemController.contextItemIndex()
                                             < static_cast<int>(selectedTile->items.size())
                                      ? m_itemController.contextItemIndex()
                                      : (selectedTile && !selectedTile->items.empty()
                                             ? static_cast<int>(selectedTile->items.size()) - 1
                                             : -1);

        if (props.contains(QStringLiteral("actionId"))) {
            const int v = std::clamp(props.value(QStringLiteral("actionId")).toInt(), 0, 65535);
            changed |= m_otbm->setItemActionIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                                 selectedIndex, static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("uniqueId"))) {
            const int v = std::clamp(props.value(QStringLiteral("uniqueId")).toInt(), 0, 65535);
            changed |= m_otbm->setItemUniqueIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                                 selectedIndex, static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("text"))) {
            changed |= m_otbm->setItemTextAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                             selectedIndex,
                                             props.value(QStringLiteral("text")).toString());
        }
        if (props.contains(QStringLiteral("description"))) {
            changed |= m_otbm->setItemDescriptionAt(
                m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), selectedIndex,
                props.value(QStringLiteral("description")).toString());
        }
        if (props.contains(QStringLiteral("depotId"))) {
            const int value = std::clamp(props.value(QStringLiteral("depotId")).toInt(),
                                         0, 65535);
            changed |= m_otbm->setItemDepotIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                                selectedIndex,
                                                static_cast<uint16_t>(value));
        }
        if (props.contains(QStringLiteral("doorId"))) {
            const int value = std::clamp(props.value(QStringLiteral("doorId")).toInt(),
                                         0, 255);
            changed |= m_otbm->setItemDoorIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                               selectedIndex,
                                               static_cast<uint8_t>(value));
        }
        if (props.contains(QStringLiteral("tier"))) {
            const int value = std::clamp(props.value(QStringLiteral("tier")).toInt(),
                                         0, 255);
            changed |= m_otbm->setItemTierAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                             selectedIndex,
                                             static_cast<uint8_t>(value));
        }
        if (props.value(QStringLiteral("teleportClear")).toBool()) {
            changed |= m_otbm->setItemTeleportAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                                 selectedIndex, -1, -1, -1);
        } else if (props.contains(QStringLiteral("teleportX"))) {
            changed |= m_otbm->setItemTeleportAt(
                m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), selectedIndex,
                props.value(QStringLiteral("teleportX")).toInt(),
                props.value(QStringLiteral("teleportY")).toInt(),
                props.value(QStringLiteral("teleportZ")).toInt());
        }
        if (props.contains(QStringLiteral("count"))) {

            const OtbmTile *t = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
            const int cid = (t && selectedIndex >= 0
                             && selectedIndex < static_cast<int>(t->items.size()) && m_otb)
                                ? m_otb->clientIdForServerId(
                                      t->items[static_cast<size_t>(selectedIndex)].server_id)
                                : 0;
            const ClientItem *ci = (m_dat && cid > 0)
                                       ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
            const OtbmMapItem *item =
                t && selectedIndex >= 0
                    && selectedIndex < static_cast<int>(t->items.size())
                    ? &t->items[static_cast<size_t>(selectedIndex)] : nullptr;
            const int group = item && m_otb
                                  ? m_otb->groupForServerId(item->server_id) : 0;
            const bool editable = item && ((ci && ci->is_stackable)
                || group == static_cast<int>(OtbItemGroup::Splash)
                || group == static_cast<int>(OtbItemGroup::Fluid)
                || item->has_subtype_attribute);
            if (editable) {
                const bool charges = item->subtype_attribute
                                     == static_cast<uint8_t>(OtbmAttribute::Charges);
                const int minimum = ci && ci->is_stackable ? 1 : 0;
                const int maximum =
                    charges ? 65535 : (ci && ci->is_stackable ? 100 : 255);
                const int v = std::clamp(
                    props.value(QStringLiteral("count")).toInt(),
                    minimum, maximum);
                if (m_otbm->setItemCountAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                           selectedIndex, static_cast<uint16_t>(v))) {
                    changed = true;
                    spriteDirty = true;
                }
            }
        }
        if (props.contains(QStringLiteral("customAttributes"))) {
            changed |= m_otbm->setItemAttributeMapAt(
                m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), selectedIndex,
                props.value(QStringLiteral("customAttributes")).toList());
        }

        m_otbm->endUndoGroup();

        if (spriteDirty) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::setContextItemActionId(int actionId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(actionId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemActionIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                     m_itemController.contextItemIndex(), static_cast<uint16_t>(v));
}

bool MapView::setContextItemUniqueId(int uniqueId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(uniqueId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemUniqueIdAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                     m_itemController.contextItemIndex(), static_cast<uint16_t>(v));
}

bool MapView::setContextItemText(const QString &text)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemTextAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                 m_itemController.contextItemIndex(), text);
}

bool MapView::setContextItemTeleport(int destX, int destY, int destZ)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemTeleportAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                     m_itemController.contextItemIndex(), destX, destY, destZ);
}

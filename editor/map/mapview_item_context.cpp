
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

QVariantMap MapView::itemContextInfo(const OtbmMapItem &item, int index) const
{
    QVariantMap m;
    m.insert(QStringLiteral("x"), m_itemController.contextX());
    m.insert(QStringLiteral("y"), m_itemController.contextY());
    m.insert(QStringLiteral("z"), m_navigationController.floor());
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("hasItem"), true);
    m.insert(QStringLiteral("serverId"), item.server_id);

    const int cid = m_otb ? m_otb->clientIdForServerId(item.server_id) : 0;
    const ClientItem *ci = (m_dat && cid > 0)
                               ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    m.insert(QStringLiteral("clientId"), cid);
    m.insert(QStringLiteral("name"),
             m_otb ? m_otb->nameForServerId(item.server_id) : QString());
    m.insert(QStringLiteral("groupName"),
             m_otb ? m_otb->groupNameForServerId(item.server_id) : QString());
    m.insert(QStringLiteral("ground"), item.is_ground);
    m.insert(QStringLiteral("stackable"), ci && ci->is_stackable);
    m.insert(QStringLiteral("count"), item.count);
    const int group = m_otb ? m_otb->groupForServerId(item.server_id) : 0;
    const bool charges =
        item.subtype_attribute == static_cast<uint8_t>(OtbmAttribute::Charges);
    const bool subtypeEditable = (ci && ci->is_stackable)
        || group == static_cast<int>(OtbItemGroup::Splash)
        || group == static_cast<int>(OtbItemGroup::Fluid)
        || item.has_subtype_attribute;
    m.insert(QStringLiteral("subtypeEditable"), subtypeEditable);
    m.insert(QStringLiteral("subtypeMinimum"), ci && ci->is_stackable ? 1 : 0);
    m.insert(QStringLiteral("subtypeMaximum"),
             charges ? 65535 : (ci && ci->is_stackable ? 100 : 255));
    m.insert(QStringLiteral("subtypeLabel"),
             charges ? QStringLiteral("Charges")
                     : (group == static_cast<int>(OtbItemGroup::Fluid)
                            || group == static_cast<int>(OtbItemGroup::Splash)
                            ? QStringLiteral("Fluid subtype")
                            : QStringLiteral("Count")));
    m.insert(QStringLiteral("actionId"), item.actionId());
    m.insert(QStringLiteral("uniqueId"), item.uniqueId());
    m.insert(QStringLiteral("depotId"), item.depotId());
    m.insert(QStringLiteral("childCount"),
             item.children() ? static_cast<int>(item.children()->size()) : 0);

    const QString text = item.extra ? item.extra->text : QString();
    m.insert(QStringLiteral("text"), text);
    m.insert(QStringLiteral("description"),
             item.extra ? item.extra->description : QString());
    m.insert(QStringLiteral("doorId"), item.extra ? item.extra->door_id : 0);
    m.insert(QStringLiteral("tier"), item.extra ? item.extra->tier : 0);
    m.insert(QStringLiteral("writable"), (ci && ci->is_writable) || !text.isEmpty());

    const bool isTele = m_otb && m_otb->isTeleportItem(item.server_id);
    const bool hasTele = item.extra && item.extra->has_teleport;
    m.insert(QStringLiteral("teleport"), isTele || hasTele);
    m.insert(QStringLiteral("hasTeleportDest"), hasTele);
    m.insert(QStringLiteral("teleportX"), hasTele ? item.extra->tele_x : 0);
    m.insert(QStringLiteral("teleportY"), hasTele ? item.extra->tele_y : 0);
    m.insert(QStringLiteral("teleportZ"), hasTele ? item.extra->tele_z : 0);
    QVariantList customAttributes;
    if (item.extra && item.extra->has_attribute_map) {
        for (const auto &attribute : item.extra->attribute_map) {
            const bool managedInteger =
                attribute.type == 2
                && (attribute.key == QByteArrayLiteral("aid")
                    || attribute.key == QByteArrayLiteral("uid")
                    || attribute.key == QByteArrayLiteral("tier"));
            const bool managedString =
                attribute.type == 1
                && (attribute.key == QByteArrayLiteral("text")
                    || attribute.key == QByteArrayLiteral("desc"));
            if (managedInteger || managedString) continue;

            QVariantMap value;
            value.insert(QStringLiteral("key"),
                         QString::fromLatin1(attribute.key));
            value.insert(QStringLiteral("typeId"), attribute.type);
            value.insert(QStringLiteral("rawBase64"),
                         QString::fromLatin1(attribute.value_raw.toBase64()));
            QString type = QStringLiteral("Unknown");
            QString text;
            if (attribute.type == 1 && attribute.value_raw.size() >= 4) {
                const auto *bytes = reinterpret_cast<const uchar *>(
                    attribute.value_raw.constData());
                const quint32 length = static_cast<quint32>(bytes[0])
                    | (static_cast<quint32>(bytes[1]) << 8)
                    | (static_cast<quint32>(bytes[2]) << 16)
                    | (static_cast<quint32>(bytes[3]) << 24);
                if (length == static_cast<quint32>(
                                  attribute.value_raw.size() - 4)) {
                    type = QStringLiteral("String");
                    text = QString::fromLatin1(attribute.value_raw.constData() + 4,
                                               static_cast<qsizetype>(length));
                }
            } else if ((attribute.type == 2 || attribute.type == 3)
                       && attribute.value_raw.size() == 4) {
                if (attribute.type == 2) {
                    qint32 number = 0;
                    std::memcpy(&number, attribute.value_raw.constData(), 4);
                    type = QStringLiteral("Number");
                    text = QString::number(number);
                } else {
                    float number = 0;
                    std::memcpy(&number, attribute.value_raw.constData(), 4);
                    type = QStringLiteral("Float");
                    text = QString::number(number, 'g', 9);
                }
            } else if (attribute.type == 4
                       && attribute.value_raw.size() == 1) {
                type = QStringLiteral("Boolean");
                text = attribute.value_raw[0] != 0
                           ? QStringLiteral("true") : QStringLiteral("false");
            } else if (attribute.type == 5
                       && attribute.value_raw.size() == 8) {
                double number = 0;
                std::memcpy(&number, attribute.value_raw.constData(), 8);
                type = QStringLiteral("Double");
                text = QString::number(number, 'g', 17);
            }
            value.insert(QStringLiteral("type"), type);
            value.insert(QStringLiteral("value"), text);
            customAttributes.append(value);
        }
    }
    m.insert(QStringLiteral("customAttributes"), customAttributes);
    m.insert(QStringLiteral("customAttributesSupported"),
             (item.extra && item.extra->has_attribute_map)
             || (m_otbm
                 && m_otbm->header().value(QStringLiteral("otbmVersion")).toInt()
                        >= static_cast<int>(OtbmVersion::V4)));
    const int rotateTo = m_otb ? m_otb->rotateToForServerId(item.server_id) : 0;
    m.insert(QStringLiteral("canRotate"),
             rotateTo > 0 && m_otb && m_otb->rowForServerId(rotateTo) >= 0);
    const bool door =
        m_brushController.store() && m_brushController.store()->canSwitchDoor(item.server_id);
    m.insert(QStringLiteral("door"), door);
    m.insert(QStringLiteral("doorOpen"),
             door && m_brushController.store()->isDoorOpen(item.server_id));

    if (m_otb) {
        const QVariantMap details = m_otb->detailsAt(m_otb->rowForServerId(item.server_id));
        m.insert(QStringLiteral("spriteIds"), details.value(QStringLiteral("spriteIds")));
        m.insert(QStringLiteral("itemWidth"), details.value(QStringLiteral("itemWidth"), 1));
        m.insert(QStringLiteral("itemHeight"), details.value(QStringLiteral("itemHeight"), 1));
        m.insert(QStringLiteral("layers"), details.value(QStringLiteral("layers"), 1));
    }
    return m;
}

QVariantMap MapView::contextInfo() const
{
    const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
    QVariantMap m;
    const bool has = tile && !tile->items.empty();
    if (has) {
        const int index = (m_itemController.contextItemIndex() >= 0
                           && m_itemController.contextItemIndex() < static_cast<int>(tile->items.size()))
                              ? m_itemController.contextItemIndex()
                              : static_cast<int>(tile->items.size()) - 1;
        m = itemContextInfo(tile->items[static_cast<size_t>(index)], index);
    } else {
        m.insert(QStringLiteral("x"), m_itemController.contextX());
        m.insert(QStringLiteral("y"), m_itemController.contextY());
        m.insert(QStringLiteral("z"), m_navigationController.floor());
        m.insert(QStringLiteral("hasItem"), false);
        m.insert(QStringLiteral("serverId"), 0);
        m.insert(QStringLiteral("clientId"), 0);
        m.insert(QStringLiteral("name"), QString());
        m.insert(QStringLiteral("groupName"), QString());
        m.insert(QStringLiteral("stackable"), false);
        m.insert(QStringLiteral("count"), 0);
        m.insert(QStringLiteral("subtypeEditable"), false);
        m.insert(QStringLiteral("subtypeMinimum"), 0);
        m.insert(QStringLiteral("subtypeMaximum"), 100);
        m.insert(QStringLiteral("subtypeLabel"), QStringLiteral("Count"));
        m.insert(QStringLiteral("actionId"), 0);
        m.insert(QStringLiteral("uniqueId"), 0);
        m.insert(QStringLiteral("text"), QString());
        m.insert(QStringLiteral("writable"), false);
        m.insert(QStringLiteral("teleport"), false);
        m.insert(QStringLiteral("hasTeleportDest"), false);
        m.insert(QStringLiteral("teleportX"), 0);
        m.insert(QStringLiteral("teleportY"), 0);
        m.insert(QStringLiteral("teleportZ"), 0);
        m.insert(QStringLiteral("customAttributes"), QVariantList());
        m.insert(QStringLiteral("customAttributesSupported"), false);
        m.insert(QStringLiteral("canRotate"), false);
        m.insert(QStringLiteral("door"), false);
        m.insert(QStringLiteral("doorOpen"), false);
    }

    m.insert(QStringLiteral("selectionCount"), m_selectionController.selected().size());
    m.insert(QStringLiteral("creatureName"),
             tile ? tile->creature_name.value() : QString());
    m.insert(QStringLiteral("creatureSpawntime"), tile ? tile->creature_spawntime : 0);
    m.insert(QStringLiteral("spawnRadius"), tile ? tile->spawn_radius : 0);
    return m;
}

QVariantList MapView::contextStack() const
{
    QVariantList result;
    const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
    if (!tile) return result;

    result.reserve(static_cast<qsizetype>(tile->items.size()));
    for (int index = static_cast<int>(tile->items.size()) - 1; index >= 0; --index) {
        QVariantMap item = itemContextInfo(tile->items[static_cast<size_t>(index)], index);
        item.insert(QStringLiteral("top"), index == static_cast<int>(tile->items.size()) - 1);
        result.append(item);
    }
    return result;
}

bool MapView::setContextStackIndex(int index)
{
    const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
    if (!tile || index < 0 || index >= static_cast<int>(tile->items.size())) return false;
    m_itemController.contextItemIndex() = index;
    return true;
}

bool MapView::removeContextStackItem(int index)
{
    if (!m_otbm) return false;
    bool removed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        removed = m_otbm->removeItemAt(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), index);
        if (removed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
            const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
            m_itemController.contextItemIndex() = tile && !tile->items.empty()
                                     ? std::min(index, static_cast<int>(tile->items.size()) - 1)
                                     : -1;
        }
    }
    if (removed) refreshAfterEdit(0);
    return removed;
}

bool MapView::rotateContextItem()
{
    if (!m_otbm || !m_otb) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        if (!tile || m_itemController.contextItemIndex() < 0
            || m_itemController.contextItemIndex() >= static_cast<int>(tile->items.size())) {
            return false;
        }
        const int target = m_otb->rotateToForServerId(
            tile->items[static_cast<size_t>(m_itemController.contextItemIndex())].server_id);
        if (target <= 0 || target > 65535 || m_otb->rowForServerId(target) < 0) {
            return false;
        }
        ensureItemSprites(target);
        changed = m_otbm->setItemServerIdAt(
            m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), m_itemController.contextItemIndex(),
            static_cast<uint16_t>(target));
        if (changed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::switchContextDoor()
{
    if (!m_otbm || !m_otb || !m_brushController.store()) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        const OtbmTile *tile = currentFloorTileAt(m_itemController.contextX(), m_itemController.contextY());
        if (!tile || m_itemController.contextItemIndex() < 0
            || m_itemController.contextItemIndex() >= static_cast<int>(tile->items.size())) {
            return false;
        }
        const int source =
            tile->items[static_cast<size_t>(m_itemController.contextItemIndex())].server_id;
        const int target = m_brushController.store()->switchedDoorItem(source);
        if (target <= 0 || target > 65535 || m_otb->rowForServerId(target) < 0) {
            return false;
        }
        ensureItemSprites(target);
        changed = m_otbm->setItemServerIdAt(
            m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), m_itemController.contextItemIndex(),
            static_cast<uint16_t>(target));
        if (changed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

QVariantList MapView::contextItemPath() const
{
    QVariantList path;
    if (m_itemController.contextItemIndex() >= 0) path.append(m_itemController.contextItemIndex());
    return path;
}

QVariantList MapView::contextContainerItems(const QVariantList &pathValues) const
{
    QVariantList result;
    if (!m_otbm) return result;

    std::vector<int> path;
    path.reserve(static_cast<size_t>(pathValues.size()));
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const OtbmMapItem *container =
        m_otbm->itemAtPath(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(), path);
    if (!container || !container->children()) return result;

    for (int index = static_cast<int>(container->children()->size()) - 1;
         index >= 0; --index) {
        QVariantMap child =
            itemContextInfo((*container->children())[static_cast<size_t>(index)], index);
        child.insert(QStringLiteral("childIndex"), index);
        QVariantList childPath = pathValues;
        childPath.append(index);
        child.insert(QStringLiteral("path"), childPath);
        result.append(child);
    }
    return result;
}

bool MapView::addContextContainerItem(const QVariantList &pathValues, int serverId)
{
    if (!m_otbm || serverId <= 0 || serverId > 65535) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        ensureItemSprites(serverId);
        changed = m_otbm->addContainerChild(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                            path, static_cast<uint16_t>(serverId));
        if (changed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::removeContextContainerItem(const QVariantList &pathValues, int childIndex)
{
    if (!m_otbm) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        changed = m_otbm->removeContainerChild(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                               path, childIndex);
        if (changed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::moveContextContainerItem(const QVariantList &pathValues,
                                       int childIndex, int delta)
{
    if (!m_otbm) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        changed = m_otbm->moveContainerChild(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor(),
                                             path, childIndex, delta);
        if (changed) {
            onTileEdited(m_itemController.contextX(), m_itemController.contextY(), m_navigationController.floor());
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

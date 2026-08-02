
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

int MapView::groundServerIdAt(const OtbmTile *tile) const
{
    if (!tile) return 0;
    for (const OtbmMapItem &it : tile->items)
        if (itemCategory(it.server_id) == 0) return it.server_id;
    return 0;
}
QString MapView::groundBrushNameAt(int x, int y) const
{
    if (!m_brushController.store()) return QString();

    if (m_groundNameCacheOn) {
        const quint64 pk = posKey(x, y);
        auto it = m_groundNameCache.constFind(pk);
        if (it != m_groundNameCache.cend()) return it.value();
        const int sid = groundServerIdAt(currentFloorTileAt(x, y));
        QString n = sid > 0 ? m_brushController.store()->groundBrushForServerId(sid) : QString();
        m_groundNameCache.insert(pk, n);
        return n;
    }
    const int sid = groundServerIdAt(currentFloorTileAt(x, y));
    return sid > 0 ? m_brushController.store()->groundBrushForServerId(sid) : QString();
}

void MapView::recomputeBordersAt(int x, int y)
{
    if (!m_brushController.store() || !m_otbm) return;

    if (!m_brushController.automagic()) return;

    const QString center = groundBrushNameAt(x, y);

    static const int dxs[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dys[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    QStringList neighbours;
    neighbours.reserve(8);
    for (int i = 0; i < 8; ++i)
        neighbours << groundBrushNameAt(x + dxs[i], y + dys[i]);

    bool uniform = true;
    for (const QString &n : neighbours)
        if (n != center) { uniform = false; break; }

    QVector<int> newBorders;
    if (!uniform) {
        newBorders = m_brushController.store()->computeBorderItems(center, neighbours);

        std::reverse(newBorders.begin(), newBorders.end());
    }

    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldBorders;
    if (tile) {
        for (const OtbmMapItem &it : tile->items)
            if (m_brushController.store()->isManagedBorderItem(it.server_id))
                oldBorders.push_back(it.server_id);
    }

    {
        std::vector<uint16_t> b;
        b.reserve(newBorders.size());
        for (int id : newBorders) b.push_back(static_cast<uint16_t>(id));
        if (oldBorders == b) return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldBorders.empty())
        m_otbm->removeItemsById(x, y, m_navigationController.floor(), oldBorders);

    const OtbmTile *t2 = currentFloorTileAt(x, y);
    int base = (t2 && !t2->items.empty() && itemCategory(t2->items[0].server_id) == 0) ? 1 : 0;
    for (int k = 0; k < newBorders.size(); ++k) {
        const int id = newBorders[k];
        if (id <= 0) continue;
        ensureItemSprites(static_cast<uint16_t>(id));
        m_otbm->placeItem(x, y, m_navigationController.floor(), static_cast<uint16_t>(id), base + k, false, false);
    }
    onTileEdited(x, y, m_navigationController.floor());
}

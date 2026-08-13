#include "palettefilter.h"
#include "otbreader.h"

#include <utility>
#include <limits>

PaletteFilter::PaletteFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void PaletteFilter::setMode(const QString &m)
{
    if (m_mode == m) return;
    beginFilterChange();
    m_mode = m;
    endFilterChange(Direction::Rows);
    emit modeChanged();
}

void PaletteFilter::setSearchText(const QString &t)
{
    if (m_search == t) return;
    beginFilterChange();
    m_search = t;
    endFilterChange(Direction::Rows);
    emit searchTextChanged();
}

void PaletteFilter::setIds(const QVariantList &ids)
{
    QSet<int> newIds;
    newIds.reserve(ids.size());
    for (const QVariant &v : ids) newIds.insert(v.toInt());

    const bool idsChanged = m_ids != newIds;
    const bool modeWasChanged = m_mode != QLatin1String("ids");
    const bool orderWasSet = !m_order.isEmpty();
    if (!idsChanged && !modeWasChanged && !orderWasSet) return;

    beginFilterChange();
    m_ids = std::move(newIds);
    m_order.clear();
    if (modeWasChanged) {
        m_mode = QStringLiteral("ids");
    }
    endFilterChange(Direction::Rows);
    sort(-1);
    if (modeWasChanged) emit modeChanged();
}

void PaletteFilter::setOrderedIds(const QVariantList &ids)
{
    QSet<int> newIds;
    QHash<int, int> newOrder;
    newIds.reserve(ids.size());
    newOrder.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
        const int serverId = ids[i].toInt();
        newIds.insert(serverId);
        if (!newOrder.contains(serverId))
            newOrder.insert(serverId, i);
    }

    const bool modeWasChanged = m_mode != QLatin1String("ids");
    if (m_ids == newIds && m_order == newOrder && !modeWasChanged) return;

    beginFilterChange();
    m_ids = std::move(newIds);
    m_order = std::move(newOrder);
    if (modeWasChanged)
        m_mode = QStringLiteral("ids");
    endFilterChange(Direction::Rows);
    sort(0);
    if (modeWasChanged) emit modeChanged();
}

int PaletteFilter::rowForServerId(int serverId) const
{
    auto *otb = qobject_cast<OtbReader *>(sourceModel());
    if (!otb) return -1;
    const int srcRow = otb->rowForServerId(serverId);
    if (srcRow < 0) return -1;
    const QModelIndex proxyIdx = mapFromSource(otb->index(srcRow, 0));
    return proxyIdx.isValid() ? proxyIdx.row() : -1;
}

bool PaletteFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *src = sourceModel();
    if (!src) return false;
    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (m_mode == QLatin1String("ids")) {
        const int sid = idx.data(OtbReader::ServerIdRole).toInt();
        if (!m_ids.contains(sid)) return false;
    }

    if (!m_search.isEmpty()) {
        const QString name = idx.data(OtbReader::NameRole).toString();
        const QString sid = idx.data(OtbReader::ServerIdRole).toString();
        if (!name.contains(m_search, Qt::CaseInsensitive) && !sid.startsWith(m_search))
            return false;
    }
    return true;
}

bool PaletteFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_order.isEmpty())
        return QSortFilterProxyModel::lessThan(left, right);
    const int leftId = left.data(OtbReader::ServerIdRole).toInt();
    const int rightId = right.data(OtbReader::ServerIdRole).toInt();
    return m_order.value(leftId, std::numeric_limits<int>::max())
           < m_order.value(rightId, std::numeric_limits<int>::max());
}

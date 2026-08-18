#include "creaturefilter.h"

#include "creaturestore.h"

CreatureFilter::CreatureFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void CreatureFilter::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    beginFilterChange();
    m_searchText = text;
    endFilterChange(Direction::Rows);
    emit searchTextChanged();
}

void CreatureFilter::setTypeFilter(const QString &type)
{
    const QString normalized = type == QLatin1String("npc")
        || type == QLatin1String("monster") ? type : QStringLiteral("all");
    if (m_typeFilter == normalized) return;
    beginFilterChange();
    m_typeFilter = normalized;
    endFilterChange(Direction::Rows);
    emit typeFilterChanged();
}

int CreatureFilter::rowForCreature(const QString &name, bool isNpc) const
{
    const auto *store = qobject_cast<const CreatureStore *>(sourceModel());
    if (!store) return -1;
    const int sourceRow = store->rowForCreature(name, isNpc);
    if (sourceRow < 0) return -1;
    const QModelIndex proxyIndex = mapFromSource(store->index(sourceRow, 0));
    return proxyIndex.isValid() ? proxyIndex.row() : -1;
}

int CreatureFilter::sourceRow(int proxyRow) const
{
    if (proxyRow < 0 || proxyRow >= rowCount()) return -1;
    const QModelIndex sourceIndex = mapToSource(index(proxyRow, 0));
    return sourceIndex.isValid() ? sourceIndex.row() : -1;
}

bool CreatureFilter::filterAcceptsRow(int sourceRow,
                                      const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *source = sourceModel();
    if (!source) return false;
    const QModelIndex index = source->index(sourceRow, 0, sourceParent);
    const bool isNpc = index.data(CreatureStore::IsNpcRole).toBool();

    if (m_typeFilter == QLatin1String("npc") && !isNpc) return false;
    if (m_typeFilter == QLatin1String("monster") && isNpc) return false;

    return m_searchText.isEmpty()
        || index.data(CreatureStore::NameRole).toString().contains(
            m_searchText, Qt::CaseInsensitive);
}

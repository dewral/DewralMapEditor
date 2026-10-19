#include "rawitemmodel.h"
#include "datreader.h"
#include "otbreader.h"

#include <QSet>
#include <algorithm>

RawItemModel::RawItemModel(QObject *parent) : QAbstractListModel(parent) {}

void RawItemModel::setSources(OtbReader *otb, DatReader *dat)
{
    if (m_otb == otb && m_dat == dat) return;
    if (m_otb) disconnect(m_otb, nullptr, this, nullptr);
    if (m_dat) disconnect(m_dat, nullptr, this, nullptr);
    m_otb = otb;
    m_dat = dat;
    if (otb) {
        connect(otb, &QAbstractItemModel::modelReset, this, &RawItemModel::rebuild);
        connect(otb, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
            if (!m_rows.isEmpty()) emit dataChanged(index(0), index(m_rows.size() - 1), roles);
        });
    }
    if (dat) connect(dat, &QAbstractItemModel::modelReset, this, &RawItemModel::rebuild);
    rebuild();
}

void RawItemModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    m_serverRows.clear();
    QSet<int> mappedClients;
    if (m_otb && m_otb->isLoaded()) {
        for (int row = 0; row < m_otb->rowCount(); ++row) {
            const QModelIndex source = m_otb->index(row);
            const int serverId = source.data(OtbReader::ServerIdRole).toInt();
            const int clientId = source.data(OtbReader::ClientIdRole).toInt();
            m_rows.append({row, serverId, clientId});
            mappedClients.insert(clientId);
        }
        if (m_dat && m_dat->isLoaded()) {
            for (const auto &item : m_dat->items()) {
                if (!mappedClients.contains(item.id)) m_rows.append({-1, 0, item.id});
            }
        }
        // Insert unmapped client entries alongside their surrounding artwork.
        std::stable_sort(m_rows.begin(), m_rows.end(), [](const Row &a, const Row &b) {
            return a.clientId < b.clientId;
        });
        for (int row = 0; row < m_rows.size(); ++row) {
            if (m_rows[row].serverId > 0) m_serverRows.insert(m_rows[row].serverId, row);
        }
    }
    endResetModel();
}

int RawItemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int RawItemModel::rowForServerId(int serverId) const
{
    return m_serverRows.value(serverId, -1);
}

QHash<int, QByteArray> RawItemModel::roleNames() const
{
    return m_otb ? m_otb->roleNames() : QHash<int, QByteArray>{};
}

QVariant RawItemModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() < 0 || idx.row() >= m_rows.size()) return {};
    const Row &row = m_rows[idx.row()];
    if (row.serverRow >= 0) return m_otb->data(m_otb->index(row.serverRow), role);
    const auto *item = m_dat ? m_dat->itemByClientId(row.clientId) : nullptr;
    switch (role) {
    case OtbReader::ItemIdRole:
    case OtbReader::ClientIdRole: return row.clientId;
    case OtbReader::ServerIdRole: return 0;
    case OtbReader::NameRole: return QString();
    case OtbReader::GroupRole: return int(OtbItemGroup::None);
    case OtbReader::SpriteIdsRole: {
        QVariantList sprites;
        if (item) for (uint32_t id : item->sprite_ids) sprites.append(id);
        return sprites;
    }
    case OtbReader::ItemWidthRole: return item ? item->width : 1;
    case OtbReader::ItemHeightRole: return item ? item->height : 1;
    case OtbReader::LayersRole: return item ? item->layers : 1;
    case OtbReader::IsRenderableRole: return item && !item->sprite_ids.empty();
    case OtbReader::IsGroundRole: return item && item->has(ClientProperty::Ground);
    case OtbReader::IsStackableRole: return item && item->has(ClientProperty::Stackable);
    case OtbReader::IsContainerRole: return item && item->has(ClientProperty::Container);
    case OtbReader::IsUnpassableRole: return item && item->has(ClientProperty::Solid);
    default: return {};
    }
}

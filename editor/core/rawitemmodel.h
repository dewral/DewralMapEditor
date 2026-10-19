#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class DatReader;
class OtbReader;

// Palette catalogue only: client items without an OTB mapping never acquire
// a synthetic server ID or become part of the map's item type definitions.
class RawItemModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit RawItemModel(QObject *parent = nullptr);
    void setSources(OtbReader *otb, DatReader *dat);
    OtbReader *itemReader() const { return m_otb; }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int rowForServerId(int serverId) const;

private:
    void rebuild();
    struct Row {
        int serverRow = -1;
        int serverId = 0;
        int clientId = 0;
    };
    OtbReader *m_otb = nullptr;
    DatReader *m_dat = nullptr;
    QVector<Row> m_rows;
    QHash<int, int> m_serverRows;
};

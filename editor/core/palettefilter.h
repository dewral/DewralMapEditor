#ifndef PALETTEFILTER_H
#define PALETTEFILTER_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QHash>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class PaletteFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PaletteFilter)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)

public:
    explicit PaletteFilter(QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    void setMode(const QString &m);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &t);

    Q_INVOKABLE void setIds(const QVariantList &ids);
    Q_INVOKABLE void setOrderedIds(const QVariantList &ids);

    Q_INVOKABLE int rowForServerId(int serverId) const;

signals:
    void modeChanged();
    void searchTextChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_mode = QStringLiteral("all");
    QString m_search;
    QSet<int> m_ids;
    QHash<int, int> m_order;
};

#endif

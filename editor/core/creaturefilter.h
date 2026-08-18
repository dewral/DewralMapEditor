#ifndef CREATUREFILTER_H
#define CREATUREFILTER_H

#include <QSortFilterProxyModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

class CreatureFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CreatureFilter)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText
               NOTIFY searchTextChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter
               NOTIFY typeFilterChanged)

public:
    explicit CreatureFilter(QObject *parent = nullptr);

    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    QString typeFilter() const { return m_typeFilter; }
    void setTypeFilter(const QString &type);

    Q_INVOKABLE int rowForCreature(const QString &name, bool isNpc) const;
    Q_INVOKABLE int sourceRow(int proxyRow) const;

signals:
    void searchTextChanged();
    void typeFilterChanged();

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex &sourceParent) const override;

private:
    QString m_searchText;
    QString m_typeFilter = QStringLiteral("all");
};

#endif

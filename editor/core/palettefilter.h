#ifndef PALETTEFILTER_H
#define PALETTEFILTER_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QHash>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class BrushStore;
class SprReader;
Q_MOC_INCLUDE("brushstore.h")
Q_MOC_INCLUDE("sprreader.h")

class PaletteFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PaletteFilter)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(BrushStore *brushStore READ brushStore WRITE setBrushStore NOTIFY brushStoreChanged)
    Q_PROPERTY(SprReader *sprReader READ sprReader WRITE setSprReader NOTIFY sprReaderChanged)
    Q_PROPERTY(bool hideInvisibleSprites READ hideInvisibleSprites WRITE setHideInvisibleSprites NOTIFY hideInvisibleSpritesChanged)
    Q_PROPERTY(bool hideNamedItems READ hideNamedItems WRITE setHideNamedItems NOTIFY hideNamedItemsChanged)

public:
    explicit PaletteFilter(QObject *parent = nullptr);

    QString mode() const { return m_mode; }
    void setMode(const QString &m);
    QString searchText() const { return m_search; }
    void setSearchText(const QString &t);
    BrushStore *brushStore() const { return m_brushStore; }
    void setBrushStore(BrushStore *store);
    SprReader *sprReader() const { return m_sprReader; }
    void setSprReader(SprReader *reader);
    bool hideInvisibleSprites() const { return m_hideInvisibleSprites; }
    void setHideInvisibleSprites(bool hide);
    bool hideNamedItems() const { return m_hideNamedItems; }
    void setHideNamedItems(bool hide);
    Q_INVOKABLE bool itemHasVisibleSprite(int serverId) const;
    Q_INVOKABLE bool doodadHasVisibleSprite(const QString &name) const;

    Q_INVOKABLE void setIds(const QVariantList &ids);
    Q_INVOKABLE void setOrderedIds(const QVariantList &ids);

    Q_INVOKABLE int rowForServerId(int serverId) const;
    Q_INVOKABLE int serverIdAtRow(int row) const;

signals:
    void modeChanged();
    void searchTextChanged();
    void brushStoreChanged();
    void sprReaderChanged();
    void hideInvisibleSpritesChanged();
    void hideNamedItemsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_mode = QStringLiteral("all");
    QString m_search;
    QSet<int> m_ids;
    QHash<int, int> m_order;
    BrushStore *m_brushStore = nullptr;
    SprReader *m_sprReader = nullptr;
    bool m_hideInvisibleSprites = false;
    bool m_hideNamedItems = false;
};

#endif

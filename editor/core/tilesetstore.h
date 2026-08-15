#ifndef TILESETSTORE_H
#define TILESETSTORE_H

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class TilesetStore : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int revision READ revision NOTIFY tilesetsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit TilesetStore(QObject *parent = nullptr);

    int revision() const { return m_revision; }
    QString errorString() const { return m_errorString; }

    Q_INVOKABLE bool loadForVersion(int clientVersion);

    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QStringList namesFor(const QString &category) const;

    Q_INVOKABLE QVariantList itemsFor(const QString &category, const QString &name) const;
    Q_INVOKABLE QString tilesetForItem(const QString &category, int serverId) const;

    Q_INVOKABLE bool isCustomOnly(const QString &category, const QString &name) const;

    Q_INVOKABLE bool newTileset(const QString &category, const QString &name);

    Q_INVOKABLE bool renameTileset(const QString &category, const QString &name,
                                   const QString &newName);

    Q_INVOKABLE bool deleteTileset(const QString &category, const QString &name);

    Q_INVOKABLE bool addItem(const QString &category, const QString &name, int serverId);
    Q_INVOKABLE bool addItems(const QString &category, const QString &name,
                              const QVariantList &serverIds);

    Q_INVOKABLE bool removeItem(const QString &category, const QString &name, int serverId);

signals:
    void tilesetsChanged();
    void errorStringChanged();

private:
    static bool loadJsonInto(const QString &path,
                              QHash<QString, QStringList> &names,
                              QHash<QString, QHash<QString, QVariantList>> &items);
    bool saveJson();
    void setErrorString(const QString &message);
    void bump() { ++m_revision; emit tilesetsChanged(); }

    int m_revision = 0;
    QString m_path;
    QString m_errorString;

    QHash<QString, QStringList> m_names;
    QHash<QString, QHash<QString, QVariantList>> m_items;
};

#endif

#ifndef CREATURESTORE_H
#define CREATURESTORE_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class CreatureStore : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY countChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    struct CreatureType {
        QString name;
        bool isNpc = false;
        int lookType = 0;
        int lookItem = 0;
        int lookHead = 0, lookBody = 0, lookLegs = 0, lookFeet = 0;
    };

    enum Roles {
        NameRole = Qt::UserRole + 1,
        IsNpcRole,
        LookTypeRole,
        LookItemRole,
    };

    explicit CreatureStore(QObject *parent = nullptr);

    Q_INVOKABLE bool loadForVersion(int version);

    Q_INVOKABLE bool loadForDir(const QString &dirName);

    int count() const { return static_cast<int>(m_creatures.size()); }
    bool hasData() const { return !m_creatures.isEmpty(); }
    QString errorString() const { return m_errorString; }

    const CreatureType *byName(const QString &name) const;
    const QVector<CreatureType> &creatureTypes() const { return m_creatures; }
    Q_INVOKABLE int rowForName(const QString &name) const;
    Q_INVOKABLE QVariantMap creatureAt(int row) const;
    Q_INVOKABLE bool saveCreature(const QString &originalName,
                                  const QString &name, bool isNpc,
                                  int lookType, int lookItem,
                                  int lookHead, int lookBody,
                                  int lookLegs, int lookFeet);
    Q_INVOKABLE bool removeCreature(const QString &name);
    Q_INVOKABLE QVariantMap importOtFile(const QString &pathOrUrl);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();
    void errorStringChanged();

private:
    bool loadFile(const QString &path);
    bool saveFile();
    void setErrorString(const QString &error);

    QVector<CreatureType> m_creatures;
    QString m_path;
    QString m_errorString;
};

#endif

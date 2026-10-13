#pragma once
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class TileMapGenProfiles : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
public:
    explicit TileMapGenProfiles(QObject *parent = nullptr, const QString &directory = {});
    QVariantList profiles() const { return m_profiles; }
    Q_INVOKABLE QVariantMap load(const QString &id) const;
    Q_INVOKABLE QVariantMap save(const QVariantMap &profile, const QString &id = {});
    Q_INVOKABLE QVariantMap remove(const QString &id);
    static QString validateRecipe(const QVariantMap &options);
signals:
    void profilesChanged();
private:
    static QString validate(const QVariantMap &profile);
    bool persist(const QVariantList &profiles, QString &error) const;
    QString m_directory, m_error;
    QVariantList m_profiles;
};

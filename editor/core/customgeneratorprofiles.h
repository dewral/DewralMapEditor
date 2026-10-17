#pragma once
#include <QObject>
#include <QVariantMap>
#include <QSet>
#include <QUrl>

class CustomGeneratorProfiles : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
public:
    explicit CustomGeneratorProfiles(QObject *parent=nullptr, const QString &directory={});
    QVariantList profiles() const;
    Q_INVOKABLE QVariantMap load(const QString &id) const;
    Q_INVOKABLE QVariantMap save(const QVariantMap &profile, const QString &id={});
    Q_INVOKABLE QVariantMap remove(const QString &id);
    Q_INVOKABLE QVariantMap importProfile(const QUrl &url);
    Q_INVOKABLE QVariantMap exportProfile(const QVariantMap &profile, const QUrl &url) const;
    static QString validate(const QVariantMap &profile, const QVariantMap *itemInfo=nullptr);
    static QStringList roles();
    static QSet<int> itemIds(const QVariantMap &profile);
signals:
    void profilesChanged();
private:
    QString m_directory;
    QString m_error;
    QVariantList m_profiles;
    bool persist(const QVariantList &profiles, QString &error) const;
};

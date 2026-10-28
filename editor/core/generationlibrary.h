#pragma once
#include "mapgeneration.h"
#include <QFutureWatcher>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class GenerationLibrary : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
  public:
    explicit GenerationLibrary(QObject *parent = nullptr, const QString &directory = {});
    ~GenerationLibrary() override;
    QVariantList entries() const;
    bool busy() const
    {
        return m_busy;
    }
    int progress() const
    {
        return m_progress;
    }
    QString status() const
    {
        return m_status;
    }
    QVariantMap result() const
    {
        return m_result;
    }
    Q_INVOKABLE bool selectProfile(const QString &profile);
    Q_INVOKABLE QVariantMap load(const QString &id) const;
    Q_INVOKABLE QVariantMap capture(const QString &name, const QVariantMap &snapshot);
    Q_INVOKABLE QVariantMap saveBlueprint(const QString &name, const QVariantMap &blueprint,
                                          const QString &id = {});
    Q_INVOKABLE QVariantMap remove(const QString &id);
    Q_INVOKABLE bool learnStyle(const QString &name, const QVariantList &exampleIds,
                                const QVariantList &prefabs, const QVariantMap &parameters);
    Q_INVOKABLE bool generate(const QString &styleId, const QVariantMap &blueprint,
                              const QVariantMap &context);
    Q_INVOKABLE bool importExample(const QString &name, const QString &path, const QVariantMap &region,
                                   const QVariantMap &materials);
    Q_INVOKABLE QVariantMap saveRecipe(const QString &name, const QVariantMap &recipe);
    Q_INVOKABLE QVariantMap attachRecipe(const QString &styleId, const QString &recipeId,
                                         const QVariantList &prefabs);
    Q_INVOKABLE QVariantMap reviseStyle(const QString &styleId, const QVariantMap &roles);
    Q_INVOKABLE QVariantMap exportPackage(const QString &path) const;
    Q_INVOKABLE QVariantMap importPackage(const QString &path);
    Q_INVOKABLE void cancel();
  signals:
    void entriesChanged();
    void stateChanged();
    void resultChanged();

  private:
    bool start(const QString &operation, std::function<QVariantMap(const MapGeneration::Progress &)> work);
    QVariantMap save(QVariantMap entry, const QString &id = {});
    bool persist(const QVariantList &data, QString &error) const;
    QString m_root, m_profile, m_status, m_operation;
    QVariantList m_entries;
    QVariantMap m_result;
    MapGeneration::Cancel m_cancel;
    bool m_busy = false;
    int m_progress = 0;
    QFutureWatcher<QVariantMap> m_worker;
};

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QFutureWatcher>
#include <atomic>
#include <memory>

// Each run builds a detached patch. The GUI commits it only after validation.
class LuaScriptEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit LuaScriptEngine(QObject *parent = nullptr);
    ~LuaScriptEngine() override;
    bool busy() const { return m_busy; }
    Q_INVOKABLE void run(const QString &path, const QString &mode, int seed);
    Q_INVOKABLE void runProfile(const QString &directory, const QVariantMap &profile,
                                const QVariantMap &itemInfo);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QStringList generatorScripts(const QString &directory) const;
    static QVariantMap execute(const QString &path, const QString &mode, int seed,
                               const std::shared_ptr<std::atomic_bool> &cancelled,
                               int timeoutMs = 120000, const QVariantMap &profile = {});
signals:
    void busyChanged();
    void finished(const QVariantMap &result);
private:
    bool m_busy = false;
    QFutureWatcher<QVariantMap> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};

#ifndef CLIENTLOADSERVICE_H
#define CLIENTLOADSERVICE_H
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include <memory>
class Backend;
class MapView;
struct ClientPreparedState;

class ClientLoadService final : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool busy READ busy NOTIFY progressChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY progressChanged)
public:
    explicit ClientLoadService(Backend *backend);
    ~ClientLoadService() override;
    bool busy() const { return m_busy; }
    int progress() const;
    QString stage() const;
    Q_INVOKABLE quint64 request(const QString &key, int version, const QString &folder, bool custom);
    Q_INVOKABLE void cancel();
    void setMapView(MapView *view);
    Q_INVOKABLE void attachViewport(QObject *view);
    static QString fingerprint(const QString &dat, const QString &spr, const QString &otb,
                               int version, bool extended, bool alpha, bool durations, bool groups);
signals:
    void progressChanged();
    void finished(quint64 requestId, const QString &key, const QString &fingerprint,
                  bool success, const QString &error);
private:
    struct Job {
        std::atomic_bool cancelled{false};
        std::atomic_int progress{0};
        std::atomic_int stage{0};
    };
    static std::shared_ptr<ClientPreparedState> prepare(const QString &key, int version,
        const QString &folder, bool custom, const QString &cachedKey,
        const QString &cachedFingerprint, const std::shared_ptr<Job> &job);
    void publish(quint64 id, const std::shared_ptr<ClientPreparedState> &state);
    void complete(quint64 id, const std::shared_ptr<ClientPreparedState> &state, const QString &error = {});
    Backend *m_backend;
    QPointer<MapView> m_view;
    QTimer m_progressTimer;
    std::shared_ptr<Job> m_job;
    quint64 m_generation = 0;
    bool m_busy = false;
    QString m_key;
    QString m_fingerprint;
};
#endif

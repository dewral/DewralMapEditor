#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QSet>
#include <QHash>
#include <QtQml/qqmlregistration.h>
class DocumentManager;
class OtbmReader;

class ActivityService final : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY changed)
    Q_PROPERTY(QString accountName READ accountName NOTIFY changed)
    Q_PROPERTY(bool linked READ linked NOTIFY changed)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY changed)
    Q_PROPERTY(QVariantList maps READ maps NOTIFY changed)
    Q_PROPERTY(QString currentMapId READ currentMapId NOTIFY changed)
    Q_PROPERTY(QString currentMapName READ currentMapName NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit ActivityService(DocumentManager *documents, QObject *parent=nullptr);
    ~ActivityService() override;
    QString status() const { return m_status; }
    QString errorString() const { return m_error.isEmpty() && m_blocked > 0 ? QStringLiteral("Some queued events are blocked by server permissions. Refresh maps after restoring access.") : m_error; }
    QString serverUrl() const { return m_server; }
    QString accountName() const { return m_accountName; }
    bool linked() const { return !m_token.isEmpty(); }
    int pendingCount() const { return m_pending; }
    QVariantList maps() const { return m_maps; }
    QString currentMapId() const { return m_mapId; }
    QString currentMapName() const;
    bool busy() const { return m_busy; }
    Q_INVOKABLE void signIn(const QString &url, const QString &email, const QString &password, const QString &deviceName);
    Q_INVOKABLE void refreshMaps();
    Q_INVOKABLE void disconnectDevice();
signals:
    void changed();
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    friend class ActivityServiceTest;
    void switchDocument();
    void checkpoint();
    void enqueue(QJsonObject event);
    void record(const QString &kind, qint64 tiles=0);
    void endSession();
    void upload();
    QString detectedMap(OtbmReader *document, bool *ambiguous=nullptr) const;
    QString utc(qint64 elapsed) const;
    QJsonObject base(const QString &kind);
    void fail(const QString &error);
    DocumentManager *m_documents;
    QPointer<OtbmReader> m_current;
    QSet<OtbmReader *> m_observed;
    QHash<OtbmReader *, QPair<QString,QString>> m_lastSessions;
    QElapsedTimer m_clock;
    qint64 m_anchor=0, m_lastInput=0, m_checkpoint=0, m_seq=0;
    QString m_server,m_token,m_device,m_session,m_mapId,m_accountName,m_status=QStringLiteral("Not connected"),m_error;
    QVariantList m_maps;
    int m_pending=0,m_blocked=0;
    bool m_busy=false,m_uploading=false;
    QTimer m_tick,m_sync;
    QNetworkAccessManager m_network;
    QThread m_storageThread;
    QObject *m_storage=nullptr;
    QString m_connection;
};

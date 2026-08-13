#ifndef UPDATESERVICE_H
#define UPDATESERVICE_H

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <functional>

class DocumentManager;
class QNetworkReply;

class UpdateService final : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString currentCommit READ currentCommit CONSTANT)
    Q_PROPERTY(QString currentChannel READ currentChannel CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateChanged)
    Q_PROPERTY(QString latestCommit READ latestCommit NOTIFY updateChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY updateChanged)
    Q_PROPERTY(QUrl releasePageUrl READ releasePageUrl NOTIFY updateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

public:
    explicit UpdateService(DocumentManager *documents, QObject *parent = nullptr);

    QString currentVersion() const;
    QString currentCommit() const;
    QString currentChannel() const;
    QString state() const { return m_state; }
    QString latestVersion() const { return m_latestVersion; }
    QString latestCommit() const { return m_latestCommit; }
    QString releaseNotes() const { return m_releaseNotes; }
    QUrl releasePageUrl() const { return m_releasePageUrl; }
    bool updateAvailable() const { return m_updateAvailable; }
    bool busy() const;
    qreal downloadProgress() const { return m_downloadProgress; }
    QString errorString() const { return m_errorString; }

    Q_INVOKABLE void checkForUpdates(const QString &channel, bool silent = false);
    Q_INVOKABLE void downloadAndInstall();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openReleasePage() const;

signals:
    void stateChanged();
    void updateChanged();
    void downloadProgressChanged();
    void interactionRequested();

private:
    void resetRelease();
    void requestRelease(const QString &channel);
    void processRelease(const QByteArray &payload);
    void requestManifest(const QUrl &url, const QByteArray &fallbackRelease);
    void processManifest(const QByteArray &payload, const QByteArray &fallbackRelease);
    void applyReleaseData(const QString &version, const QString &commit,
                          const QString &notes, const QUrl &pageUrl,
                          const QUrl &downloadUrl, const QString &sha256,
                          qint64 size);
    void startRequest(const QUrl &url, const std::function<void(QByteArray)> &handler);
    void setState(const QString &state, const QString &error = {});
    void fail(const QString &message);
    bool isNewer(const QString &version, const QString &commit) const;
    bool launchUpdater(const QString &archivePath);

    DocumentManager *m_documents = nullptr;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QFile m_downloadFile;
    QCryptographicHash m_downloadHash{QCryptographicHash::Sha256};
    QString m_requestedChannel = QStringLiteral("stable");
    QString m_state = QStringLiteral("idle");
    QString m_errorString;
    QString m_latestVersion;
    QString m_latestCommit;
    QString m_releaseNotes;
    QUrl m_releasePageUrl;
    QUrl m_downloadUrl;
    QString m_expectedSha256;
    qint64 m_expectedSize = -1;
    qreal m_downloadProgress = 0.0;
    bool m_updateAvailable = false;
    bool m_silent = false;
    bool m_cancelled = false;
};

#endif

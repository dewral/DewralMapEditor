#pragma once

#include <QObject>
#include <QProcess>

// Runs TileMapGen's local CLI; no shell or web server is needed.
class TileMapGenProcess : public QObject {
    Q_OBJECT
public:
    explicit TileMapGenProcess(QObject *parent = nullptr);
    ~TileMapGenProcess() override;
    static QString programForDirectory(const QString &directory);
    void start(const QString &directory, const QString &request,
               const QString &pack, const QString &output);
    void cancel();
    void shutdown();
signals:
    void progressChanged(int progress, const QString &stage);
    void finished(bool success, const QString &error);
private:
    void readOutput();
    void complete(bool success, const QString &error);
    QProcess m_process;
    QByteArray m_pendingOutput, m_errors;
    bool m_running = false, m_cancelled = false;
};

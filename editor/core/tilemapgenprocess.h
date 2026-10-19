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
    static QString errorSummary(const QString &error);
    void start(const QString &directory, const QString &request,
               const QString &pack, const QString &output);
    void startCommand(const QString &directory, const QStringList &arguments, const QByteArray &input = {});
    QByteArray output() const { return m_output; }
    bool running() const { return m_running; }
    void cancel();
    void shutdown();
signals:
    void progressChanged(int progress, const QString &stage);
    void finished(bool success, const QString &error);
private:
    void readOutput();
    void complete(bool success, const QString &error);
    void run(const QString &directory, const QStringList &arguments, const QByteArray &input, bool capture);
    QProcess m_process;
    QByteArray m_pendingOutput, m_errors;
    QByteArray m_output;
    bool m_capture = false;
    bool m_running = false, m_cancelled = false;
};

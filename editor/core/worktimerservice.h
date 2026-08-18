#ifndef WORKTIMERSERVICE_H
#define WORKTIMERSERVICE_H

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class DocumentManager;
class OtbmReader;

class WorkTimerService final : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString state READ state NOTIFY timerChanged)
    Q_PROPERTY(bool active READ active NOTIFY timerChanged)
    Q_PROPERTY(bool running READ running NOTIFY timerChanged)
    Q_PROPERTY(QString taskName READ taskName NOTIFY timerChanged)
    Q_PROPERTY(QString mapName READ mapName NOTIFY timerChanged)
    Q_PROPERTY(QString elapsedText READ elapsedText NOTIFY timerChanged)
    Q_PROPERTY(qint64 elapsedSeconds READ elapsedSeconds NOTIFY timerChanged)
    Q_PROPERTY(QString taskElapsedText READ taskElapsedText NOTIFY timerChanged)
    Q_PROPERTY(qint64 taskElapsedSeconds READ taskElapsedSeconds NOTIFY timerChanged)
    Q_PROPERTY(qint64 operationCount READ operationCount NOTIFY timerChanged)
    Q_PROPERTY(qint64 changedTileCount READ changedTileCount NOTIFY timerChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QVariantList taskTimers READ taskTimers NOTIFY timerChanged)
    Q_PROPERTY(QString todayText READ todayText NOTIFY statisticsChanged)
    Q_PROPERTY(QString weekText READ weekText NOTIFY statisticsChanged)
    Q_PROPERTY(QString totalText READ totalText NOTIFY statisticsChanged)
    Q_PROPERTY(QString currentMapText READ currentMapText NOTIFY statisticsChanged)
    Q_PROPERTY(int idlePauseMinutes READ idlePauseMinutes WRITE setIdlePauseMinutes NOTIFY optionsChanged)
    Q_PROPERTY(int breakReminderMinutes READ breakReminderMinutes WRITE setBreakReminderMinutes NOTIFY optionsChanged)
    Q_PROPERTY(bool pomodoroEnabled READ pomodoroEnabled WRITE setPomodoroEnabled NOTIFY optionsChanged)
    Q_PROPERTY(int pomodoroWorkMinutes READ pomodoroWorkMinutes WRITE setPomodoroWorkMinutes NOTIFY optionsChanged)
    Q_PROPERTY(int pomodoroBreakMinutes READ pomodoroBreakMinutes WRITE setPomodoroBreakMinutes NOTIFY optionsChanged)
    Q_PROPERTY(bool checkpointEnabled READ checkpointEnabled WRITE setCheckpointEnabled NOTIFY optionsChanged)
    Q_PROPERTY(bool pomodoroBreakActive READ pomodoroBreakActive NOTIFY timerChanged)
    Q_PROPERTY(int pomodoroBreakRemaining READ pomodoroBreakRemaining NOTIFY timerChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    explicit WorkTimerService(DocumentManager *documents, QObject *parent = nullptr);
    ~WorkTimerService() override;

    QString state() const { return m_state; }
    bool active() const { return m_state != QStringLiteral("stopped"); }
    bool running() const { return m_state == QStringLiteral("running"); }
    QString taskName() const { return m_active.value(QStringLiteral("task")).toString(); }
    QString mapName() const { return m_active.value(QStringLiteral("mapName")).toString(); }
    qint64 elapsedSeconds() const;
    QString elapsedText() const;
    qint64 taskElapsedSeconds() const;
    QString taskElapsedText() const;
    qint64 operationCount() const;
    qint64 changedTileCount() const;
    QVariantList history() const;
    QVariantList taskTimers() const;
    QString todayText() const;
    QString weekText() const;
    QString totalText() const;
    QString currentMapText() const;
    int idlePauseMinutes() const { return m_idlePauseMinutes; }
    int breakReminderMinutes() const { return m_breakReminderMinutes; }
    bool pomodoroEnabled() const { return m_pomodoroEnabled; }
    int pomodoroWorkMinutes() const { return m_pomodoroWorkMinutes; }
    int pomodoroBreakMinutes() const { return m_pomodoroBreakMinutes; }
    bool checkpointEnabled() const { return m_checkpointEnabled; }
    bool pomodoroBreakActive() const { return m_pomodoroBreakActive; }
    int pomodoroBreakRemaining() const;
    QString errorString() const { return m_errorString; }

    void setIdlePauseMinutes(int minutes);
    void setBreakReminderMinutes(int minutes);
    void setPomodoroEnabled(bool enabled);
    void setPomodoroWorkMinutes(int minutes);
    void setPomodoroBreakMinutes(int minutes);
    void setCheckpointEnabled(bool enabled);

    Q_INVOKABLE bool startSession(const QString &task);
    Q_INVOKABLE bool switchTask(const QString &task);
    Q_INVOKABLE void pauseSession();
    Q_INVOKABLE bool resumeSession();
    Q_INVOKABLE bool finishSession(const QString &note = {});
    Q_INVOKABLE bool updateSession(const QString &id, const QString &task,
                                   const QString &note, qint64 durationSeconds);
    Q_INVOKABLE bool removeSession(const QString &id);
    Q_INVOKABLE bool removeTaskTimer(const QString &task);
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);
    Q_INVOKABLE bool exportJson(const QUrl &fileUrl);
    Q_INVOKABLE QString formatDuration(qint64 seconds) const;

signals:
    void timerChanged();
    void historyChanged();
    void statisticsChanged();
    void optionsChanged();
    void errorChanged();
    void reminderRequested(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QString storagePath() const;
    void load();
    bool save();
    void tick();
    void pauseAt(qint64 timestampMs, const QString &reason = {});
    void captureMetrics();
    void resetMetricBaseline();
    void clearError();
    bool fail(const QString &message);
    QString currentMapKey() const;
    QString currentMapNameValue() const;
    QJsonObject sessionView(const QJsonObject &session) const;
    qint64 statisticsSeconds(const QString &scope) const;
    static QString localPath(const QUrl &url);
    static QString csvCell(QString value);

    DocumentManager *m_documents = nullptr;
    QTimer m_timer;
    QList<QJsonObject> m_sessions;
    QJsonObject m_active;
    QString m_state = QStringLiteral("stopped");
    QString m_errorString;
    qint64 m_accumulatedMs = 0;
    qint64 m_resumeMs = 0;
    qint64 m_lastActivityMs = 0;
    qint64 m_baseOperations = 0;
    qint64 m_baseTiles = 0;
    qint64 m_sessionOperations = 0;
    qint64 m_sessionTiles = 0;
    qint64 m_nextBreakReminderMs = 0;
    qint64 m_nextPomodoroMs = 0;
    qint64 m_pomodoroBreakEndMs = 0;
    int m_idlePauseMinutes = 15;
    int m_breakReminderMinutes = 60;
    int m_pomodoroWorkMinutes = 50;
    int m_pomodoroBreakMinutes = 10;
    int m_saveTicks = 0;
    bool m_pomodoroEnabled = false;
    bool m_pomodoroBreakActive = false;
    bool m_checkpointEnabled = false;
};

#endif

#include "worktimerservice.h"

#include "documentmanager.h"
#include "otbmreader.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>

namespace {
constexpr auto kDuration = "durationMs";
constexpr auto kOperations = "operations";
constexpr auto kTiles = "changedTiles";
}

WorkTimerService::WorkTimerService(DocumentManager *documents, QObject *parent)
    : QObject(parent), m_documents(documents)
{
    QSettings settings;
    m_idlePauseMinutes = settings.value(QStringLiteral("workTimer/idlePauseMinutes"), 15).toInt();
    m_breakReminderMinutes = settings.value(QStringLiteral("workTimer/breakReminderMinutes"), 60).toInt();
    m_pomodoroEnabled = settings.value(QStringLiteral("workTimer/pomodoroEnabled"), false).toBool();
    m_pomodoroWorkMinutes = settings.value(QStringLiteral("workTimer/pomodoroWorkMinutes"), 50).toInt();
    m_pomodoroBreakMinutes = settings.value(QStringLiteral("workTimer/pomodoroBreakMinutes"), 10).toInt();
    m_checkpointEnabled = settings.value(QStringLiteral("workTimer/checkpointEnabled"), false).toBool();
    load();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &WorkTimerService::tick);
    m_timer.start();
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);
    connect(m_documents, &DocumentManager::currentChanged, this, [this] {
        if (running() && currentMapKey() != m_active.value(QStringLiteral("mapKey")).toString())
            pauseAt(QDateTime::currentMSecsSinceEpoch(), QStringLiteral("Session paused because the active map changed."));
        emit statisticsChanged();
    });
}

WorkTimerService::~WorkTimerService()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeEventFilter(this);
    if (running())
        pauseAt(QDateTime::currentMSecsSinceEpoch());
    save();
}

qint64 WorkTimerService::elapsedSeconds() const
{
    qint64 value = m_accumulatedMs;
    if (running())
        value += qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - m_resumeMs);
    return value / 1000;
}

QString WorkTimerService::formatDuration(qint64 seconds) const
{
    seconds = qMax<qint64>(0, seconds);
    const qint64 hours = seconds / 3600;
    const int minutes = static_cast<int>((seconds / 60) % 60);
    const int secs = static_cast<int>(seconds % 60);
    return QStringLiteral("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
}

QString WorkTimerService::elapsedText() const { return formatDuration(elapsedSeconds()); }

qint64 WorkTimerService::taskElapsedSeconds() const
{
    if (!active()) return 0;

    qint64 seconds = elapsedSeconds();
    const QString activeTask = taskName();
    for (const QJsonObject &session : m_sessions) {
        if (session.value(QStringLiteral("task")).toString() == activeTask)
            seconds += session.value(QLatin1String(kDuration)).toInteger() / 1000;
    }
    return seconds;
}

QString WorkTimerService::taskElapsedText() const
{
    return formatDuration(taskElapsedSeconds());
}

qint64 WorkTimerService::operationCount() const
{
    if (!running() || !m_documents->current()) return m_sessionOperations;
    return m_sessionOperations + qMax<qint64>(0, m_documents->current()->editOperationCount() - m_baseOperations);
}

qint64 WorkTimerService::changedTileCount() const
{
    if (!running() || !m_documents->current()) return m_sessionTiles;
    return m_sessionTiles + qMax<qint64>(0, m_documents->current()->changedTileCount() - m_baseTiles);
}

QVariantList WorkTimerService::history() const
{
    QVariantList result;
    for (auto it = m_sessions.crbegin(); it != m_sessions.crend(); ++it)
        result.push_back(sessionView(*it).toVariantMap());
    return result;
}

QVariantList WorkTimerService::taskTimers() const
{
    // A timer is a named task. Its total is the sum of all recorded work
    // segments, so switching tasks never loses the accumulated value.
    QMap<QString, QJsonObject> timers;
    for (const QJsonObject &session : m_sessions) {
        const QString name = session.value(QStringLiteral("task")).toString();
        if (name.isEmpty()) continue;
        QJsonObject timer = timers.value(name);
        timer.insert(QStringLiteral("name"), name);
        timer.insert(QStringLiteral("durationSeconds"),
                     timer.value(QStringLiteral("durationSeconds")).toInteger()
                         + session.value(QLatin1String(kDuration)).toInteger() / 1000);
        timer.insert(QStringLiteral("lastMap"), session.value(QStringLiteral("mapName")));
        timer.insert(QStringLiteral("lastStartedAt"), session.value(QStringLiteral("startedAt")));
        timers.insert(name, timer);
    }
    if (active()) {
        const QString name = taskName();
        QJsonObject timer = timers.value(name);
        timer.insert(QStringLiteral("name"), name);
        timer.insert(QStringLiteral("durationSeconds"),
                     timer.value(QStringLiteral("durationSeconds")).toInteger() + elapsedSeconds());
        timer.insert(QStringLiteral("lastMap"), mapName());
        timer.insert(QStringLiteral("lastStartedAt"), m_active.value(QStringLiteral("startedAt")));
        timers.insert(name, timer);
    }
    QVariantList result;
    for (auto it = timers.cbegin(); it != timers.cend(); ++it) {
        QJsonObject timer = it.value();
        timer.insert(QStringLiteral("durationText"),
                     formatDuration(timer.value(QStringLiteral("durationSeconds")).toInteger()));
        timer.insert(QStringLiteral("active"), active() && it.key() == taskName());
        timer.insert(QStringLiteral("running"), running() && it.key() == taskName());
        result.push_back(timer.toVariantMap());
    }
    return result;
}

QJsonObject WorkTimerService::sessionView(const QJsonObject &session) const
{
    QJsonObject value = session;
    value.insert(QStringLiteral("durationSeconds"), session.value(QLatin1String(kDuration)).toInteger() / 1000);
    value.insert(QStringLiteral("durationText"), formatDuration(session.value(QLatin1String(kDuration)).toInteger() / 1000));
    return value;
}

qint64 WorkTimerService::statisticsSeconds(const QString &scope) const
{
    const QDate today = QDate::currentDate();
    const QDate weekStart = today.addDays(1 - today.dayOfWeek());
    const QString mapKey = currentMapKey();
    const QDate rangeStartDate = scope == QStringLiteral("week") ? weekStart : today;
    const qint64 rangeStart = QDateTime(rangeStartDate, QTime(0, 0),
                                        QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    const qint64 rangeEnd = QDateTime(today.addDays(1), QTime(0, 0),
                                      QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    const auto segmentMilliseconds = [&](const QJsonObject &session,
                                         bool includeRunning) {
        qint64 value = 0;
        const QJsonArray segments = session.value(QStringLiteral("segments")).toArray();
        for (const QJsonValue &entry : segments) {
            const QJsonObject segment = entry.toObject();
            const qint64 start = QDateTime::fromString(
                segment.value(QStringLiteral("startedAt")).toString(),
                Qt::ISODate).toMSecsSinceEpoch();
            const qint64 end = QDateTime::fromString(
                segment.value(QStringLiteral("endedAt")).toString(),
                Qt::ISODate).toMSecsSinceEpoch();
            value += qMax<qint64>(0, qMin(end, rangeEnd) - qMax(start, rangeStart));
        }
        if (includeRunning && running())
            value += qMax<qint64>(0, qMin(QDateTime::currentMSecsSinceEpoch(), rangeEnd)
                                      - qMax(m_resumeMs, rangeStart));
        if (segments.isEmpty() && (!includeRunning || !running())) {
            const QDate date = QDateTime::fromString(
                session.value(QStringLiteral("startedAt")).toString(),
                Qt::ISODate).toLocalTime().date();
            if ((scope == QStringLiteral("today") && date == today)
                || (scope == QStringLiteral("week") && date >= weekStart && date <= today))
                value = includeRunning ? m_accumulatedMs
                                       : session.value(QLatin1String(kDuration)).toInteger();
        }
        return value;
    };
    qint64 milliseconds = 0;
    for (const QJsonObject &session : m_sessions) {
        if (scope == QStringLiteral("map") && session.value(QStringLiteral("mapKey")).toString() != mapKey) continue;
        milliseconds += (scope == QStringLiteral("today") || scope == QStringLiteral("week"))
            ? segmentMilliseconds(session, false)
            : session.value(QLatin1String(kDuration)).toInteger();
    }
    if (active()) {
        if (scope == QStringLiteral("today") || scope == QStringLiteral("week"))
            milliseconds += segmentMilliseconds(m_active, true);
        else if (scope == QStringLiteral("all")
                 || (scope == QStringLiteral("map")
                     && m_active.value(QStringLiteral("mapKey")).toString() == mapKey))
            milliseconds += elapsedSeconds() * 1000;
    }
    return milliseconds / 1000;
}

QString WorkTimerService::todayText() const { return formatDuration(statisticsSeconds(QStringLiteral("today"))); }
QString WorkTimerService::weekText() const { return formatDuration(statisticsSeconds(QStringLiteral("week"))); }
QString WorkTimerService::totalText() const { return formatDuration(statisticsSeconds(QStringLiteral("all"))); }
QString WorkTimerService::currentMapText() const { return formatDuration(statisticsSeconds(QStringLiteral("map"))); }

int WorkTimerService::pomodoroBreakRemaining() const
{
    if (!m_pomodoroBreakActive) return 0;
    return static_cast<int>(qMax<qint64>(0, m_pomodoroBreakEndMs - QDateTime::currentMSecsSinceEpoch()) / 1000);
}

void WorkTimerService::setIdlePauseMinutes(int value)
{
    value = qBound(0, value, 240);
    if (m_idlePauseMinutes == value) return;
    m_idlePauseMinutes = value;
    QSettings().setValue(QStringLiteral("workTimer/idlePauseMinutes"), value);
    emit optionsChanged();
}

void WorkTimerService::setBreakReminderMinutes(int value)
{
    value = qBound(0, value, 480);
    if (m_breakReminderMinutes == value) return;
    m_breakReminderMinutes = value;
    QSettings().setValue(QStringLiteral("workTimer/breakReminderMinutes"), value);
    m_nextBreakReminderMs = value > 0 ? m_accumulatedMs + value * 60000LL : 0;
    emit optionsChanged();
}

void WorkTimerService::setPomodoroEnabled(bool value)
{
    if (m_pomodoroEnabled == value) return;
    m_pomodoroEnabled = value;
    QSettings().setValue(QStringLiteral("workTimer/pomodoroEnabled"), value);
    m_nextPomodoroMs = value ? m_accumulatedMs + m_pomodoroWorkMinutes * 60000LL : 0;
    emit optionsChanged();
}

void WorkTimerService::setPomodoroWorkMinutes(int value)
{
    value = qBound(1, value, 180);
    if (m_pomodoroWorkMinutes == value) return;
    m_pomodoroWorkMinutes = value;
    QSettings().setValue(QStringLiteral("workTimer/pomodoroWorkMinutes"), value);
    emit optionsChanged();
}

void WorkTimerService::setPomodoroBreakMinutes(int value)
{
    value = qBound(1, value, 60);
    if (m_pomodoroBreakMinutes == value) return;
    m_pomodoroBreakMinutes = value;
    QSettings().setValue(QStringLiteral("workTimer/pomodoroBreakMinutes"), value);
    emit optionsChanged();
}

void WorkTimerService::setCheckpointEnabled(bool value)
{
    if (m_checkpointEnabled == value) return;
    m_checkpointEnabled = value;
    QSettings().setValue(QStringLiteral("workTimer/checkpointEnabled"), value);
    emit optionsChanged();
}

bool WorkTimerService::startSession(const QString &task)
{
    clearError();
    if (active()) return fail(QStringLiteral("Finish the current session first."));
    OtbmReader *map = m_documents->current();
    if (!map || !map->isLoaded()) return fail(QStringLiteral("Open a map before starting a session."));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_active = QJsonObject{{QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("task"), task.trimmed().isEmpty() ? QStringLiteral("Mapping session") : task.trimmed()},
        {QStringLiteral("mapKey"), currentMapKey()}, {QStringLiteral("mapName"), currentMapNameValue()},
        {QStringLiteral("mapPath"), map->filePath()}, {QStringLiteral("profile"), m_documents->currentProfileKey()},
        {QStringLiteral("startedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("note"), QString()}};
    m_accumulatedMs = 0; m_resumeMs = now; m_lastActivityMs = now;
    m_sessionOperations = 0; m_sessionTiles = 0;
    m_state = QStringLiteral("running");
    resetMetricBaseline();
    m_nextBreakReminderMs = m_breakReminderMinutes > 0 ? m_breakReminderMinutes * 60000LL : 0;
    m_nextPomodoroMs = m_pomodoroEnabled ? m_pomodoroWorkMinutes * 60000LL : 0;
    m_pomodoroBreakActive = false;
    if (m_checkpointEnabled) m_documents->autosaveNow();
    save();
    emit timerChanged(); emit statisticsChanged();
    return true;
}

bool WorkTimerService::switchTask(const QString &task)
{
    const QString normalized = task.trimmed();
    if (normalized.isEmpty()) return fail(QStringLiteral("Enter a task name."));
    if (active() && taskName() == normalized) {
        return running() ? (pauseSession(), true) : resumeSession();
    }
    if (active() && !finishSession()) return false;
    return startSession(normalized);
}

void WorkTimerService::captureMetrics()
{
    OtbmReader *map = m_documents->current();
    if (!map || currentMapKey() != m_active.value(QStringLiteral("mapKey")).toString()) return;
    m_sessionOperations += qMax<qint64>(0, map->editOperationCount() - m_baseOperations);
    m_sessionTiles += qMax<qint64>(0, map->changedTileCount() - m_baseTiles);
}

void WorkTimerService::resetMetricBaseline()
{
    OtbmReader *map = m_documents->current();
    m_baseOperations = map ? map->editOperationCount() : 0;
    m_baseTiles = map ? map->changedTileCount() : 0;
}

void WorkTimerService::pauseAt(qint64 timestampMs, const QString &reason)
{
    if (!running()) return;
    QJsonArray segments = m_active.value(QStringLiteral("segments")).toArray();
    segments.append(QJsonObject{
        {QStringLiteral("startedAt"),
         QDateTime::fromMSecsSinceEpoch(m_resumeMs, QTimeZone::UTC)
             .toString(Qt::ISODateWithMs)},
        {QStringLiteral("endedAt"),
         QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::UTC)
             .toString(Qt::ISODateWithMs)}});
    m_active.insert(QStringLiteral("segments"), segments);
    m_accumulatedMs += qMax<qint64>(0, timestampMs - m_resumeMs);
    captureMetrics();
    m_state = QStringLiteral("paused");
    save();
    emit timerChanged(); emit statisticsChanged();
    if (!reason.isEmpty()) emit reminderRequested(reason);
}

void WorkTimerService::pauseSession() { pauseAt(QDateTime::currentMSecsSinceEpoch()); }

bool WorkTimerService::resumeSession()
{
    clearError();
    if (m_state != QStringLiteral("paused")) return false;
    if (currentMapKey() != m_active.value(QStringLiteral("mapKey")).toString())
        return fail(QStringLiteral("Switch back to the session map before resuming."));
    m_resumeMs = QDateTime::currentMSecsSinceEpoch();
    m_lastActivityMs = m_resumeMs;
    m_state = QStringLiteral("running");
    resetMetricBaseline();
    m_pomodoroBreakActive = false;
    save(); emit timerChanged();
    return true;
}

bool WorkTimerService::finishSession(const QString &note)
{
    clearError();
    if (!active()) return false;
    if (running()) pauseAt(QDateTime::currentMSecsSinceEpoch());
    m_active.insert(QLatin1String(kDuration), m_accumulatedMs);
    m_active.insert(QLatin1String(kOperations), m_sessionOperations);
    m_active.insert(QLatin1String(kTiles), m_sessionTiles);
    m_active.insert(QStringLiteral("endedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    m_active.insert(QStringLiteral("note"), note.trimmed());
    m_sessions.push_back(m_active);
    if (m_checkpointEnabled) m_documents->autosaveNow();
    m_active = {}; m_state = QStringLiteral("stopped"); m_accumulatedMs = 0;
    m_sessionOperations = 0; m_sessionTiles = 0; m_pomodoroBreakActive = false;
    save(); emit timerChanged(); emit historyChanged(); emit statisticsChanged();
    return true;
}

bool WorkTimerService::updateSession(const QString &id, const QString &task,
                                     const QString &note, qint64 durationSeconds)
{
    for (QJsonObject &session : m_sessions) {
        if (session.value(QStringLiteral("id")).toString() != id) continue;
        session.insert(QStringLiteral("task"), task.trimmed());
        session.insert(QStringLiteral("note"), note.trimmed());
        session.insert(QLatin1String(kDuration), qMax<qint64>(0, durationSeconds) * 1000);
        save(); emit historyChanged(); emit timerChanged(); emit statisticsChanged(); return true;
    }
    return fail(QStringLiteral("Session was not found."));
}

bool WorkTimerService::removeSession(const QString &id)
{
    for (qsizetype i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions[i].value(QStringLiteral("id")).toString() != id) continue;
        m_sessions.removeAt(i); save(); emit historyChanged(); emit timerChanged(); emit statisticsChanged(); return true;
    }
    return false;
}

bool WorkTimerService::removeTaskTimer(const QString &task)
{
    clearError();
    const QString normalized = task.trimmed();
    if (normalized.isEmpty()) return false;

    const bool removedActive = active() && taskName() == normalized;
    if (removedActive) {
        m_active = {};
        m_state = QStringLiteral("stopped");
        m_accumulatedMs = 0;
        m_sessionOperations = 0;
        m_sessionTiles = 0;
        m_pomodoroBreakActive = false;
    }

    qsizetype removed = 0;
    for (qsizetype i = m_sessions.size(); i-- > 0;) {
        if (m_sessions[i].value(QStringLiteral("task")).toString() != normalized) continue;
        m_sessions.removeAt(i);
        ++removed;
    }
    if (removed == 0 && !removedActive) return false;

    save();
    emit historyChanged();
    emit timerChanged();
    emit statisticsChanged();
    return true;
}

QString WorkTimerService::storagePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/work-timer.json");
}

void WorkTimerService::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return;
    for (const QJsonValue &value : document.object().value(QStringLiteral("sessions")).toArray())
        if (value.isObject()) m_sessions.push_back(value.toObject());
    const QJsonObject activeSession = document.object().value(QStringLiteral("active")).toObject();
    if (!activeSession.isEmpty()) {
        m_active = activeSession;
        m_accumulatedMs = activeSession.value(QLatin1String(kDuration)).toInteger();
        m_sessionOperations = activeSession.value(QLatin1String(kOperations)).toInteger();
        m_sessionTiles = activeSession.value(QLatin1String(kTiles)).toInteger();
        m_state = QStringLiteral("paused");
    }
}

bool WorkTimerService::save()
{
    QDir().mkpath(QFileInfo(storagePath()).absolutePath());
    QJsonArray sessions;
    for (const QJsonObject &session : m_sessions) sessions.push_back(session);
    QJsonObject root{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("sessions"), sessions}};
    if (active()) {
        QJsonObject activeSession = m_active;
        activeSession.insert(QLatin1String(kDuration), elapsedSeconds() * 1000);
        activeSession.insert(QLatin1String(kOperations), operationCount());
        activeSession.insert(QLatin1String(kTiles), changedTileCount());
        root.insert(QStringLiteral("active"), activeSession);
    }
    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

void WorkTimerService::tick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (running()) {
        const qint64 elapsedMs = m_accumulatedMs + now - m_resumeMs;
        if (m_idlePauseMinutes > 0 && now - m_lastActivityMs >= m_idlePauseMinutes * 60000LL) {
            pauseAt(m_lastActivityMs + m_idlePauseMinutes * 60000LL,
                    QStringLiteral("Work timer paused after %1 minutes of inactivity.").arg(m_idlePauseMinutes));
            return;
        }
        if (m_pomodoroEnabled && m_nextPomodoroMs > 0 && elapsedMs >= m_nextPomodoroMs) {
            pauseAt(now);
            m_pomodoroBreakActive = true;
            m_pomodoroBreakEndMs = now + m_pomodoroBreakMinutes * 60000LL;
            m_nextPomodoroMs += m_pomodoroWorkMinutes * 60000LL;
            emit timerChanged();
            emit reminderRequested(QStringLiteral("Pomodoro work interval complete. Take a %1-minute break.").arg(m_pomodoroBreakMinutes));
            return;
        }
        if (m_breakReminderMinutes > 0 && m_nextBreakReminderMs > 0 && elapsedMs >= m_nextBreakReminderMs) {
            m_nextBreakReminderMs += m_breakReminderMinutes * 60000LL;
            emit reminderRequested(QStringLiteral("You have been mapping for %1 minutes. Consider taking a break.").arg(m_breakReminderMinutes));
        }
        if (++m_saveTicks >= 30) { m_saveTicks = 0; save(); }
    } else if (m_pomodoroBreakActive && now >= m_pomodoroBreakEndMs) {
        m_pomodoroBreakActive = false;
        emit reminderRequested(QStringLiteral("Pomodoro break complete. You can resume the session."));
    }
    emit timerChanged();
    if (active()) emit statisticsChanged();
}

bool WorkTimerService::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    switch (event->type()) {
    case QEvent::KeyPress: case QEvent::MouseButtonPress: case QEvent::MouseMove:
    case QEvent::Wheel: case QEvent::TouchBegin: case QEvent::TabletMove:
        if (running()) m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
        break;
    default: break;
    }
    return false;
}

QString WorkTimerService::currentMapKey() const
{
    const OtbmReader *map = m_documents->current();
    if (!map) return {};
    if (!map->filePath().isEmpty()) return QFileInfo(map->filePath()).canonicalFilePath().toLower();
    return QStringLiteral("untitled:") + m_documents->currentDocumentId();
}

QString WorkTimerService::currentMapNameValue() const
{
    const OtbmReader *map = m_documents->current();
    if (!map || map->filePath().isEmpty()) return QStringLiteral("(new map)");
    return QFileInfo(map->filePath()).fileName();
}

QString WorkTimerService::localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QString WorkTimerService::csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

bool WorkTimerService::exportCsv(const QUrl &fileUrl)
{
    QSaveFile file(localPath(fileUrl));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return fail(QStringLiteral("Could not create the CSV file."));
    file.write("Task,Map,Map path,Profile,Started,Ended,Duration seconds,Operations,Changed tiles,Note\r\n");
    for (const QJsonObject &s : m_sessions) {
        const QStringList row{csvCell(s.value(QStringLiteral("task")).toString()), csvCell(s.value(QStringLiteral("mapName")).toString()),
            csvCell(s.value(QStringLiteral("mapPath")).toString()), csvCell(s.value(QStringLiteral("profile")).toString()),
            csvCell(s.value(QStringLiteral("startedAt")).toString()), csvCell(s.value(QStringLiteral("endedAt")).toString()),
            QString::number(s.value(QLatin1String(kDuration)).toInteger() / 1000), QString::number(s.value(QLatin1String(kOperations)).toInteger()),
            QString::number(s.value(QLatin1String(kTiles)).toInteger()), csvCell(s.value(QStringLiteral("note")).toString())};
        file.write(row.join(QLatin1Char(',')).toUtf8() + "\r\n");
    }
    return file.commit() || fail(QStringLiteral("Could not finish the CSV file."));
}

bool WorkTimerService::exportJson(const QUrl &fileUrl)
{
    QJsonArray values;
    for (const QJsonObject &session : m_sessions) values.push_back(sessionView(session));
    QSaveFile file(localPath(fileUrl));
    if (!file.open(QIODevice::WriteOnly)) return fail(QStringLiteral("Could not create the JSON file."));
    file.write(QJsonDocument(values).toJson(QJsonDocument::Indented));
    return file.commit() || fail(QStringLiteral("Could not finish the JSON file."));
}

void WorkTimerService::clearError()
{
    if (m_errorString.isEmpty()) return;
    m_errorString.clear(); emit errorChanged();
}

bool WorkTimerService::fail(const QString &message)
{
    m_errorString = message; emit errorChanged(); return false;
}

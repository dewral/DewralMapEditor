#include "tilemapgenprocess.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

TileMapGenProcess::TileMapGenProcess(QObject *parent) : QObject(parent) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &TileMapGenProcess::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors = (m_errors + m_process.readAllStandardError()).right(16384);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false, tr("Cannot start Tile Map Generator: %1").arg(m_process.errorString()));
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        readOutput();
        m_errors = (m_errors + m_process.readAllStandardError()).right(16384);
        const bool success = !m_cancelled && status == QProcess::NormalExit && code == 0;
        complete(success, m_cancelled ? tr("Generation cancelled.")
                 : success ? QString() : tr("Tile Map Generator failed (exit %1).\n%2")
                     .arg(code).arg(QString::fromUtf8(m_errors).trimmed()));
    });
}

TileMapGenProcess::~TileMapGenProcess() {
    shutdown();
}

void TileMapGenProcess::shutdown() {
    cancel();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.waitForFinished(5000);
    }
}

QString TileMapGenProcess::programForDirectory(const QString &directory) {
    if (!QFileInfo(directory).isDir()) return {};
    const QDir root(directory);
    // A source checkout can have a newer engine than its last packaged build.
    const auto python = root.filePath(QStringLiteral(".venv/Scripts/python.exe"));
    if (QFileInfo(python).isFile() && QFileInfo(root.filePath(QStringLiteral("src/tilemapgen/cli.py"))).isFile())
        return python;
    for (const auto &relative : {"dist/TileMapGen/TileMapGen.exe", "TileMapGen.exe"}) {
        const auto path = root.filePath(QLatin1String(relative));
        if (QFileInfo(path).isFile()) return path;
    }
    return {};
}

QString TileMapGenProcess::errorSummary(const QString &error) {
    if (!error.contains(QStringLiteral("Traceback (most recent call last):"))) return error;
    auto lines = error.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    while (!lines.isEmpty() && lines.last().trimmed().startsWith(QStringLiteral("[PYI-"))) lines.removeLast();
    if (lines.size() < 2) return error;
    auto reason = lines.last().trimmed();
    static const QRegularExpression exceptionType(QStringLiteral("^[\\w.]+(?:Error|Exception):\\s*"));
    reason.remove(exceptionType);
    return lines.first().trimmed() + QLatin1Char('\n') + reason;
}

void TileMapGenProcess::start(const QString &directory, const QString &request,
                              const QString &pack, const QString &output) {
    run(directory, {QStringLiteral("generate"), request, QStringLiteral("--pack"), pack,
                    QStringLiteral("--output"), output}, {}, false);
}

void TileMapGenProcess::startCommand(const QString &directory, const QStringList &arguments, const QByteArray &input) {
    run(directory, arguments, input, true);
}

void TileMapGenProcess::run(const QString &directory, const QStringList &command, const QByteArray &input, bool capture) {
    if (m_running) return;
    m_running = true;
    m_cancelled = false;
    m_pendingOutput.clear(); m_errors.clear();
    m_output.clear(); m_capture = capture;
    const auto program = programForDirectory(directory);
    if (program.isEmpty()) {
        complete(false, tr("TileMapGen.exe or the prepared Python environment was not found in %1.").arg(directory));
        return;
    }
    QStringList arguments;
    auto environment = QProcessEnvironment::systemEnvironment();
    if (QFileInfo(program).fileName().compare(QStringLiteral("python.exe"), Qt::CaseInsensitive) == 0) {
        arguments << QStringLiteral("-m") << QStringLiteral("tilemapgen");
        environment.insert(QStringLiteral("PYTHONPATH"), QDir(directory).filePath(QStringLiteral("src")));
        environment.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    }
    arguments << command;
    m_process.setWorkingDirectory(directory);
    m_process.setProcessEnvironment(environment);
    m_process.start(program, arguments);
    if (!input.isEmpty()) m_process.write(input);
    m_process.closeWriteChannel();
}

void TileMapGenProcess::cancel() {
    if (!m_running) return;
    m_cancelled = true;
#ifdef Q_OS_WIN
    // A Windows virtual-environment launcher may start a second Python process.
    // Kill this owned process tree so it cannot continue writing after Cancel.
    if (m_process.processId() > 0 && m_process.state() != QProcess::NotRunning) {
        QProcess killer;
        killer.start(QDir(qEnvironmentVariable("SystemRoot", "C:/Windows"))
                         .filePath(QStringLiteral("System32/taskkill.exe")),
                     {QStringLiteral("/PID"), QString::number(m_process.processId()),
                      QStringLiteral("/T"), QStringLiteral("/F")});
        killer.waitForFinished(3000);
    }
#endif
    m_process.kill();
}

void TileMapGenProcess::readOutput() {
    const auto bytes = m_process.readAllStandardOutput();
    if (m_capture) m_output = (m_output + bytes).right(1024 * 1024);
    m_pendingOutput += bytes;
    static const QRegularExpression linePattern(QStringLiteral("^(\\d+)%\\s+(.+)$"));
    qsizetype newline;
    while ((newline = m_pendingOutput.indexOf('\n')) >= 0) {
        const auto line = QString::fromUtf8(m_pendingOutput.left(newline)).trimmed();
        m_pendingOutput.remove(0, newline + 1);
        const auto match = linePattern.match(line);
        if (match.hasMatch()) emit progressChanged(qBound(0, match.captured(1).toInt(), 100), match.captured(2));
    }
    m_pendingOutput = m_pendingOutput.right(16384);
}

void TileMapGenProcess::complete(bool success, const QString &error) {
    if (!m_running) return;
    m_running = false;
    emit finished(success, error);
}

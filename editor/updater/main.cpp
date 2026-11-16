#include "../core/updaterruntime.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QLabel>
#include <QLockFile>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QtWidgets/qdrawutil.h>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::wofstream logFile;

void log(const std::wstring &message)
{
    if (logFile)
        logFile << message << L'\n' << std::flush;
}

QString powershellQuote(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + value + QLatin1Char('\'');
}

bool runPowerShell(const QString &script)
{
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("PSModulePath"));
    process.setProcessEnvironment(environment);
    process.setProgram(QStringLiteral("powershell.exe"));
    process.setArguments({QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"),
                          QStringLiteral("-NonInteractive"),
                          QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                          QStringLiteral("-Command"),
                          QStringLiteral("$ErrorActionPreference='Stop'; ") + script});
    process.start();
    if (!process.waitForStarted(10000)) {
        log(L"Could not start PowerShell.");
        return false;
    }
    process.waitForFinished(-1);
    log(QString::fromLocal8Bit(process.readAllStandardError()).toStdWString());
    log(QString::fromLocal8Bit(process.readAllStandardOutput()).toStdWString());
    log(L"PowerShell exit code: " + std::to_wstring(process.exitCode()));
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool waitForApplication(quint32 pid, int timeoutMs)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return true;
    const DWORD result = WaitForSingleObject(process, static_cast<DWORD>(timeoutMs));
    CloseHandle(process);
    return result == WAIT_OBJECT_0;
#else
    Q_UNUSED(pid)
    Q_UNUSED(timeoutMs)
    return true;
#endif
}

fs::path packageRoot(const fs::path &staging)
{
    if (fs::exists(staging / L"DME.exe"))
        return staging;
    for (const fs::directory_entry &entry : fs::directory_iterator(staging)) {
        if (entry.is_directory() && fs::exists(entry.path() / L"DME.exe"))
            return entry.path();
    }
    return {};
}

bool applicationInstanceRunning(const fs::path &executable)
{
#ifdef Q_OS_WIN
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                         entry.th32ProcessID);
            if (!process)
                continue;

            std::wstring path(32768, L'\0');
            DWORD pathSize = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &pathSize)) {
                path.resize(pathSize);
                running = _wcsicmp(fs::path(path).lexically_normal().c_str(),
                                   executable.lexically_normal().c_str()) == 0;
            }
            CloseHandle(process);
            if (running)
                break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return running;
#else
    Q_UNUSED(executable)
    return false;
#endif
}

bool waitForApplicationInstances(const fs::path &executable)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    while (applicationInstanceRunning(executable)) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        QThread::msleep(250);
    }
    return true;
}

void copyFileOverwrite(const fs::path &source, const fs::path &destination,
                       std::error_code &error)
{
#ifdef Q_OS_WIN
    if (CopyFileW(source.c_str(), destination.c_str(), FALSE))
        error.clear();
    else
        error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
#endif
}

void copyFileOverwrite(const fs::path &source, const fs::path &destination)
{
    std::error_code error;
    copyFileOverwrite(source, destination, error);
    if (error)
        throw fs::filesystem_error("cannot replace update file", source, destination, error);
}

bool installFiles(const fs::path &source, const fs::path &target,
                  const fs::path &backup,
                  const std::function<void(int)> &progress)
{
    std::vector<fs::path> overwritten;
    std::vector<fs::path> created;
    size_t fileCount = 0;
    for (const fs::directory_entry &entry : fs::recursive_directory_iterator(source)) {
        if (entry.is_regular_file())
            ++fileCount;
    }

    size_t copied = 0;
    int lastProgress = -1;
    try {
        fs::create_directories(backup);
        for (const fs::directory_entry &entry : fs::recursive_directory_iterator(source)) {
            const fs::path relative = fs::relative(entry.path(), source);
            const fs::path destination = target / relative;
            if (entry.is_directory()) {
                fs::create_directories(destination);
                continue;
            }
            if (!entry.is_regular_file())
                continue;

            fs::create_directories(destination.parent_path());
            if (fs::exists(destination)) {
                const fs::path saved = backup / relative;
                fs::create_directories(saved.parent_path());
                copyFileOverwrite(destination, saved);
                overwritten.push_back(relative);
            } else {
                created.push_back(relative);
            }
            copyFileOverwrite(entry.path(), destination);
            ++copied;
            const int value = fileCount > 0
                ? 55 + static_cast<int>((copied * 35) / fileCount) : 90;
            if (value != lastProgress) {
                progress(value);
                lastProgress = value;
            }
        }
        return true;
    } catch (const std::exception &exception) {
        const QString detail = QString::fromLocal8Bit(exception.what());
        log(L"Installation error: " + detail.toStdWString());
        log(L"Installation failed; restoring backup.");
        std::error_code ignored;
        for (auto it = created.rbegin(); it != created.rend(); ++it)
            fs::remove(target / *it, ignored);
        for (auto it = overwritten.rbegin(); it != overwritten.rend(); ++it) {
            fs::create_directories((target / *it).parent_path(), ignored);
            copyFileOverwrite(backup / *it, target / *it, ignored);
        }
        return false;
    }
}

QString argumentValue(const QStringList &arguments, const QString &name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments[index + 1] : QString();
}

QString updaterTheme(const QString &requested, const QString &overrideStyle,
                     const QString &saved)
{
    const auto supported = [](const QString &theme) {
        return theme == QStringLiteral("tibia-ui") || theme == QStringLiteral("fluent-dark");
    };
    if (supported(requested))
        return requested;
    if (supported(overrideStyle))
        return overrideStyle;
    if (saved == QStringLiteral("fluent-dark") || saved == QStringLiteral("rme-fluent"))
        return QStringLiteral("fluent-dark");
    return QStringLiteral("tibia-ui");
}

void scheduleTemporaryRuntimeCleanup()
{
    QDir runtime(QCoreApplication::applicationDirPath());
    if (!runtime.dirName().startsWith(QStringLiteral("runtime-")))
        return;
    QDir parent = runtime;
    if (!parent.cdUp() || parent.dirName() != QStringLiteral("DewralMapEditorUpdates"))
        return;

    const QString script = QStringLiteral(
        "Start-Sleep -Milliseconds 1500; "
        "Remove-Item -LiteralPath %1 -Recurse -Force -ErrorAction SilentlyContinue")
        .arg(powershellQuote(runtime.absolutePath()));
    QProcess::startDetached(QStringLiteral("powershell.exe"),
                            {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"),
                             QStringLiteral("-NonInteractive"),
                             QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                             QStringLiteral("-Command"), script},
                            QDir::tempPath());
}
}

class UpdaterWindow final : public QDialog
{
public:
    explicit UpdaterWindow(bool classic)
        : QDialog(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
          m_frame(classic ? QPixmap(QStringLiteral(":/update/menubox.png")) : QPixmap())
    {
        setWindowTitle(QStringLiteral("Dewral Map Editor Update"));
        setFixedWidth(420);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 22, 24, 20);
        layout->setSpacing(12);

        auto *heading = new QLabel(QStringLiteral("Updating Dewral Map Editor"), this);
        heading->setObjectName(QStringLiteral("heading"));
        layout->addWidget(heading);

        m_status = new QLabel(QStringLiteral("Preparing the update..."), this);
        m_status->setWordWrap(true);
        layout->addWidget(m_status);

        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);
        m_progress->setTextVisible(true);
        layout->addWidget(m_progress);

        m_closeButton = new QPushButton(QStringLiteral("Close"), this);
        m_closeButton->setFixedWidth(100);
        m_closeButton->setVisible(false);
        connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(m_closeButton, 0, Qt::AlignRight);

        if (classic) {
            setStyleSheet(QStringLiteral(R"(
                QLabel { color: #d5d1c7; font-family: Verdana; font-size: 12px; }
                QLabel#heading { color: #f0eee6; font-size: 15px; font-weight: 600; }
                QProgressBar { background: #222720; border: 1px solid #181b17;
                               color: #f0eee6; text-align: center; border-radius: 7px; height: 14px; }
                QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                                       stop:0 #72ad5b, stop:1 #487a38); border-radius: 6px; }
                QPushButton { background: #383832; color: #f0eee6;
                              border: 1px solid #68665c; padding: 6px 16px; }
                QPushButton:hover { background: #45453e; }
            )"));
            return;
        }

        setStyleSheet(QStringLiteral(R"(
            QDialog { background: #0d1117; color: #e6edf3; }
            QLabel { color: #b8c0ca; font-size: 12px; }
            QLabel#heading { color: #f0f6fc; font-size: 16px; font-weight: 600; }
            QProgressBar { background: #161b22; border: 1px solid #30363d;
                           border-radius: 5px; color: #e6edf3; height: 18px;
                           text-align: center; }
            QProgressBar::chunk { background: #238636; border-radius: 4px; }
            QPushButton { background: #21262d; color: #e6edf3;
                          border: 1px solid #30363d; border-radius: 5px;
                          padding: 6px 16px; }
            QPushButton:hover { background: #30363d; }
        )"));

    }

    void showStandaloneMessage()
    {
        m_busy = false;
        m_status->setText(QStringLiteral(
            "DMEUpdater is started automatically by Dewral Map Editor.\n\n"
            "Open DME to check for updates from the start window."));
        m_progress->setVisible(false);
        m_closeButton->setVisible(true);
    }

    void start(const QStringList &arguments)
    {
        const QString pidText = argumentValue(arguments, QStringLiteral("--pid"));
        const QString archive = argumentValue(arguments, QStringLiteral("--archive"));
        const QString target = argumentValue(arguments, QStringLiteral("--target"));
        const QString executable = argumentValue(arguments, QStringLiteral("--exe"));
        const QString expectedHash = argumentValue(arguments, QStringLiteral("--sha256"));
        const QString readyPath = argumentValue(arguments, QStringLiteral("--ready-file"));

        bool pidOk = false;
        const quint32 pid = pidText.toUInt(&pidOk);
        if (!pidOk || archive.isEmpty() || target.isEmpty() || executable.isEmpty()
            || expectedHash.size() != 64 || !QFileInfo::exists(archive)
            || !QFileInfo(target).isDir()
            || (!readyPath.isEmpty() && QFileInfo(readyPath).absolutePath() != QCoreApplication::applicationDirPath())) {
            fail(QStringLiteral("The updater received invalid arguments."));
            return;
        }

        m_busy = true;
        QThread *worker = QThread::create([this, pid, archive, target,
                                            executable, expectedHash, readyPath]() {
            try {
                runUpdate(pid, archive, target, executable, expectedHash, readyPath);
            } catch (const std::exception &exception) {
                log(QString::fromLocal8Bit(exception.what()).toStdWString());
                fail(QStringLiteral("The update failed. See DMEUpdater.log for details."));
            }
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        if (m_frame.isNull()) {
            QDialog::paintEvent(event);
            return;
        }
        QPainter painter(this);
        const QMargins border(3, 3, 3, 3);
        qDrawBorderPixmap(&painter, rect(), border, m_frame, m_frame.rect(), border,
                          QTileRules(Qt::RepeatTile));
    }

    void reject() override
    {
        if (!m_busy)
            QDialog::reject();
    }

    void closeEvent(QCloseEvent *event) override
    {
        if (m_busy)
            event->ignore();
        else
            QDialog::closeEvent(event);
    }

private:
    void postStatus(const QString &text, int progress)
    {
        QMetaObject::invokeMethod(this, [this, text, progress]() {
            m_status->setText(text);
            m_progress->setValue(progress);
        }, Qt::QueuedConnection);
    }

    void fail(const QString &message)
    {
        QMetaObject::invokeMethod(this, [this, message]() {
            m_busy = false;
            m_status->setText(message);
            m_status->setStyleSheet(QStringLiteral("color: #f85149;"));
            m_progress->setVisible(false);
            m_closeButton->setVisible(true);
        }, Qt::QueuedConnection);
    }

    void runUpdate(quint32 pid, const QString &archivePath,
                   const QString &targetPath, const QString &executable,
                   QString expectedHash, const QString &readyPath)
    {
        logFile.open((fs::temp_directory_path() / L"DMEUpdater.log"), std::ios::trunc);
        log(L"DME updater started.");

        QLockFile installLock(QDir(targetPath).filePath(QStringLiteral(".dme-update.lock")));
        installLock.setStaleLockTime(0);
        if (!installLock.tryLock(0)) {
            fail(QStringLiteral("Could not lock the installation folder. Another update may be running."));
            return;
        }
        if (!readyPath.isEmpty()) {
            QFile ready(readyPath);
            if (!ready.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                fail(QStringLiteral("Could not confirm updater startup."));
                return;
            }
        }

        const fs::path target = targetPath.toStdWString();
        const fs::path targetExecutable = target / executable.toStdWString();
        postStatus(QStringLiteral("Waiting for all DME windows to close..."), 42);
        if (!waitForApplication(pid, readyPath.isEmpty() ? 300000 : 30000)
            || !waitForApplicationInstances(targetExecutable)) {
            fail(QStringLiteral(
                "Close every DME window before installing the update, then try again."));
            return;
        }

        if (!readyPath.isEmpty() && !QFileInfo::exists(readyPath + QStringLiteral(".commit"))) {
            fail(QStringLiteral("The editor did not authorize installation."));
            return;
        }

        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path work = fs::temp_directory_path()
            / (L"DMEUpdate-" + std::to_wstring(stamp));
        const fs::path staging = work / L"staging";
        const fs::path backup = work / L"backup";
        const fs::path archive = archivePath.toStdWString();
        std::error_code error;
        fs::create_directories(staging, error);
        if (error) {
            fail(QStringLiteral("The update staging directory could not be created."));
            return;
        }

        postStatus(QStringLiteral("Verifying the downloaded package..."), 45);
        expectedHash = expectedHash.toUpper();
        const QString script =
            QStringLiteral("$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath %1).Hash; "
                           "if ($actual -ne %2) { exit 23 }; "
                           "Expand-Archive -LiteralPath %1 -DestinationPath %3 -Force")
                .arg(powershellQuote(archivePath), powershellQuote(expectedHash),
                     powershellQuote(QString::fromStdWString(staging.wstring())));
        postStatus(QStringLiteral("Extracting the update..."), 50);
        if (!runPowerShell(script)) {
            fs::remove_all(work, error);
            fail(QStringLiteral("The update could not be verified or extracted."));
            return;
        }

        const fs::path source = packageRoot(staging);
        if (source.empty()) {
            fs::remove_all(work, error);
            fail(QStringLiteral("The update package does not contain DME.exe."));
            return;
        }

        postStatus(QStringLiteral("Installing application files..."), 55);
        if (!installFiles(source, target, backup, [this](int value) {
                postStatus(QStringLiteral("Installing application files..."), value);
            })) {
            fail(QStringLiteral("Installation failed. Recovery files are in %1.")
                     .arg(QString::fromStdWString(work.wstring())));
            return;
        }

        postStatus(QStringLiteral("Update installed. Restarting DME..."), 96);
        fs::remove_all(work, error);
        fs::remove(archive, error);
        QProcess editor;
        editor.setProgram(QDir(targetPath).filePath(executable));
        editor.setWorkingDirectory(targetPath);
        auto environment = QProcessEnvironment::systemEnvironment();
        if (!readyPath.isEmpty()) {
            environment.remove(QStringLiteral("QT_PLUGIN_PATH"));
            environment.remove(QStringLiteral("QT_QPA_PLATFORM"));
        }
        editor.setProcessEnvironment(environment);
        if (!editor.startDetached()) {
            fail(QStringLiteral("The update was installed, but DME could not be restarted."));
            return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setValue(100);
            m_busy = false;
            QTimer::singleShot(700, qApp, &QCoreApplication::quit);
        }, Qt::QueuedConnection);
    }

    const QPixmap m_frame;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_closeButton = nullptr;
    bool m_busy = false;
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("DMEUpdater"));
    // Exercise the Windows loader and Qt platform plugin without installing anything.
    if (application.arguments().contains(QStringLiteral("--smoke-test")))
        return application.arguments().contains(QStringLiteral("--require-handoff"))
            ? UpdaterRuntime::handoffProbeExitCode : 0;
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &application, &scheduleTemporaryRuntimeCleanup);

    const QStringList arguments = application.arguments().mid(1);
    const QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                             QStringLiteral("Dewral"), QStringLiteral("DewralMapEditor"));
    const QString theme = updaterTheme(
        argumentValue(arguments, QStringLiteral("--theme")),
        qEnvironmentVariable("DME_UI_STYLE_OVERRIDE"),
        settings.value(QStringLiteral("ui/style")).toString());
    UpdaterWindow window(theme == QStringLiteral("tibia-ui"));
    window.adjustSize();
    bool hasX = false, hasY = false;
    const int x = argumentValue(arguments, QStringLiteral("--center-x")).toInt(&hasX);
    const int y = argumentValue(arguments, QStringLiteral("--center-y")).toInt(&hasY);
    const QPoint center(x, y);
    const QScreen *screen = hasX && hasY ? QGuiApplication::screenAt(center) : nullptr;
    const QRect area = (screen ? screen : QGuiApplication::primaryScreen())->availableGeometry();
    const QPoint anchor = screen ? center : area.center();
    window.move(qBound(area.left(), anchor.x() - window.width() / 2,
                       qMax(area.left(), area.right() - window.width() + 1)),
                qBound(area.top(), anchor.y() - window.height() / 2,
                       qMax(area.top(), area.bottom() - window.height() + 1)));
    QObject::connect(&window, &QDialog::finished, &application, &QCoreApplication::quit);
    window.show();
    if (arguments.isEmpty())
        window.showStandaloneMessage();
    else
        QTimer::singleShot(0, &window, [&window, arguments]() { window.start(arguments); });
    return application.exec();
}

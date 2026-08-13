#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMetaObject>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
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
    process.setProgram(QStringLiteral("powershell.exe"));
    process.setArguments({QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"),
                          QStringLiteral("-NonInteractive"),
                          QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                          QStringLiteral("-Command"), script});
    process.start();
    if (!process.waitForStarted(10000)) {
        log(L"Could not start PowerShell.");
        return false;
    }
    process.waitForFinished(-1);
    log(L"PowerShell exit code: " + std::to_wstring(process.exitCode()));
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool waitForApplication(quint32 pid)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return true;
    const DWORD result = WaitForSingleObject(process, 5 * 60 * 1000);
    CloseHandle(process);
    return result == WAIT_OBJECT_0;
#else
    Q_UNUSED(pid)
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
                fs::copy_file(destination, saved, fs::copy_options::overwrite_existing);
                overwritten.push_back(relative);
            } else {
                created.push_back(relative);
            }
            fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing);
            ++copied;
            const int value = fileCount > 0
                ? 55 + static_cast<int>((copied * 35) / fileCount) : 90;
            if (value != lastProgress) {
                progress(value);
                lastProgress = value;
            }
        }
        return true;
    } catch (...) {
        log(L"Installation failed; restoring backup.");
        std::error_code ignored;
        for (auto it = created.rbegin(); it != created.rend(); ++it)
            fs::remove(target / *it, ignored);
        for (auto it = overwritten.rbegin(); it != overwritten.rend(); ++it) {
            fs::create_directories((target / *it).parent_path(), ignored);
            fs::copy_file(backup / *it, target / *it,
                          fs::copy_options::overwrite_existing, ignored);
        }
        return false;
    }
}

QString argumentValue(const QStringList &arguments, const QString &name)
{
    const int index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments[index + 1] : QString();
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
    explicit UpdaterWindow(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Dewral Map Editor Update"));
        setFixedSize(460, 205);

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

        layout->addStretch();
        m_closeButton = new QPushButton(QStringLiteral("Close"), this);
        m_closeButton->setFixedWidth(100);
        m_closeButton->setVisible(false);
        connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(m_closeButton, 0, Qt::AlignRight);

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
            "Open DME and select Help > Check for Updates."));
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

        bool pidOk = false;
        const quint32 pid = pidText.toUInt(&pidOk);
        if (!pidOk || archive.isEmpty() || target.isEmpty() || executable.isEmpty()
            || expectedHash.size() != 64 || !QFileInfo::exists(archive)
            || !QFileInfo(target).isDir()) {
            fail(QStringLiteral("The updater received invalid arguments."));
            return;
        }

        m_busy = true;
        QThread *worker = QThread::create([this, pid, archive, target,
                                            executable, expectedHash]() {
            runUpdate(pid, archive, target, executable, expectedHash);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }

protected:
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
                   QString expectedHash)
    {
        logFile.open((fs::temp_directory_path() / L"DMEUpdater.log"), std::ios::trunc);
        log(L"DME updater started.");

        postStatus(QStringLiteral("Waiting for DME to close..."), 5);
        if (!waitForApplication(pid)) {
            fail(QStringLiteral("DME did not close in time. The update was cancelled."));
            return;
        }

        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const fs::path work = fs::temp_directory_path()
            / (L"DMEUpdate-" + std::to_wstring(stamp));
        const fs::path staging = work / L"staging";
        const fs::path backup = work / L"backup";
        const fs::path archive = archivePath.toStdWString();
        const fs::path target = targetPath.toStdWString();
        std::error_code error;
        fs::create_directories(staging, error);
        if (error) {
            fail(QStringLiteral("The update staging directory could not be created."));
            return;
        }

        postStatus(QStringLiteral("Verifying the downloaded package..."), 15);
        expectedHash = expectedHash.toUpper();
        const QString script =
            QStringLiteral("$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath %1).Hash; "
                           "if ($actual -ne %2) { exit 23 }; "
                           "Expand-Archive -LiteralPath %1 -DestinationPath %3 -Force")
                .arg(powershellQuote(archivePath), powershellQuote(expectedHash),
                     powershellQuote(QString::fromStdWString(staging.wstring())));
        postStatus(QStringLiteral("Extracting the update..."), 30);
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
            fs::remove_all(work, error);
            fail(QStringLiteral("Installation failed. Existing files were restored."));
            return;
        }

        postStatus(QStringLiteral("Update installed. Restarting DME..."), 96);
        fs::remove_all(work, error);
        fs::remove(archive, error);
        if (!QProcess::startDetached(QDir(targetPath).filePath(executable), {}, targetPath)) {
            fail(QStringLiteral("The update was installed, but DME could not be restarted."));
            return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            m_progress->setValue(100);
            m_busy = false;
            QTimer::singleShot(700, qApp, &QCoreApplication::quit);
        }, Qt::QueuedConnection);
    }

    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_closeButton = nullptr;
    bool m_busy = false;
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("DMEUpdater"));
    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &application, &scheduleTemporaryRuntimeCleanup);

    UpdaterWindow window;
    window.show();
    const QStringList arguments = application.arguments().mid(1);
    if (arguments.isEmpty())
        window.showStandaloneMessage();
    else
        QTimer::singleShot(0, &window, [&window, arguments]() { window.start(arguments); });
    return application.exec();
}

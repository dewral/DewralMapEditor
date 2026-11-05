#include "updaterruntime.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVariant>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

bool UpdaterRuntime::prepare(const QString &source, const QString &destination, QString &error)
{
    error.clear();
    const QDir sourceDir(source);
    QStringList files{QStringLiteral("DMEUpdater.exe"), QStringLiteral("Qt6Core.dll"),
                      QStringLiteral("Qt6Gui.dll"), QStringLiteral("Qt6Widgets.dll"),
                      QStringLiteral("platforms/qwindows.dll")};
    // Qt's MSVC builds can depend on ICU and other DLLs, not just Qt itself.
    files.append(sourceDir.entryList({QStringLiteral("*.dll")}, QDir::Files));
    files.removeDuplicates();
    for (const QString &file : files) {
        const QString target = QDir(destination).filePath(file);
        if (!QDir().mkpath(QFileInfo(target).absolutePath())
            || !QFile::copy(sourceDir.filePath(file), target)) {
            error = QStringLiteral("Could not prepare the updater runtime file: %1").arg(file);
            return false;
        }
    }
    return true;
}

void UpdaterRuntime::probe(const QString &directory, QObject *owner,
                          const std::function<void(bool, const QString &)> &finished)
{
    auto *process = new QProcess(owner);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"));
    environment.insert(QStringLiteral("QT_PLUGIN_PATH"), directory);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("windows"));
    process->setProcessEnvironment(environment);
    process->setWorkingDirectory(directory);
    process->setProgram(QDir(directory).filePath(QStringLiteral("DMEUpdater.exe")));
    process->setArguments({QStringLiteral("--smoke-test")});
    auto complete = [process, finished](bool ok) {
        if (process->property("reported").toBool())
            return;
        process->setProperty("reported", true);
        finished(ok, ok ? QString() : QStringLiteral(
            "The updater could not load its bundled runtime. The EXE and Qt DLLs may come "
            "from different builds (MSVC/MinGW). Extract the complete release ZIP into "
            "a new folder and run DME from there. The editor has not been closed."));
        process->deleteLater();
    };
    QObject::connect(process, &QProcess::finished, owner,
        [complete](int code, QProcess::ExitStatus status) {
            complete(status == QProcess::NormalExit && code == 0);
        });
    QObject::connect(process, &QProcess::errorOccurred, owner,
        [complete](QProcess::ProcessError) { complete(false); });
    QTimer::singleShot(15000, process, [process, complete] {
        if (!process->property("reported").toBool()) {
            process->kill(); // Only our disposable smoke process; never the editor.
            complete(false);
        }
    });
#ifdef Q_OS_WIN
    // A broken DLL must fail the probe rather than show a Windows loader dialog.
    const UINT previousMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    process->start();
#ifdef Q_OS_WIN
    SetErrorMode(previousMode);
#endif
}

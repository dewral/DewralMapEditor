#include "updaterruntime.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

bool probe(const QString &directory) {
    QEventLoop loop;
    bool result = false;
    bool completed = false;
    UpdaterRuntime::probe(directory, &loop, [&](bool ok, const QString &) {
        result = ok;
        completed = true;
        loop.quit();
    });
    if (!completed) loop.exec();
    return result;
}
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    if (argc != 2) return 1;
#ifdef Q_OS_WIN
    // Do not let a developer Qt kit on PATH hide missing packaged dependencies.
    const QByteArray windows = qgetenv("SystemRoot");
    qputenv("PATH", windows + "/System32;" + windows);
#endif
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    QString error;
    if (UpdaterRuntime::prepare(temporary.path() + "/missing", temporary.path() + "/empty", error)
        || error.isEmpty()) return 2;
    const QString runtime = temporary.path() + "/runtime";
    if (!UpdaterRuntime::prepare(QString::fromLocal8Bit(argv[1]), runtime, error)) {
        std::cerr << error.toStdString(); return 3;
    }
    // Both compiler-runtime DLLs and Qt's transitive dependencies must be staged.
    const QDir source(QString::fromLocal8Bit(argv[1]));
    for (const QString &dll : source.entryList({"*.dll"}, QDir::Files))
        if (!QFile::exists(runtime + '/' + dll)) return 4;
    if (!probe(runtime)) return 5;
    QFile broken(runtime + "/Qt6Core.dll");
    if (!broken.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 6;
    broken.write("Invalid Qt runtime");
    broken.close();
    if (probe(runtime)) return 7;
    std::cout << "Complete runtime accepted; incomplete and incompatible runtimes rejected.\n";
    return 0;
}

#ifndef UPDATERRUNTIME_H
#define UPDATERRUNTIME_H

#include <QProcessEnvironment>
#include <QString>
#include <functional>

class QObject;
namespace UpdaterRuntime {
inline constexpr int handoffProbeExitCode = 42;
QProcessEnvironment environment(const QString &directory);
bool prepare(const QString &source, const QString &destination, QString &error);
void probe(const QString &directory, QObject *owner,
           const std::function<void(bool, const QString &)> &finished);
}
#endif

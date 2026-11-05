#ifndef UPDATERRUNTIME_H
#define UPDATERRUNTIME_H

#include <QString>
#include <functional>

class QObject;
namespace UpdaterRuntime {
bool prepare(const QString &source, const QString &destination, QString &error);
void probe(const QString &directory, QObject *owner,
           const std::function<void(bool, const QString &)> &finished);
}
#endif

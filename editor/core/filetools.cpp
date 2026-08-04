#include "filetools.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>

bool FileTools::exists(const QString &path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

QString FileTools::findByExt(const QString &folder, const QString &ext,
                             const QString &preferred) const
{
    if (folder.isEmpty()) {
        return QString();
    }
    QDir dir(folder);
    if (!dir.exists()) {
        return QString();
    }

    if (!preferred.isEmpty() && dir.exists(preferred)) {
        return dir.absoluteFilePath(preferred);
    }

    const QStringList matches = dir.entryList({QStringLiteral("*.%1").arg(ext)},
                                              QDir::Files, QDir::Name);
    if (matches.isEmpty()) {
        return QString();
    }
    return dir.absoluteFilePath(matches.first());
}

QString FileTools::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString FileTools::dirName(const QString &path) const
{
    return QFileInfo(path).absolutePath();
}

void FileTools::setClipboard(const QString &text) const
{
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(text);
    }
}

QString FileTools::clipboardText() const
{
    if (const QClipboard *cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return QString();
}

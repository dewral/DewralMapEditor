#ifndef FILETOOLS_H
#define FILETOOLS_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class FileTools : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    using QObject::QObject;

    Q_INVOKABLE bool exists(const QString &path) const;

    Q_INVOKABLE QString findByExt(const QString &folder, const QString &ext,
                                  const QString &preferred = QString()) const;
    Q_INVOKABLE QString fileName(const QString &path) const;
    Q_INVOKABLE QString dirName(const QString &path) const;
    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE void setClipboard(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;
};

#endif

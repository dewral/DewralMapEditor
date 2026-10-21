#include "brushpngexport.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>
#include <QVariantMap>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QtCore/private/qzipwriter_p.h>
#else
#include <QtGui/private/qzipwriter_p.h>
#endif

namespace BrushPngExport {
namespace {
QString tr(const char *message) { return QCoreApplication::translate("BrushPngExport", message); }

QString fileStem(QString name)
{
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]")), QStringLiteral("_"));
    name = name.trimmed().left(160);
    while (name.startsWith(QLatin1Char('.')))
        name.remove(0, 1);
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name.isEmpty())
        name = QStringLiteral("brush");
    static const QRegularExpression reserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(name).hasMatch())
        name.prepend(QLatin1Char('_'));
    return name;
}
}

QString saveArchive(const QVariantList &brushes, const QUrl &destination,
                    const std::function<QString(const QVariantList &, QIODevice *)> &writePng)
{
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return tr("Choose a local ZIP file to export the brushes.");
    if (brushes.isEmpty())
        return tr("There are no saved brushes to export.");

    // QZipWriter closes its device. Stage on disk, then atomically publish so
    // a failed brush never leaves an incomplete ZIP or replaces an existing one.
    QTemporaryFile staging;
    if (!staging.open())
        return tr("Could not create the brush archive: %1").arg(staging.errorString());
    QZipWriter zip(&staging);
    zip.setCompressionPolicy(QZipWriter::NeverCompress); // PNGs are already compressed.
    QSet<QString> filenames;
    for (const QVariant &value : brushes) {
        const QVariantMap brush = value.toMap();
        const QString name = brush.value(QStringLiteral("name")).toString();
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        const QString error = writePng(brush.value(QStringLiteral("cells")).toList(), &buffer);
        if (!error.isEmpty())
            return tr("Could not export brush '%1': %2").arg(name, error);

        const QString stem = fileStem(name);
        QString filename = stem + QStringLiteral(".png");
        for (int suffix = 2; filenames.contains(filename.toCaseFolded()); ++suffix)
            filename = stem + QStringLiteral(" (%1).png").arg(suffix);
        filenames.insert(filename.toCaseFolded());
        zip.addFile(filename, png);
        if (zip.status() != QZipWriter::NoError)
            return tr("Could not write the brush archive.");
    }
    zip.close();
    if (zip.status() != QZipWriter::NoError)
        return tr("Could not finish the brush archive.");
    if (!staging.open())
        return tr("Could not read the brush archive: %1").arg(staging.errorString());

    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return tr("Could not save the brush archive: %1").arg(file.errorString());
    while (!staging.atEnd()) {
        const QByteArray bytes = staging.read(64 * 1024);
        if (bytes.isEmpty() || file.write(bytes) != bytes.size())
            return tr("Could not copy the brush archive.");
    }
    if (!file.commit())
        return tr("Could not save the brush archive: %1").arg(file.errorString());
    return {};
}
}

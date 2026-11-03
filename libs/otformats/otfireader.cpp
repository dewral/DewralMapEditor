#include "otfireader.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

OtfiReader::OtfiReader(QObject *parent)
    : QObject(parent)
{
}

bool OtfiReader::loadFromFolder(const QString &folder)
{
    const QDir dir(folder);
    const auto matches = dir.entryList({QStringLiteral("*.otfi")}, QDir::Files, QDir::Name);
    return loadFile(matches.isEmpty() ? QString() : dir.absoluteFilePath(matches.first()));
}

bool OtfiReader::loadFile(const QString &path)
{
    const bool wasFound = m_found;
    m_found = false;
    m_extended = false;
    m_transparency = false;
    m_frameDurations = false;
    m_frameGroups = false;
    m_metadataFile.clear();
    m_spritesFile.clear();

    if(path.isEmpty()) { if(wasFound)emit foundChanged(); return false; }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (wasFound) emit foundChanged();
        return false;
    }

    QTextStream in(&f);
    bool inDatSpr = false, sectionFound = false;
    while (!in.atEnd()) {
        const QString rawLine = in.readLine();
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;

        const bool indented = rawLine.startsWith(QLatin1Char(' ')) || rawLine.startsWith(QLatin1Char('\t'));
        if (!indented) {

            inDatSpr = (trimmed.compare(QLatin1String("DatSpr"), Qt::CaseInsensitive) == 0);
            sectionFound = sectionFound || inDatSpr;
            continue;
        }
        if (!inDatSpr) continue;

        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString key = trimmed.left(colon).trimmed().toLower();
        const QString value = trimmed.mid(colon + 1).trimmed();
        const bool boolVal = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;

        if (key == QLatin1String("extended")) m_extended = boolVal;
        else if (key == QLatin1String("transparency")) m_transparency = boolVal;
        else if (key == QLatin1String("frame-durations")) m_frameDurations = boolVal;
        else if (key == QLatin1String("frame-groups")) m_frameGroups = boolVal;
        else if (key == QLatin1String("metadata-file")) m_metadataFile = value;
        else if (key == QLatin1String("sprites-file")) m_spritesFile = value;

    }

    if(!sectionFound) { if(wasFound)emit foundChanged(); return false; }
    if (m_metadataFile.isEmpty()) m_metadataFile = QStringLiteral("Tibia.dat");
    if (m_spritesFile.isEmpty()) m_spritesFile = QStringLiteral("Tibia.spr");

    m_found = true;
    emit foundChanged();
    return true;
}

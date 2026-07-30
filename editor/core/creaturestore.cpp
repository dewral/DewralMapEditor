#include "creaturestore.h"

#include "dmedatadir.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

CreatureStore::CreatureStore(QObject *parent)
    : QAbstractListModel(parent)
{
}

bool CreatureStore::loadForVersion(int version)
{
    return loadForDir(QString::number(version));
}

bool CreatureStore::loadForDir(const QString &dirName)
{
    beginResetModel();
    m_creatures.clear();
    m_path = QDir(dmeDataDir())
                 .filePath(QStringLiteral("%1/creatures.xml").arg(dirName));
    const bool ok = QFile::exists(m_path) && loadFile(m_path);
    std::sort(m_creatures.begin(), m_creatures.end(),
              [](const CreatureType &a, const CreatureType &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    endResetModel();
    emit countChanged();
    return ok;
}

void CreatureStore::setErrorString(const QString &error)
{
    if (m_errorString == error) return;
    m_errorString = error;
    emit errorStringChanged();
}

bool CreatureStore::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QXmlStreamReader xml(&f);

    bool inNpcsSection = false;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        const auto tag = xml.name();
        if (token == QXmlStreamReader::StartElement) {
            if (tag == QLatin1String("npcs")) { inNpcsSection = true; continue; }
            if (tag == QLatin1String("monsters")) { inNpcsSection = false; continue; }

            const bool isCreature = tag == QLatin1String("creature")
                                    || tag == QLatin1String("monster")
                                    || tag == QLatin1String("npc");
            if (!isCreature) continue;

            const auto a = xml.attributes();
            CreatureType c;
            c.name = a.value(QLatin1String("name")).toString();
            if (c.name.isEmpty()) continue;
            c.isNpc = inNpcsSection || tag == QLatin1String("npc")
                      || a.value(QLatin1String("type")) == QLatin1String("npc");
            c.lookType = a.value(QLatin1String("looktype")).toInt();
            c.lookItem = a.value(QLatin1String("lookitem")).toInt();
            c.lookHead = a.value(QLatin1String("lookhead")).toInt();
            c.lookBody = a.value(QLatin1String("lookbody")).toInt();
            c.lookLegs = a.value(QLatin1String("looklegs")).toInt();
            c.lookFeet = a.value(QLatin1String("lookfeet")).toInt();
            m_creatures.push_back(std::move(c));
        } else if (token == QXmlStreamReader::EndElement) {
            if (tag == QLatin1String("npcs")) inNpcsSection = false;
        }
    }
    return !xml.hasError();
}

const CreatureStore::CreatureType *CreatureStore::byName(const QString &name) const
{
    for (const CreatureType &c : m_creatures)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0) return &c;
    return nullptr;
}

int CreatureStore::rowForName(const QString &name) const
{
    for (int row = 0; row < m_creatures.size(); ++row)
        if (m_creatures[row].name.compare(name, Qt::CaseInsensitive) == 0)
            return row;
    return -1;
}

QVariantMap CreatureStore::creatureAt(int row) const
{
    QVariantMap result;
    if (row < 0 || row >= m_creatures.size()) return result;
    const CreatureType &c = m_creatures[row];
    result.insert(QStringLiteral("name"), c.name);
    result.insert(QStringLiteral("isNpc"), c.isNpc);
    result.insert(QStringLiteral("lookType"), c.lookType);
    result.insert(QStringLiteral("lookItem"), c.lookItem);
    result.insert(QStringLiteral("lookHead"), c.lookHead);
    result.insert(QStringLiteral("lookBody"), c.lookBody);
    result.insert(QStringLiteral("lookLegs"), c.lookLegs);
    result.insert(QStringLiteral("lookFeet"), c.lookFeet);
    return result;
}

bool CreatureStore::saveFile()
{
    if (m_path.isEmpty()) {
        setErrorString(QStringLiteral("No client profile is loaded."));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        setErrorString(QStringLiteral("Cannot create the creature data directory."));
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        setErrorString(file.errorString());
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("creatures"));
    for (const CreatureType &c : m_creatures) {
        xml.writeStartElement(QStringLiteral("creature"));
        xml.writeAttribute(QStringLiteral("name"), c.name);
        xml.writeAttribute(QStringLiteral("type"),
                           c.isNpc ? QStringLiteral("npc")
                                   : QStringLiteral("monster"));
        xml.writeAttribute(QStringLiteral("looktype"), QString::number(c.lookType));
        xml.writeAttribute(QStringLiteral("lookitem"), QString::number(c.lookItem));
        xml.writeAttribute(QStringLiteral("lookhead"), QString::number(c.lookHead));
        xml.writeAttribute(QStringLiteral("lookbody"), QString::number(c.lookBody));
        xml.writeAttribute(QStringLiteral("looklegs"), QString::number(c.lookLegs));
        xml.writeAttribute(QStringLiteral("lookfeet"), QString::number(c.lookFeet));
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError() || !file.commit()) {
        setErrorString(file.errorString().isEmpty()
                           ? QStringLiteral("Could not save creatures.xml.")
                           : file.errorString());
        return false;
    }
    setErrorString(QString());
    return true;
}

bool CreatureStore::saveCreature(const QString &originalName,
                                 const QString &name, bool isNpc,
                                 int lookType, int lookItem,
                                 int lookHead, int lookBody,
                                 int lookLegs, int lookFeet)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty() || lookType < 0 || lookItem < 0) return false;
    const int existing = rowForName(cleanName);
    const int original = rowForName(originalName);
    if (existing >= 0 && existing != original) {
        setErrorString(QStringLiteral("A creature named \"%1\" already exists.")
                           .arg(cleanName));
        return false;
    }

    const QVector<CreatureType> previous = m_creatures;
    beginResetModel();
    CreatureType value;
    value.name = cleanName;
    value.isNpc = isNpc;
    value.lookType = std::clamp(lookType, 0, 65535);
    value.lookItem = std::clamp(lookItem, 0, 65535);
    value.lookHead = std::clamp(lookHead, 0, 255);
    value.lookBody = std::clamp(lookBody, 0, 255);
    value.lookLegs = std::clamp(lookLegs, 0, 255);
    value.lookFeet = std::clamp(lookFeet, 0, 255);
    if (original >= 0) m_creatures[original] = value;
    else m_creatures.append(value);
    std::sort(m_creatures.begin(), m_creatures.end(),
              [](const CreatureType &a, const CreatureType &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    endResetModel();
    const bool ok = saveFile();
    if (!ok) {
        beginResetModel();
        m_creatures = previous;
        endResetModel();
    }
    emit countChanged();
    return ok;
}

bool CreatureStore::removeCreature(const QString &name)
{
    const int row = rowForName(name);
    if (row < 0) return false;
    const QVector<CreatureType> previous = m_creatures;
    beginResetModel();
    m_creatures.removeAt(row);
    endResetModel();
    const bool ok = saveFile();
    if (!ok) {
        beginResetModel();
        m_creatures = previous;
        endResetModel();
    }
    emit countChanged();
    return ok;
}

namespace {
bool readCreatureDefinition(const QString &path,
                            CreatureStore::CreatureType &creature,
                            QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement()) {
        if (error) *error = QStringLiteral("Empty XML file.");
        return false;
    }
    const QString root = xml.name().toString().toLower();
    if (root != QLatin1String("monster") && root != QLatin1String("npc")
        && root != QLatin1String("creature")) {
        if (error) *error = QStringLiteral("Not a monster or NPC definition.");
        return false;
    }
    const auto rootAttributes = xml.attributes();
    creature.name = rootAttributes.value(QLatin1String("name")).toString();
    creature.isNpc = root == QLatin1String("npc")
                     || rootAttributes.value(QLatin1String("type"))
                            .toString().compare(QLatin1String("npc"),
                                                Qt::CaseInsensitive) == 0;
    while (xml.readNextStartElement()) {
        const QString tag = xml.name().toString().toLower();
        if (tag == QLatin1String("look") || tag == QLatin1String("outfit")) {
            const auto a = xml.attributes();
            creature.lookType =
                a.value(QLatin1String("type")).toInt();
            if (creature.lookType == 0)
                creature.lookType =
                    a.value(QLatin1String("looktype")).toInt();
            creature.lookItem =
                a.value(QLatin1String("typeex")).toInt();
            if (creature.lookItem == 0)
                creature.lookItem =
                    a.value(QLatin1String("lookitem")).toInt();
            creature.lookHead = a.value(QLatin1String("head")).toInt();
            creature.lookBody = a.value(QLatin1String("body")).toInt();
            creature.lookLegs = a.value(QLatin1String("legs")).toInt();
            creature.lookFeet = a.value(QLatin1String("feet")).toInt();
            xml.skipCurrentElement();
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError() || creature.name.trimmed().isEmpty()) {
        if (error) {
            *error = xml.hasError() ? xml.errorString()
                                    : QStringLiteral("Creature name is missing.");
        }
        return false;
    }
    return true;
}
}

QVariantMap CreatureStore::importOtFile(const QString &pathOrUrl)
{
    QVariantMap result;
    QString path = QUrl(pathOrUrl).toLocalFile();
    if (path.isEmpty()) path = pathOrUrl;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement()) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("Empty XML file."));
        return result;
    }

    QVector<CreatureType> imported;
    QStringList failures;
    if (xml.name().compare(QLatin1String("monsters"),
                           Qt::CaseInsensitive) == 0) {
        const QDir directory = QFileInfo(path).dir();
        while (xml.readNextStartElement()) {
            if (xml.name().compare(QLatin1String("monster"),
                                   Qt::CaseInsensitive) != 0) {
                xml.skipCurrentElement();
                continue;
            }
            const QString relative =
                xml.attributes().value(QLatin1String("file")).toString();
            const QString listedName =
                xml.attributes().value(QLatin1String("name")).toString();
            xml.skipCurrentElement();
            if (relative.isEmpty()) continue;
            CreatureType creature;
            QString error;
            if (readCreatureDefinition(directory.filePath(relative),
                                       creature, &error)) {
                if (creature.name.isEmpty()) creature.name = listedName;
                imported.append(creature);
            } else {
                failures.append(QStringLiteral("%1: %2").arg(relative, error));
            }
        }
    } else {
        file.close();
        CreatureType creature;
        QString error;
        if (readCreatureDefinition(path, creature, &error)) {
            imported.append(creature);
        } else {
            failures.append(error);
        }
    }

    if (imported.isEmpty()) {
        const QString error = failures.isEmpty()
                                  ? QStringLiteral("No creature definitions found.")
                                  : failures.join(QLatin1Char('\n'));
        setErrorString(error);
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), error);
        return result;
    }

    const QVector<CreatureType> previous = m_creatures;
    beginResetModel();
    for (const CreatureType &creature : imported) {
        int row = -1;
        for (int i = 0; i < m_creatures.size(); ++i) {
            if (m_creatures[i].name.compare(creature.name,
                                            Qt::CaseInsensitive) == 0) {
                row = i;
                break;
            }
        }
        if (row >= 0) m_creatures[row] = creature;
        else m_creatures.append(creature);
    }
    std::sort(m_creatures.begin(), m_creatures.end(),
              [](const CreatureType &a, const CreatureType &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    endResetModel();
    const bool saved = saveFile();
    if (!saved) {
        beginResetModel();
        m_creatures = previous;
        endResetModel();
    }
    emit countChanged();
    result.insert(QStringLiteral("success"), saved);
    result.insert(QStringLiteral("imported"), imported.size());
    result.insert(QStringLiteral("failed"), failures.size());
    result.insert(QStringLiteral("error"),
                  saved ? failures.join(QLatin1Char('\n')) : m_errorString);
    return result;
}

int CreatureStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_creatures.size());
}

QVariant CreatureStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_creatures.size())
        return {};
    const CreatureType &c = m_creatures[index.row()];
    switch (role) {
    case NameRole:     return c.name;
    case IsNpcRole:    return c.isNpc;
    case LookTypeRole: return c.lookType;
    case LookItemRole: return c.lookItem;
    default:           return {};
    }
}

QHash<int, QByteArray> CreatureStore::roleNames() const
{
    return {
        { NameRole,     QByteArrayLiteral("name") },
        { IsNpcRole,    QByteArrayLiteral("isNpc") },
        { LookTypeRole, QByteArrayLiteral("lookType") },
        { LookItemRole, QByteArrayLiteral("lookItem") },
    };
}

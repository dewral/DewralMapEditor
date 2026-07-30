#include "itemsxmlreader.h"

#include "dmedatadir.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

ItemsXmlReader::ItemsXmlReader(QObject *parent)
    : QObject(parent)
{
}

void ItemsXmlReader::clear()
{
    if (m_items.isEmpty()) return;
    m_items.clear();
    emit loadedChanged();
}

bool ItemsXmlReader::loadForVersion(int version)
{
    return loadForDir(QString::number(version));
}

bool ItemsXmlReader::loadForDir(const QString &dirName)
{
    const QString path = QDir(dmeDataDir())
                             .filePath(QStringLiteral("%1/items.xml").arg(dirName));
    if (!QFile::exists(path)) {
        clear();
        return false;
    }
    return loadFile(path);
}

bool ItemsXmlReader::loadFile(const QString &path)
{
    m_items.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit loadedChanged();
        return false;
    }

    QXmlStreamReader xml(&f);

    QVector<int> currentIds;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const auto tag = xml.name();
            if (tag == QLatin1String("item")) {
                currentIds.clear();
                const auto a = xml.attributes();
                const QString name = a.value(QLatin1String("name")).toString();

                if (a.hasAttribute(QLatin1String("id"))) {
                    const int id = a.value(QLatin1String("id")).toInt();
                    if (id > 0) currentIds.append(id);
                } else if (a.hasAttribute(QLatin1String("fromid"))
                           && a.hasAttribute(QLatin1String("toid"))) {
                    const int from = a.value(QLatin1String("fromid")).toInt();
                    const int to = a.value(QLatin1String("toid")).toInt();

                    if (from > 0 && to >= from && (to - from) <= 65535) {
                        for (int id = from; id <= to; ++id) currentIds.append(id);
                    }
                }

                for (int id : currentIds) {
                    Entry &e = m_items[id];
                    if (!name.isEmpty()) e.name = name;
                }
            } else if (tag == QLatin1String("attribute") && !currentIds.isEmpty()) {
                const auto a = xml.attributes();
                const QString key =
                    a.value(QLatin1String("key")).toString().toLower();
                if (key == QLatin1String("type")) {
                    const QString type = a.value(QLatin1String("value")).toString().toLower();
                    for (int id : currentIds) m_items[id].type = type;
                } else if (key == QLatin1String("rotateto")) {
                    const int rotateTo =
                        a.value(QLatin1String("value")).toInt();
                    if (rotateTo > 0 && rotateTo <= 65535) {
                        for (int id : currentIds) {
                            m_items[id].rotateTo = rotateTo;
                        }
                    }
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == QLatin1String("item")) currentIds.clear();
        }
    }

    emit loadedChanged();
    return !xml.hasError();
}

QString ItemsXmlReader::nameForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? QString() : it->name;
}

QString ItemsXmlReader::typeForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? QString() : it->type;
}

int ItemsXmlReader::rotateToForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? 0 : it->rotateTo;
}

bool ItemsXmlReader::isTeleport(int serverId) const
{
    return typeForServerId(serverId) == QLatin1String("teleport");
}

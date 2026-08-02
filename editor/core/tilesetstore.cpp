#include "tilesetstore.h"

#include "dmedatadir.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QSaveFile>

TilesetStore::TilesetStore(QObject *parent)
    : QObject(parent)
{
}

void TilesetStore::clear()
{
    m_path.clear();
    m_names.clear();
    m_items.clear();
    bump();
}

namespace {
constexpr const char *kCategories[] = {
    "terrain", "doodad", "item", "raw", "collection", "door"
};
}

bool TilesetStore::loadJsonInto(const QString &path,
                                 QHash<QString, QStringList> &names,
                                 QHash<QString, QHash<QString, QVariantList>> &items)
{
    if (!QFile::exists(path)) return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    bool any = false;
    for (const char *catC : kCategories) {
        const QString cat = QString::fromLatin1(catC);
        if (!root.contains(cat) || !root.value(cat).isObject()) continue;
        const QJsonObject catObj = root.value(cat).toObject();
        for (auto it = catObj.begin(); it != catObj.end(); ++it) {
            if (!it.value().isArray()) continue;
            QVariantList ids;
            for (const QJsonValue &v : it.value().toArray()) ids.append(v.toInt());
            names[cat].append(it.key());
            items[cat][it.key()] = ids;
            any = true;
        }
    }
    return any;
}

void TilesetStore::setErrorString(const QString &message)
{
    if (m_errorString == message) return;
    m_errorString = message;
    emit errorStringChanged();
}

bool TilesetStore::saveJson()
{
    if (m_path.isEmpty()) {
        setErrorString(QStringLiteral("No data profile was selected for tilesets."));
        return false;
    }

    QJsonObject root;
    for (const char *catC : kCategories) {
        const QString cat = QString::fromLatin1(catC);
        const auto namesIt = m_names.find(cat);
        if (namesIt == m_names.end() || namesIt->isEmpty()) continue;
        QJsonObject catObj;
        const auto &itemsForCat = m_items.value(cat);
        for (const QString &name : *namesIt) {
            QJsonArray arr;
            for (const QVariant &id : itemsForCat.value(name)) arr.append(id.toInt());
            catObj.insert(name, arr);
        }
        root.insert(cat, catObj);
    }

    const QString dir = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(dir)) {
        setErrorString(QStringLiteral("Cannot create directory: %1").arg(dir));
        return false;
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) {
        setErrorString(QStringLiteral("Cannot open %1: %2").arg(m_path, f.errorString()));
        return false;
    }
    if (f.write(json) != json.size()) {
        setErrorString(QStringLiteral("Cannot write %1: %2").arg(m_path, f.errorString()));
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        setErrorString(QStringLiteral("Cannot commit %1: %2").arg(m_path, f.errorString()));
        return false;
    }
    setErrorString(QString());
    return true;
}

bool TilesetStore::loadForVersion(int clientVersion)
{
    return loadForDir(QString::number(clientVersion));
}

bool TilesetStore::loadForDir(const QString &dirName)
{
    clear();
    m_path = QDir(dmeDataDir()).filePath(QStringLiteral("%1/tilesets.json").arg(dirName));
    const bool has = loadJsonInto(m_path, m_names, m_items);
    bump();
    return has;
}

QStringList TilesetStore::namesFor(const QString &category) const
{
    return m_names.value(category);
}

QVariantList TilesetStore::itemsFor(const QString &category, const QString &name) const
{
    return m_items.value(category).value(name);
}

QString TilesetStore::tilesetForItem(const QString &category, int serverId) const
{
    const QStringList names = m_names.value(category);
    const auto categoryItems = m_items.constFind(category);
    if (categoryItems == m_items.cend()) return {};

    for (const QString &name : names) {
        const QVariantList items = categoryItems->value(name);
        for (const QVariant &item : items)
            if (item.toInt() == serverId) return name;
    }
    return {};
}

bool TilesetStore::isCustomOnly(const QString &category, const QString &name) const
{
    return m_names.value(category).contains(name);
}

bool TilesetStore::newTileset(const QString &category, const QString &name)
{
    if (name.isEmpty()) return false;
    if (m_names.value(category).contains(name)) return false;
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    m_names[category].append(name);
    m_items[category][name] = {};
    if (!saveJson()) {
        m_names = oldNames;
        m_items = oldItems;
        return false;
    }
    bump();
    return true;
}

bool TilesetStore::deleteTileset(const QString &category, const QString &name)
{
    if (!m_names.value(category).contains(name)) return false;
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    m_names[category].removeAll(name);
    m_items[category].remove(name);
    if (!saveJson()) {
        m_names = oldNames;
        m_items = oldItems;
        return false;
    }
    bump();
    return true;
}

bool TilesetStore::addItem(const QString &category, const QString &name, int serverId)
{
    if (name.isEmpty()) return false;
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    if (!m_names.value(category).contains(name)) m_names[category].append(name);
    QVariantList &list = m_items[category][name];
    if (list.contains(serverId)) return true;
    list.append(serverId);
    if (!saveJson()) {
        m_names = oldNames;
        m_items = oldItems;
        return false;
    }
    bump();
    return true;
}

bool TilesetStore::removeItem(const QString &category, const QString &name, int serverId)
{
    if (!m_items.value(category).value(name).contains(serverId)) return false;
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    QVariantList &list = m_items[category][name];
    list.removeAll(QVariant(serverId));
    if (!saveJson()) {
        m_names = oldNames;
        m_items = oldItems;
        return false;
    }
    bump();
    return true;
}

#include "tilesetstore.h"
#include "otbreader.h"

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
    auto names = m_names.value(category);
    if (category == QLatin1String("raw")) {
        names.removeAll(QStringLiteral("Others"));
        if (m_itemSource || !m_rawOthers.isEmpty()) names.append(QStringLiteral("Others"));
    }
    return names;
}

QVariantList TilesetStore::itemsFor(const QString &category, const QString &name) const
{
    if (category == QLatin1String("raw") && name == QLatin1String("Others")) return m_rawOthers;
    return m_items.value(category).value(name);
}

QString TilesetStore::tilesetForItem(const QString &category, int serverId) const
{
    const QStringList names = m_names.value(category);
    const auto categoryItems = m_items.constFind(category);
    if (categoryItems == m_items.cend()) return category == QLatin1String("raw") && m_rawOtherIds.contains(serverId) ? QStringLiteral("Others") : QString();

    for (const QString &name : names) {
        if (category == QLatin1String("raw") && name == QLatin1String("Others")) continue;
        const QVariantList items = categoryItems->value(name);
        for (const QVariant &item : items)
            if (item.toInt() == serverId) return name;
    }
    return category == QLatin1String("raw") && m_rawOtherIds.contains(serverId) ? QStringLiteral("Others") : QString();
}

bool TilesetStore::isCustomOnly(const QString &category, const QString &name) const
{
    return !(category == QLatin1String("raw") && name == QLatin1String("Others")) && m_names.value(category).contains(name);
}

bool TilesetStore::newTileset(const QString &category, const QString &name,
                             const QVariantList &serverIds)
{
    if (category == QLatin1String("raw") && name.trimmed() == QLatin1String("Others")) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) {
        setErrorString(QStringLiteral("Enter a name."));
        return false;
    }
    if (m_names.value(category).contains(cleanName)) {
        setErrorString(QStringLiteral("A tileset named '%1' already exists.").arg(cleanName));
        return false;
    }
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    m_names[category].append(cleanName);
    QVariantList ids;
    QSet<int> seen;
    for (const QVariant &value : serverIds) {
        const int id = value.toInt();
        if (id <= 0 || seen.contains(id)) continue;
        ids.append(id);
        seen.insert(id);
    }
    m_items[category][cleanName] = ids;
    if (!saveJson()) {
        m_names = oldNames;
        m_items = oldItems;
        return false;
    }
    bump();
    return true;
}

bool TilesetStore::renameTileset(const QString &category, const QString &name,
                                 const QString &newName)
{
    if (category == QLatin1String("raw") && (name.trimmed() == QLatin1String("Others")
                                            || newName.trimmed() == QLatin1String("Others"))) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
    const QString cleanName = name.trimmed();
    const QString cleanNewName = newName.trimmed();
    if (cleanName.isEmpty() || cleanNewName.isEmpty()) return false;
    QStringList &names = m_names[category];
    const int index = names.indexOf(cleanName);
    if (index < 0) return false;
    if (cleanName == cleanNewName) return true;
    if (names.contains(cleanNewName)) {
        setErrorString(QStringLiteral("A tileset named '%1' already exists.").arg(cleanNewName));
        return false;
    }

    const auto oldNames = m_names;
    const auto oldItems = m_items;
    names[index] = cleanNewName;
    m_items[category].insert(cleanNewName, m_items[category].take(cleanName));
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
    if (category == QLatin1String("raw") && name.trimmed() == QLatin1String("Others")) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
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
    if (category == QLatin1String("raw") && name.trimmed() == QLatin1String("Others")) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
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

bool TilesetStore::addItems(const QString &category, const QString &name,
                            const QVariantList &serverIds)
{
    if (category == QLatin1String("raw") && name.trimmed() == QLatin1String("Others")) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
    if (name.isEmpty() || serverIds.isEmpty()) return false;
    const auto oldNames = m_names;
    const auto oldItems = m_items;
    if (!m_names.value(category).contains(name)) m_names[category].append(name);
    QVariantList &list = m_items[category][name];
    bool changed = false;
    for (const QVariant &value : serverIds) {
        const int serverId = value.toInt();
        if (serverId <= 0 || list.contains(serverId)) continue;
        list.append(serverId);
        changed = true;
    }
    if (!changed) return true;
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
    if (category == QLatin1String("raw") && name.trimmed() == QLatin1String("Others")) {
        setErrorString(QStringLiteral("RAW Others is managed automatically. Choose a named RAW tileset."));
        return false;
    }
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

void TilesetStore::setItemSource(OtbReader *source)
{
    if (source == m_itemSource) return;
    if (m_itemSource) disconnect(m_itemSource, nullptr, this, nullptr);
    m_itemSource = source;
    if (source) {
        connect(source, &QAbstractItemModel::modelReset, this, &TilesetStore::bump);
        connect(source, &OtbReader::loadedChanged, this, &TilesetStore::bump);
    }
    bump();
}

void TilesetStore::bump()
{
    if (m_preparedPublication) return;
    rebuildRawOthers();
    ++m_revision;
    emit tilesetsChanged();
}

void TilesetStore::rebuildRawOthers()
{
    m_rawOthers.clear(); m_rawOtherIds.clear();
    if (m_itemSource) {
        // Others is the complete RAW catalogue, including categorized items.
        for (int row = 0; row < m_itemSource->rowCount(); ++row) {
            const int id = m_itemSource->data(m_itemSource->index(row, 0), OtbReader::ServerIdRole).toInt();
            if (id > 0) {
                m_rawOthers.append(id); m_rawOtherIds.insert(id);
            }
        }
    } else {
        const auto raw = m_items.value(QStringLiteral("raw"));
        for (const auto &value : raw.value(QStringLiteral("Others"))) {
            const int id = value.toInt();
            if (id > 0 && !m_rawOtherIds.contains(id)) {
                m_rawOthers.append(id); m_rawOtherIds.insert(id);
            }
        }
    }
}

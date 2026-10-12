#include "customgeneratorprofiles.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QSet>
#include <cmath>

namespace {
QVariantMap fail(const QString &error) { return {{"success",false},{"error",error}}; }
bool number(const QVariant &v,int min,int max) {
    bool ok=false; const double n=v.toDouble(&ok);
    return ok && std::isfinite(n) && n>=min && n<=max && std::floor(n)==n;
}
}
QStringList CustomGeneratorProfiles::roles()
{
    QStringList result{"background","floor","secondary","decorations"};
    for (int i=1;i<=4;++i) result.append("wall"+QString::number(i));
    for (int i=1;i<=12;++i) result.append("border"+QString::number(i));
    return result;
}
QSet<int> CustomGeneratorProfiles::itemIds(const QVariantMap &profile)
{
    QSet<int> result;
    auto appendMaterials=[&](const QVariantMap &materials) {
        for (const QVariant &pool : materials)
            for (const QVariant &entry : pool.toList()) result.insert(entry.toMap().value("id").toInt());
    };
    appendMaterials(profile.value("materials").toMap());
    for (const QVariant &value : profile.value("groundBorders").toList()) {
        const auto set=value.toMap();
        appendMaterials(set.value("materials").toMap());
        for (const QVariant &id : set.value("toGroundIds").toList()) result.insert(id.toInt());
    }
    return result;
}
QString CustomGeneratorProfiles::validate(const QVariantMap &p,const QVariantMap *info)
{
    if (!number(p.value("version"),1,1)) return "Unsupported material profile version.";
    if (p.value("name").toString().trimmed().isEmpty() || p.value("name").toString().size()>120)
        return "Enter a profile name (up to 120 characters).";
    if (p.value("kind")!="dungeon" && p.value("kind")!="cave") return "Choose Dungeon or Cave.";
    for (const QString &field : {QString("width"),QString("height")})
        if (!number(p.value(field),24,200)) return "Map dimensions must be between 24 and 200.";
    if (!number(p.value("rooms"),2,16) || !number(p.value("seed"),1,2147483647)
        || !number(p.value("density"),0,30)) return "Invalid room count, seed or decoration density.";
    if (!number(p.value("x"),0,65535) || !number(p.value("y"),0,65535)
        || !number(p.value("z"),0,15)) return "Invalid destination coordinates.";
    if (p.value("x").toInt()+p.value("width").toInt()>65536
        || p.value("y").toInt()+p.value("height").toInt()>65536) return "The map exceeds coordinate limits.";
    const int capacity=((p.value("width").toInt()-16)/8+1)*((p.value("height").toInt()-16)/8+1);
    if (p.value("rooms").toInt()>capacity) return "Too many rooms for this area. Increase the map size.";
    if (p.value("materials").metaType().id()!=QMetaType::QVariantMap) return "Invalid material categories.";
    const auto materials=p.value("materials").toMap();
    const auto known=roles();
    auto validateMaterials=[&](const QVariantMap &pools,bool bordersOnly) -> QString {
        for (auto it=pools.cbegin();it!=pools.cend();++it) {
            if (!known.contains(it.key()) || (bordersOnly && !it.key().startsWith("border")))
                return "Unknown material category: "+it.key();
            if (it.value().metaType().id()!=QMetaType::QVariantList || it.value().toList().size()>64)
                return "Invalid material list: "+it.key();
            QSet<int> ids;
            for (const QVariant &v : it.value().toList()) {
                const auto entry=v.toMap();
                if (!number(entry.value("id"),1,65535) || !number(entry.value("weight"),1,100))
                    return "Item IDs must be 1–65535 and weights 1–100.";
                const int id=entry.value("id").toInt();
                if (ids.contains(id)) return "Duplicate item in category: "+it.key();
                ids.insert(id);
                if (!info) continue;
                const auto metadata=info->value(QString::number(id)).toMap();
                if (!metadata.value("valid").toBool()) return QString("Item %1 is missing from the loaded client.").arg(id);
                const bool groundRole=it.key()=="background" || it.key()=="floor" || it.key()=="secondary";
                if (metadata.value("ground").toBool()!=groundRole)
                    return QString("Item %1 has the wrong type for %2.").arg(id).arg(it.key());
                if ((it.key()=="floor" || it.key()=="secondary") && metadata.value("blocked").toBool())
                    return QString("Floor item %1 blocks walking.").arg(id);
            }
        }
        return {};
    };
    const QString materialsError=validateMaterials(materials,false);
    if (!materialsError.isEmpty()) return materialsError;
    bool hasGroundBorders=false;
    if (p.contains("groundBorders")) {
        if (p.value("groundBorders").metaType().id()!=QMetaType::QVariantList || p.value("groundBorders").toList().size()>128)
            return "Invalid ground border sets.";
        QSet<int> grounds;
        for (const QString &role : {QString("background"),QString("floor"),QString("secondary")})
            for (const QVariant &entry : materials.value(role).toList()) grounds.insert(entry.toMap().value("id").toInt());
        for (const QVariant &value : p.value("groundBorders").toList()) {
            const auto set=value.toMap();
            if (set.value("name").toString().trimmed().isEmpty() || set.value("name").toString().size()>240
                || (set.value("align")!="outer" && set.value("align")!="inner")
                || !number(set.value("zorder"),-2147483647,2147483647) || set.value("to").toString().size()>120)
                return "Invalid ground border name, alignment or target.";
            const auto ids=set.value("groundIds").toList();
            if (set.value("groundIds").metaType().id()!=QMetaType::QVariantList || ids.isEmpty() || ids.size()>64)
                return "Assign each border set to its ground items.";
            QSet<int> owners;
            for (const QVariant &id : ids) {
                if (!number(id,1,65535) || !grounds.contains(id.toInt()) || owners.contains(id.toInt()))
                    return "A border set must belong to ground items in this profile.";
                owners.insert(id.toInt());
            }
            if (set.value("toGroundIds").metaType().id()!=QMetaType::QVariantList || set.value("toGroundIds").toList().size()>64)
                return "Invalid border target ground items.";
            for (const QVariant &id : set.value("toGroundIds").toList()) {
                if (!number(id,1,65535)) return "Invalid border target item ID.";
                if (info) {
                    const auto item=info->value(QString::number(id.toInt())).toMap();
                    if (!item.value("valid").toBool() || !item.value("ground").toBool()) return "A border target ground is missing from this client.";
                }
            }
            if (set.value("materials").metaType().id()!=QMetaType::QVariantMap) return "Invalid ground border materials.";
            const auto pools=set.value("materials").toMap();
            const QString error=validateMaterials(pools,true);
            if (!error.isEmpty()) return error;
            for (const QVariant &pool : pools) hasGroundBorders |= !pool.toList().isEmpty();
        }
    }
    if (info) {
        if (materials.value("background").toList().isEmpty() || materials.value("floor").toList().isEmpty())
            return "Add background and floor materials before generating.";
        for (const auto &group : {qMakePair(QString("wall"),4),qMakePair(QString("border"),12)}) {
            if (!p.value(group.first+"sEnabled").toBool()) continue;
            if (group.first=="border" && hasGroundBorders) {
                bool hasDefault=false;
                for (int i=1;i<=12;++i) hasDefault |= !materials.value("border"+QString::number(i)).toList().isEmpty();
                if (!hasDefault) continue;
            }
            for (int i=1;i<=group.second;++i)
                if (materials.value(group.first+QString::number(i)).toList().isEmpty())
                    return QString("Fill every enabled %1 slot (missing %2).").arg(group.first).arg(i);
        }
    }
    return {};
}
CustomGeneratorProfiles::CustomGeneratorProfiles(QObject *parent,const QString &directory) : QObject(parent)
{
    m_directory=directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        +"/custom-generator" : directory;
    QFile file(m_directory+"/profiles.json");
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) { m_error=file.errorString(); return; }
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    const auto root=doc.object().toVariantMap();
    if (error.error!=QJsonParseError::NoError || root.value("version").toInt()!=1
        || root.value("profiles").metaType().id()!=QMetaType::QVariantList) {
        m_error="Cannot read the profile library; the existing file will not be overwritten."; return;
    }
    QSet<QString> ids;
    for (const QVariant &v : root.value("profiles").toList()) {
        const auto p=v.toMap(); const QString id=p.value("id").toString();
        if (!validate(p).isEmpty() || id.isEmpty() || ids.contains(id)) {
            m_error="The profile library contains invalid data; it will not be overwritten."; return;
        }
        ids.insert(id);
    }
    m_profiles=root.value("profiles").toList();
}
QVariantList CustomGeneratorProfiles::profiles() const { return m_profiles; }
QVariantMap CustomGeneratorProfiles::load(const QString &id) const {
    for (const QVariant &v : m_profiles) if (v.toMap().value("id")==id) return v.toMap();
    return {};
}
bool CustomGeneratorProfiles::persist(const QVariantList &profiles,QString &error) const {
    if (!m_error.isEmpty()) { error=m_error; return false; }
    if (!QDir().mkpath(m_directory)) { error="Cannot create profile directory."; return false; }
    QSaveFile file(m_directory+"/profiles.json");
    const QByteArray data=QJsonDocument::fromVariant(QVariantMap{{"version",1},{"profiles",profiles}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) {
        error=file.errorString(); return false;
    }
    return true;
}
QVariantMap CustomGeneratorProfiles::save(const QVariantMap &profile,const QString &id) {
    const QString error=validate(profile); if (!error.isEmpty()) return fail(error);
    QVariantMap stored=profile; stored.remove("itemInfo");
    stored["id"]=id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
    QVariantList next=m_profiles;
    bool replaced=false;
    for (QVariant &v : next) if (v.toMap().value("id")==stored.value("id")) { v=stored; replaced=true; break; }
    if (!id.isEmpty() && !replaced) return fail("Profile no longer exists. Save it as a new profile.");
    if (!replaced) next.append(stored);
    QString writeError; if (!persist(next,writeError)) return fail(writeError);
    m_profiles=next; emit profilesChanged();
    return {{"success",true},{"profile",stored}};
}
QVariantMap CustomGeneratorProfiles::remove(const QString &id) {
    QVariantList next;
    for (const QVariant &v : m_profiles) if (v.toMap().value("id")!=id) next.append(v);
    if (next.size()==m_profiles.size()) return fail("Profile not found.");
    QString error; if (!persist(next,error)) return fail(error);
    m_profiles=next; emit profilesChanged(); return {{"success",true}};
}
QVariantMap CustomGeneratorProfiles::importProfile(const QUrl &url) {
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly) || file.size()>1024*1024) return fail("Cannot read profile or file is too large.");
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    if (error.error!=QJsonParseError::NoError || !doc.isObject()) return fail("Invalid profile JSON.");
    return save(doc.object().toVariantMap());
}
QVariantMap CustomGeneratorProfiles::exportProfile(const QVariantMap &profile,const QUrl &url) const {
    const QString error=validate(profile); if (!error.isEmpty()) return fail(error);
    QVariantMap clean=profile; clean.remove("id"); clean.remove("itemInfo");
    QSaveFile file(url.toLocalFile());
    const QByteArray data=QJsonDocument::fromVariant(clean).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) return fail(file.errorString());
    return {{"success",true}};
}

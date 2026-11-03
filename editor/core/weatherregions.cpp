#include "weatherregions.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QPolygon>
#include <QSet>
#include <QLineF>
#include <QUrl>
#include <QCryptographicHash>
#include <QtConcurrent/QtConcurrentRun>
#include <QPromise>
#include <QTemporaryDir>
#include <QDateTime>
#include <QDir>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
QString localPath(const QString &value) { const QUrl url(value); return url.isLocalFile()?url.toLocalFile():value; }
bool idValid(const QString &id) { return QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,64}$")).match(id).hasMatch() && id!=QStringLiteral("global"); }
bool finite(const QJsonValue &v,double lo,double hi) { return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble()>=lo && v.toDouble()<=hi; }
bool validPreset(const QString &id,const QJsonObject &p) {
    if(!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{1,64}$")).match(id).hasMatch() || !p.value("effects").isObject())return false;
    const QStringList kinds={"rain","storm","snow","fog","leaves","fireflies"};
    for(const auto &key:kinds)if(p.value("effects").toObject().contains(key) && !finite(p.value("effects").toObject().value(key),0,100))return false;
    const QMap<QString,QPair<double,double>> ranges={{"wind",{-160,160}},{"variation",{0,100}},{"transition",{0,60}},
        {"rainLength",{4,24}},{"rainThickness",{1,6}},{"fogOpacity",{0,200}},
        {"fogScale",{0.25,3}},{"fogSpeed",{0,3}},{"fogTurbulence",{0,100}},{"gustPeriod",{2,60}},{"rainSpeed",{0.5,2}},{"rainOpacity",{0,100}}};
    for(auto it=ranges.cbegin();it!=ranges.cend();++it)if(p.contains(it.key()) && !finite(p.value(it.key()),it.value().first,it.value().second))return false;
    for(const auto &key:{"fogColor","rainColor"})if(p.contains(key) && !QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(p.value(key).toString()).hasMatch())return false;
    return true;
}
QJsonObject climate(const QString &preset) {
    return {{"defaultPreset",preset},{"presets",QJsonArray{QJsonObject{{"preset",preset},{"weight",1}}}},
        {"minDurationSeconds",600},{"maxDurationSeconds",1800}};
}
QJsonObject defaults() {
    return {{"version",1},{"presets",QJsonObject{
        {"clear",QJsonObject{{"effects",QJsonObject{}}}},
        {"gentle_mist",QJsonObject{{"effects",QJsonObject{{"fog",35}}},{"fogOpacity",75},{"wind",8}}},
        {"dense_fog",QJsonObject{{"effects",QJsonObject{{"fog",100}}},{"fogOpacity",135},{"fogScale",1.4},{"wind",5}}},
        {"drizzle",QJsonObject{{"effects",QJsonObject{{"rain",25},{"fog",8}}},{"rainOpacity",45},{"rainSpeed",0.7}}},
        {"rain",QJsonObject{{"effects",QJsonObject{{"rain",65},{"fog",15}}},{"wind",-30},{"transition",8}}},
        {"storm",QJsonObject{{"effects",QJsonObject{{"storm",85},{"fog",30}}},{"wind",-90},{"transition",10}}},
        {"snow",QJsonObject{{"effects",QJsonObject{{"snow",70},{"fog",20}}}}},
        {"leaves",QJsonObject{{"effects",QJsonObject{{"leaves",60}}},{"wind",35}}},
        {"fireflies",QJsonObject{{"effects",QJsonObject{{"fireflies",60}}}}},
        {"swamp_fog",QJsonObject{{"effects",QJsonObject{{"fog",65},{"fireflies",30}}},{"fogColor","#9FC8A0"}}}}},
        {"global",climate("clear")},{"regions",QJsonArray{}}};
}
bool validClimate(const QJsonObject &c,const QJsonObject &presets) {
    if (!presets.contains(c.value("defaultPreset").toString()) || !finite(c.value("minDurationSeconds"),1,604800)
        || !finite(c.value("maxDurationSeconds"),c.value("minDurationSeconds").toDouble(),604800)) return false;
    const auto weights=c.value("presets").toArray();
    if(weights.isEmpty() || weights.size()>128) return false;
    for(const auto &v:weights) { const auto w=v.toObject(); if(!presets.contains(w.value("preset").toString()) || !finite(w.value("weight"),0.000001,1000000))return false; }
    return true;
}
bool polygon(const QJsonArray &points,QRegion &result) {
    if(points.size()<3 || points.size()>128)return false;
    QVector<QPointF> p;double minY=65536,maxY=0,area=0;
    for(const auto &value:points){const auto xy=value.toArray();if(xy.size()!=2 || !finite(xy[0],0,65536) || !finite(xy[1],0,65536))return false;
        p.append(QPointF(xy[0].toDouble(),xy[1].toDouble()));minY=std::min(minY,p.last().y());maxY=std::max(maxY,p.last().y());}
    auto on=[](QPointF a,QPointF b,QPointF c){return std::abs((b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x()))<1e-8 && c.x()>=std::min(a.x(),b.x()) && c.x()<=std::max(a.x(),b.x()) && c.y()>=std::min(a.y(),b.y()) && c.y()<=std::max(a.y(),b.y());};
    for(int i=0;i<p.size();++i){const auto a=p[i],b=p[(i+1)%p.size()],prev=p[(i+p.size()-1)%p.size()];
        if(a==b || on(prev,a,b) || on(a,b,prev))return false;area+=a.x()*b.y()-b.x()*a.y();
        for(int j=i+1;j<p.size();++j){if(j==i+1 || (i==0 && j==p.size()-1))continue;const auto c=p[j],d=p[(j+1)%p.size()];QPointF out;
            if(QLineF(a,b).intersects(QLineF(c,d),&out)==QLineF::BoundedIntersection || on(a,b,c) || on(a,b,d) || on(c,d,a) || on(c,d,b))return false;}}
    if(std::abs(area)<1e-8)return false;
    auto interval=[&](double a,double b,int y){if(a>b)std::swap(a,b);const int left=std::max(0,int(std::ceil(a-0.5-1e-8))),right=std::min(65535,int(std::floor(b-0.5+1e-8)));if(right>=left)result|=QRect(left,y,right-left+1,1);};
    for(int y=std::max(0,int(std::ceil(minY-0.5)));y<=std::min(65535,int(std::floor(maxY-0.5)));++y){const double cy=y+0.5;QVector<double> cuts;
        for(int i=0;i<p.size();++i){const auto a=p[i],b=p[(i+1)%p.size()];
            if((a.y()>cy)!=(b.y()>cy))cuts.append(a.x()+(b.x()-a.x())*(cy-a.y())/(b.y()-a.y()));
            if(std::abs(a.y()-b.y())<1e-8){if(std::abs(cy-a.y())<1e-8)interval(a.x(),b.x(),y);}
            else if(cy>=std::min(a.y(),b.y()) && cy<=std::max(a.y(),b.y())){const double x=a.x()+(b.x()-a.x())*(cy-a.y())/(b.y()-a.y());interval(x,x,y);}}
        std::sort(cuts.begin(),cuts.end());for(int i=0;i+1<cuts.size();i+=2)interval(cuts[i],cuts[i+1],y);
    }
    return true;
}
}

WeatherRegionDocument::WeatherRegionDocument(OtbmReader *map):QObject(map),m_map(map),m_environment(defaults()) {
    setObjectName(QStringLiteral("weatherRegions"));m_lastPath=path();
    auto readCompanion=[this]{QFile file(path());if(file.open(QIODevice::ReadOnly))load(file.readAll());else m_error=file.errorString();};
    if(!path().isEmpty() && QFileInfo::exists(path()))readCompanion();
    connect(map,&OtbmReader::mapChanged,this,[this]{emit changed();});
    connect(map,&OtbmReader::zonesChanged,this,[this]{emit changed();});
    connect(map,&OtbmReader::zonesImported,this,&WeatherRegionDocument::importZoneWeather);
    connect(map,&OtbmReader::filePathChanged,this,[this,readCompanion]{
        if(m_lastPath.isEmpty() && !m_dirty && m_regions.isEmpty() && QFileInfo::exists(path()))readCompanion();
        else if(!m_regions.isEmpty() || m_dirty || QFileInfo::exists(m_lastPath)){m_dirty=true;save();}
        m_lastPath=path();emit changed();emit selectionChanged();
    });
}
void WeatherRegions::setMap(OtbmReader *map) {
    if(m_map==map) return;
    if(m_document)m_document->endStroke();
    m_map=map; m_document=nullptr;
    if(map) { m_document=map->findChild<WeatherRegionDocument*>(QStringLiteral("weatherRegions"),Qt::FindDirectChildrenOnly);
        if(!m_document) m_document=new WeatherRegionDocument(map); }
    emit documentChanged();
}
QString WeatherRegionDocument::path() const { return m_map->filePath().isEmpty()?QString():m_map->filePath()+QStringLiteral(".weather.json"); }
WeatherRegionDocument::Region *WeatherRegionDocument::current() { for(auto &r:m_regions)if(r.definition.value("id").toString()==m_selected)return &r; return nullptr; }
const WeatherRegionDocument::Region *WeatherRegionDocument::current() const { for(const auto &r:m_regions)if(r.definition.value("id").toString()==m_selected)return &r; return nullptr; }
void WeatherRegionDocument::setSelectedId(const QString &id) {
    if(m_selected==id)return;endStroke();
    if(id.startsWith("zone:")) {
        const int zone=id.mid(5).toInt();if(zone<1 || zone>65535)return;
        QString key="zone_"+QString::number(zone);int suffix=2;
        auto used=[&](const QString &v){for(const auto &r:m_regions)if(r.definition.value("id").toString()==v)return true;return false;};
        while(used(key))key="zone_"+QString::number(zone)+"_"+QString::number(suffix++);
        m_regions.append({QJsonObject{{"id",key},{"name","Strefa "+QString::number(zone)},{"zoneId",zone},{"priority",10},{"editorOnly",true}}, {}});
        m_selected=key;emit changed();
    }else m_selected=id;
    emit selectionChanged();
}
QVariantList WeatherRegionDocument::regions() const {
    QVariantList list;QSet<int> bound;
    for(const auto &r:m_regions){auto entry=r.definition.toVariantMap();const int zone=r.definition.value("zoneId").toInt();
        if(zone){bound.insert(zone);const auto areas=m_map->zoneAreas(zone);qint64 tiles=0;for(const auto &a:areas)tiles+=a.toMap().value("tiles").toLongLong();entry["areas"]=areas;entry["tileCount"]=tiles;}
        list.append(entry);}
    for(const auto &id:m_map->zoneIds())if(!bound.contains(id.toInt())){const int zone=id.toInt();const auto areas=m_map->zoneAreas(zone);qint64 tiles=0;for(const auto &a:areas)tiles+=a.toMap().value("tiles").toLongLong();
        list.append(QVariantMap{{"id","zone:"+QString::number(zone)},{"name","Strefa "+QString::number(zone)},{"zoneId",zone},{"areas",areas},{"tileCount",tiles}});}
    return list;
}
QStringList WeatherRegionDocument::presets() const { return m_environment.value("presets").toObject().keys(); }
QVariantMap WeatherRegionDocument::selected() const { const auto *r=current(); return r?r->definition.toVariantMap():QVariantMap{}; }
QVariantMap WeatherRegionDocument::preview() const {
    const auto *r=current(); const auto c=r && r->definition.contains("climate")?r->definition.value("climate").toObject():m_environment.value("global").toObject();
    auto result=previewPreset(c.value("defaultPreset").toString());
    result.insert("inherited",!r || !r->definition.contains("climate"));
    result.insert("minDurationSeconds",c.value("minDurationSeconds").toInt());
    result.insert("maxDurationSeconds",c.value("maxDurationSeconds").toInt());
    const auto weights=c.value("presets").toArray();
    bool stopAfter=weights.size()==1 && weights[0].toObject().value("preset")!=c.value("defaultPreset");
    if(stopAfter) {
        const auto next=m_environment.value("presets").toObject().value(weights[0].toObject().value("preset").toString()).toObject().value("effects").toObject();
        for(const auto &value:next)if(value.toDouble()!=0)stopAfter=false;
    }
    result.insert("stopAfter",stopAfter);
    return result;
}
bool WeatherRegionDocument::applyWeather(const QVariantMap &mix,int minimumSeconds,int maximumSeconds,bool stopAfter) {
    const auto *r=current();if(!r)return fail("Select a region first.");
    if(minimumSeconds<1 || maximumSeconds<minimumSeconds || maximumSeconds>604800)return fail("Weather duration must be 1–604800 seconds; maximum cannot be below minimum.");
    auto presets=m_environment.value("presets").toObject();
    const auto old=r->definition.value("climate").toObject();
    auto preset=presets.value(old.value("defaultPreset").toString()).toObject();
    QJsonObject effects;
    for(const auto &key:{"rain","storm","snow","fog","leaves","fireflies"})effects.insert(key,QJsonValue::fromVariant(mix.value(key,0)));
    preset.insert("effects",effects);preset.insert("wind",QJsonValue::fromVariant(mix.value("wind",0)));
    for(const auto &key:{"variation","transition","rainLength","rainThickness","fogOpacity","fogColor","fogScale","fogSpeed","fogTurbulence","gustPeriod","rainSpeed","rainOpacity","rainColor"})if(mix.contains(key))preset.insert(key,QJsonValue::fromVariant(mix.value(key)));
    const auto id=QStringLiteral("dme_")+QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(preset).toJson(QJsonDocument::Compact),QCryptographicHash::Sha1).toHex().left(20));
    if(!validPreset(id,preset))return fail("Invalid weather intensities or wind.");
    presets.insert(id,preset);
    const auto clearId=id+QStringLiteral("_clear");
    if(stopAfter)presets.insert(clearId,QJsonObject{{"effects",QJsonObject{}}});
    if(presets.size()>128)return fail("Maximum 128 presets. Remove unused presets before adding a custom mix.");
    const QJsonObject c={{"defaultPreset",id},{"presets",QJsonArray{QJsonObject{{"preset",stopAfter?clearId:id},{"weight",1}}}},
        {"minDurationSeconds",minimumSeconds},{"maxDurationSeconds",maximumSeconds}};
    auto definition=r->definition;definition.remove("editorOnly");definition.insert("climate",c);
    if(definition==r->definition && presets==m_environment.value("presets").toObject())return true;
    const auto before=snapshot();m_environment.insert("presets",presets);storeDefinition(definition);
    commit(before);emit selectionChanged();return true;
}
QVariantMap WeatherRegionDocument::previewPreset(const QString &id) const {
    const auto *r=current();
    const auto presets=m_environment.value("presets").toObject();
    const auto resolved=id.isEmpty()?m_environment.value("global").toObject().value("defaultPreset").toString():id;
    if(!presets.contains(resolved))return {};
    const auto p=presets.value(resolved).toObject();
    QVariantMap result=p.value("effects").toObject().toVariantMap();
    result.insert("presetId",resolved);
    result.insert("inherited",id.isEmpty());
    result.insert("enabled",true); result.insert("wind",p.value("wind").toDouble());
    for(auto it=p.constBegin();it!=p.constEnd();++it)if(it.key()!="effects")result[it.key()]=it.value().toVariant();
    result["indoorFog"]=r && r->definition.value("indoorFog").toBool();result["indoorFireflies"]=r && r->definition.value("indoorFireflies").toBool();
    result.insert("quality",1); result.insert("respectRoofs",true);
    return result;
}
WeatherRegionDocument::Snapshot WeatherRegionDocument::snapshot() const { return {m_environment,m_regions,m_selected}; }
void WeatherRegionDocument::restore(const Snapshot &s) { m_environment=s.environment; m_regions=s.regions; m_selected=s.selected; m_dirty=true; save(); notify(); emit selectionChanged(); }
void WeatherRegionDocument::notify() {
    QSet<int> ids;for(const auto &r:m_regions)if(r.definition.contains("zoneId"))ids.insert(r.definition.value("zoneId").toInt());m_map->setReservedZoneIds(ids);
    emit changed();emit m_map->dirtyChanged();
}
void WeatherRegionDocument::storeDefinition(const QJsonObject &definition) {current()->definition=definition;}
QRegion WeatherRegionDocument::geometry(const Region &r,int floor) const {return r.definition.contains("zoneId")?m_map->zoneGeometry(r.definition.value("zoneId").toInt(),floor):r.floors.value(floor);}
bool WeatherRegionDocument::canUndoGeometry() const {const auto *r=current();return r && r->definition.contains("zoneId")?m_map->undoCount()>0:canUndo();}
bool WeatherRegionDocument::canRedoGeometry() const {const auto *r=current();return r && r->definition.contains("zoneId")?m_map->redoCount()>0:canRedo();}
void WeatherRegionDocument::undoGeometry(){endStroke();const auto *r=current();if(r && r->definition.contains("zoneId"))m_map->undo();else undo();}
void WeatherRegionDocument::redoGeometry(){endStroke();const auto *r=current();if(r && r->definition.contains("zoneId"))m_map->redo();else redo();}
bool WeatherRegionDocument::fail(const QString &message) { m_error=message; emit changed(); return false; }
void WeatherRegionDocument::commit(const Snapshot &before) {
    m_undo.append(before); if(m_undo.size()>100)m_undo.removeFirst(); m_redo.clear(); m_dirty=true; save(); notify();
}
bool WeatherRegionDocument::createRegion(const QString &id,const QString &name) {
    if(!idValid(id) || name.trimmed().isEmpty() || name.toUtf8().size()>128)return fail("Enter a unique ID (letters, digits, _ or -) and a name.");
    for(const auto &r:m_regions)if(r.definition.value("id").toString()==id)return fail("This region ID already exists.");
    if(m_regions.size()>=1024)return fail("Maximum 1024 regions.");
    const int zone=m_map->nextZoneId();if(!zone)return fail("No free ZoneID.");
    const auto before=snapshot();
    m_map->setNumberedZonesEnabled(true);
    m_regions.append({QJsonObject{{"id",id},{"name",name},{"zoneId",zone},{"priority",10},{"indoorFog",false},{"indoorFireflies",false}}, {}});
    m_selected=id; commit(before); emit selectionChanged(); return true;
}
bool WeatherRegionDocument::applyRegion(const QVariantMap &options) {
    auto *r=current(); if(!r)return false;
    auto definition=r->definition;definition.remove("editorOnly");
    for(const auto &key:{"name","priority","indoorFog","indoorFireflies"})if(options.contains(key))definition.insert(key,QJsonValue::fromVariant(options.value(key)));
    if(definition.value("name").toString().trimmed().isEmpty() || definition.value("name").toString().toUtf8().size()>128
        || !finite(definition.value("priority"),-1000000,1000000) || definition.value("priority").toDouble()!=definition.value("priority").toInt())return fail("Invalid region name or priority.");
    if(options.value("inherit").toBool())definition.remove("climate");
    else if(options.contains("climate")) {
        const auto c=QJsonObject::fromVariantMap(options.value("climate").toMap());
        if(!validClimate(c,m_environment.value("presets").toObject()))return fail("Invalid climate: check presets, weights and duration.");
        definition.insert("climate",c);
    }
    if(definition==r->definition)return true;
    const auto before=snapshot(); storeDefinition(definition); commit(before); emit selectionChanged(); return true;
}
void WeatherRegionDocument::removeSelected() { if(!current())return; const auto before=snapshot();
    for(int i=0;i<m_regions.size();++i)if(m_regions[i].definition.value("id").toString()==m_selected){m_regions.removeAt(i);break;}
    m_selected=m_regions.isEmpty()?QString():m_regions.first().definition.value("id").toString(); commit(before); emit selectionChanged();
}
void WeatherRegionDocument::beginStroke() {
    if(m_stroke)return;m_before=snapshot();m_stroke=true;m_strokeChanged=false;
    m_zoneStroke=current() && current()->definition.contains("zoneId");if(m_zoneStroke)m_map->beginUndoGroup();
}
void WeatherRegionDocument::endStroke() {
    if(!m_stroke)return;m_stroke=false;
    if(m_zoneStroke)m_map->endUndoGroup();else if(m_strokeChanged)commit(m_before);
    m_before={};m_zoneStroke=false;emit changed();
}
void WeatherRegionDocument::cancelStroke() {
    if(!m_stroke)return;m_stroke=false;
    if(m_zoneStroke)m_map->cancelUndoGroup();else{const auto before=m_before;restore(before);}
    m_before={};m_zoneStroke=false;emit changed();
}
void WeatherRegionDocument::applyShape(const QRegion &shape,int floor,bool erase) {
    auto *r=current();if(!r || floor<0 || floor>15)return;
    const bool standalone=!m_stroke;if(standalone)beginStroke();r=current();
    const QRegion clipped=shape.intersected(QRect(0,0,65536,65536));
    if(m_zoneStroke){const int id=r->definition.value("zoneId").toInt();
        for(const auto &rect:clipped)for(int y=rect.top();y<=rect.bottom();++y)for(int x=rect.left();x<=rect.right();++x)
            m_strokeChanged=m_map->editNumberedZone(x,y,floor,id,erase)||m_strokeChanged;
    }else{const QRegion old=r->floors.value(floor),next=erase?old.subtracted(clipped):old.united(clipped);if(old!=next){r->floors[floor]=next;m_strokeChanged=true;}}
    emit changed();if(standalone)endStroke();
}
void WeatherRegionDocument::paint(int x,int y,int floor,int radius,bool circle,bool erase) {
    radius=std::clamp(radius,0,32);
    applyShape(QRegion(QRect(x-radius,y-radius,radius*2+1,radius*2+1),circle?QRegion::Ellipse:QRegion::Rectangle),floor,erase);
}
void WeatherRegionDocument::paintRectangle(int x1,int y1,int x2,int y2,int floor,bool erase) {
    if(std::abs(qint64(x2)-x1)>4096 || std::abs(qint64(y2)-y1)>4096){fail("A rectangle may span at most 4096 tiles per axis.");return;}
    applyShape(QRegion(QRect(QPoint(x1,y1),QPoint(x2,y2)).normalized()),floor,erase);
}
void WeatherRegionDocument::paintLine(int x1,int y1,int x2,int y2,int floor,int radius,bool circle,bool erase) {
    const int steps=std::max(std::abs(x2-x1),std::abs(y2-y1));if(steps>4096)return;
    for(int i=0;i<=steps;++i){const double t=steps?double(i)/steps:0;paint(qRound(x1+(x2-x1)*t),qRound(y1+(y2-y1)*t),floor,radius,circle,erase);}
}
void WeatherRegionDocument::undo(){endStroke();if(m_undo.isEmpty())return;m_redo.append(snapshot());const auto s=m_undo.takeLast();restore(s);}
void WeatherRegionDocument::redo(){endStroke();if(m_redo.isEmpty())return;m_undo.append(snapshot());const auto s=m_redo.takeLast();restore(s);}
QVariantList WeatherRegionDocument::visibleRects(int left,int top,int right,int bottom,int floor) const {
    QVariantList out; const QRect view=QRect(QPoint(left,top),QPoint(right,bottom)).normalized();
    const QStringList colors={"#4aa3df","#d9a441","#9d71d4","#50b890","#db728f","#86b957"};
    for(int i=0;i<m_regions.size();++i){const auto &r=m_regions[i];const auto clipped=geometry(r,floor).intersected(view);
        for(const auto &rect:clipped)out.append(QVariantMap{{"x",rect.x()},{"y",rect.y()},{"w",rect.width()},{"h",rect.height()},
            {"color",colors[i%colors.size()]},{"selected",r.definition.value("id").toString()==m_selected},{"label",r.definition.value("name").toString()}});
    } return out;
}
QByteArray WeatherRegionDocument::serialize(bool editor) const {
    auto root=m_environment; QJsonArray regions,empty,unassigned;
    for(const auto &r:m_regions){auto definition=r.definition;QJsonArray areas,floors;
        if(definition.value("editorOnly").toBool()){unassigned.append(definition);continue;}
        if(definition.contains("zoneId")){if(m_map->zoneAreas(definition.value("zoneId").toInt()).isEmpty())empty.append(definition);else regions.append(definition);continue;}
        for(auto it=r.floors.cbegin();it!=r.floors.cend();++it){if(it.value().isEmpty())continue;floors.append(it.key());
            for(const auto &rect:it.value()) {const int x=rect.x(),y=rect.y(),right=x+rect.width(),bottom=y+rect.height();
                areas.append(QJsonObject{{"floor",it.key()},{"polygon",QJsonArray{QJsonArray{x,y},QJsonArray{right,y},QJsonArray{right,bottom},QJsonArray{x,bottom}}}});
            }
        }
        definition.insert("floors",floors);definition.insert("areas",areas);
        if(areas.isEmpty())empty.append(definition);else regions.append(definition);
    }
    root.insert("regions",regions);root.remove("editor");
    if(editor)root.insert("editor",QJsonObject{{"emptyRegions",empty},{"unassignedRegions",unassigned},{"selectedId",m_selected}});
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
bool WeatherRegionDocument::write(const QString &name,bool editor){
    if(name.isEmpty())return fail("Save the map first, or choose an export path.");
    if(!editor) {
        for(const auto &r:m_regions)if(r.definition.contains("zoneId") && !m_map->numberedZonesEnabled())return fail("Enable numbered zones before export.");
        if(m_map->isDirty())return fail("Save the map first, or export a server package.");
        qint64 references=0;
        for(const auto &region:m_regions) {
            int areas=0;
            for(const auto &shape:region.floors)for(const auto &rect:shape) {
                ++areas;
                references+=qint64((rect.x()+rect.width())/32-rect.x()/32+1)
                    *((rect.y()+rect.height())/32-rect.y()/32+1);
            }
            if(areas>16384)return fail("Region exceeds 16384 areas. Simplify its painted boundary before export.");
        }
        if(references>262144)return fail("Weather exceeds 262144 shape/chunk references. Simplify the regions before export.");
    }
    const auto bytes=serialize(editor);
    if(bytes.size()>4*1024*1024)return fail("Weather JSON exceeds the server's 4 MiB limit. Simplify the regions.");
    QSaveFile file(localPath(name));if(!file.open(QIODevice::WriteOnly))return fail(file.errorString());
    if(file.write(bytes)!=bytes.size() || !file.commit())return fail(file.errorString());
    m_error.clear();emit changed();return true;
}
bool WeatherRegionDocument::save(){if(!write(path(),true))return false;m_dirty=false;emit changed();emit m_map->dirtyChanged();return true;}
bool WeatherRegionDocument::exportFile(const QString &file){return write(file,false);}
bool WeatherRegionDocument::importFile(const QString &name){
    QFile file(localPath(name));if(!file.open(QIODevice::ReadOnly))return fail(file.errorString());
    if(file.size()>4*1024*1024)return fail("Weather JSON exceeds 4 MiB.");
    const auto before=snapshot();if(!load(file.readAll()))return false;commit(before);emit selectionChanged();return true;
}
bool WeatherRegionDocument::load(const QByteArray &bytes){
    QJsonParseError error;const auto parsed=QJsonDocument::fromJson(bytes,&error);const auto root=parsed.object();
    const auto presetMap=root.value("presets").toObject();
    if(error.error!=QJsonParseError::NoError || root.value("version").toInt()!=1 || presetMap.isEmpty() || presetMap.size()>128
        || !validClimate(root.value("global").toObject(),presetMap) || !root.value("regions").isArray())return fail("Invalid weather environment v1.");
    for(auto it=presetMap.begin();it!=presetMap.end();++it)if(!validPreset(it.key(),it.value().toObject()))return fail("Invalid preset, intensity, color or rendering parameter.");
    QList<Region> regions;QSet<QString> ids;auto all=root.value("regions").toArray();
    for(const auto &v:root.value("editor").toObject().value("emptyRegions").toArray())all.append(v);
    for(const auto &v:root.value("editor").toObject().value("unassignedRegions").toArray())all.append(v);
    if(all.size()>1024)return fail("Maximum 1024 regions.");
    for(const auto &v:all){auto definition=v.toObject();const auto id=definition.value("id").toString();
        if(!idValid(id) || ids.contains(id) || definition.value("name").toString().trimmed().isEmpty()
            || definition.value("name").toString().toUtf8().size()>128 || !finite(definition.value("priority"),-1000000,1000000)
            || definition.value("priority").toDouble()!=definition.value("priority").toInt())return fail("Invalid or duplicate region.");
        for(const auto &key:{"indoorFog","indoorFireflies"})if(definition.contains(key) && !definition.value(key).isBool())return fail("Invalid indoor policy.");
        if(definition.contains("polygon") && definition.contains("areas"))return fail("Region cannot contain both polygon and areas.");
        ids.insert(id);if(definition.contains("climate") && !validClimate(definition.value("climate").toObject(),presetMap))return fail("Invalid regional climate.");
        Region r{definition,{}};
        if(definition.contains("zoneId")){
            const auto zone=definition.value("zoneId");if(!finite(zone,1,65535) || zone.toDouble()!=zone.toInt() || definition.contains("polygon") || definition.contains("areas") || definition.contains("floors"))return fail("Invalid or ambiguous ZoneID binding.");
            for(const auto &existing:regions)if(existing.definition.value("zoneId")==zone)return fail("Duplicate weather ZoneID.");

        }else if(definition.contains("polygon")){
            QRegion shape;if(!polygon(definition.value("polygon").toArray(),shape))return fail("Polygons must be simple and use valid tile coordinates.");
            for(const auto &z:definition.value("floors").toArray()){if(!finite(z,0,15) || z.toDouble()!=z.toInt())return fail("Invalid floor.");r.floors[z.toInt()]|=shape;}
        }else{
            if(!definition.value("areas").isArray())return fail("Region requires polygon or areas.");
            for(const auto &value:definition.value("areas").toArray()){const auto a=value.toObject();const auto z=a.value("floor");QRegion shape;
                if(!finite(z,0,15) || z.toDouble()!=z.toInt() || !polygon(a.value("polygon").toArray(),shape))return fail("Invalid weather area.");
                r.floors[z.toInt()]|=shape;
            }
        }
        r.definition.remove("polygon");r.definition.remove("areas");r.definition.remove("floors");regions.append(r);
    }
    m_environment=root;m_environment.remove("editor");m_regions=regions;
    m_selected=root.value("editor").toObject().value("selectedId").toString();
    if(!current())m_selected=m_regions.isEmpty()?QString():m_regions.first().definition.value("id").toString();
    QSet<int> reserved;for(const auto &r:m_regions)if(r.definition.contains("zoneId"))reserved.insert(r.definition.value("zoneId").toInt());m_map->setReservedZoneIds(reserved);
    m_error.clear();return true;
}

void WeatherRegionDocument::setTestTimeout(int seconds)
{
    auto *timer=findChild<QTimer*>(QStringLiteral("weatherLocalTimeout"),Qt::FindDirectChildrenOnly);
    if(!timer){timer=new QTimer(this);timer->setObjectName(QStringLiteral("weatherLocalTimeout"));timer->setSingleShot(true);timer->setTimerType(Qt::PreciseTimer);connect(timer,&QTimer::timeout,this,&WeatherRegionDocument::testTimedOut);}
    timer->stop();if(seconds>0 && seconds<=3600)timer->start(seconds*1000);
}
WeatherRegionDocument::~WeatherRegionDocument() { if(m_operation)m_operation->cancel(); }
void WeatherRegionDocument::cancelOperation() { if(m_operation)m_operation->cancel(); }
bool WeatherRegionDocument::exportPackage(const QString &parent,bool migrateLegacy)
{
    if(busy())return fail("An export is already running.");
    const QString directory=localPath(parent);
    if(!QFileInfo(directory).isDir())return fail("Choose an existing parent directory.");
    for(const auto &r:m_regions)if(r.definition.contains("zoneId") && !m_map->numberedZonesEnabled())return fail("Enable numbered zones before exporting.");
    const auto snapshot=m_map->createRecoverySnapshot();
    const auto environment=m_environment;const auto regions=m_regions;const auto selected=m_selected;
    m_error.clear();m_progress=0;
    auto *watcher=new QFutureWatcher<QString>(this);m_operation=watcher;
    connect(watcher,&QFutureWatcher<QString>::progressValueChanged,this,[this](int value){m_progress=value;emit operationChanged();});
    connect(watcher,&QFutureWatcher<QString>::finished,this,[this,watcher]{
        if(watcher->isCanceled())m_error="Export cancelled.";
        else if(watcher->future().resultCount()) {const auto result=watcher->result();if(result.startsWith("ERROR:"))m_error=result.mid(6);else m_exportedPackage=result;}
        m_operation=nullptr;watcher->deleteLater();emit operationChanged();emit changed();
    });
    watcher->setFuture(QtConcurrent::run([snapshot,environment,regions,selected,directory,migrateLegacy](QPromise<QString> &promise){
        promise.setProgressRange(0,100);
        auto failure=[&](const QString &text){promise.addResult("ERROR:"+text);};
        QTemporaryDir stage(QDir(directory).filePath(".zones-export-XXXXXX"));
        if(!stage.isValid()){failure("Cannot create export staging directory.");return;}
        const QString mapPath=stage.filePath("map.otbm");
        const auto error=OtbmReader::writeRecoverySnapshot(snapshot,mapPath);
        if(!error.isEmpty()){failure(error);return;}promise.setProgressValue(10);
        if(promise.isCanceled())return;
        OtbmReader map;
        if(!map.loadFileDetached(mapPath,{},[&]{return promise.isCanceled();})){if(!promise.isCanceled())failure(map.errorString());return;}
        map.setUndoLimit(0);
        auto converted=regions;
        for(const auto &r:converted)if(r.definition.contains("zoneId"))map.reserveZoneId(r.definition.value("zoneId").toInt());
        qint64 total=0,done=0;
        if(migrateLegacy)for(const auto &r:converted)if(!r.definition.contains("zoneId"))for(const auto &shape:r.floors)for(const auto &rect:shape)total+=qint64(rect.width())*rect.height();
        if(migrateLegacy){map.setNumberedZonesEnabled(true);
            for(auto &r:converted){if(r.definition.contains("zoneId"))continue;
                const int id=map.nextZoneId();if(!id){failure("No free ZoneID for migration.");return;}map.reserveZoneId(id);
                for(auto f=r.floors.cbegin();f!=r.floors.cend();++f)for(const auto &rect:f.value())for(int y=rect.top();y<=rect.bottom();++y)for(int x=rect.left();x<=rect.right();++x){
                    if((done++%1024)==0){if(promise.isCanceled())return;promise.setProgressValue(10+int(60*done/std::max<qint64>(1,total)));}
                    const auto *tile=map.tileAt(x,y,f.key());auto ids=tile?tile->zone_ids:TileZoneIds{};ids.add(uint16_t(id));map.setTileZoneIds(x,y,f.key(),ids);
                }
                r.definition.insert("zoneId",id);r.floors.clear();
            }
        }
        if(promise.isCanceled())return;
        if(!map.saveFile(mapPath)){failure(map.errorString());return;}
        WeatherRegionDocument document(&map);document.m_environment=environment;document.m_regions=converted;document.m_selected=selected;
        if(!document.save() || !document.exportFile(stage.filePath("environment.json"))){failure(document.error());return;}
        promise.setProgressValue(90);
        OtbmReader verify;
        if(!verify.loadFileDetached(mapPath,{},[&]{return promise.isCanceled();})){if(!promise.isCanceled())failure(verify.errorString());return;}
        if(map.zoneIds()!=verify.zoneIds()){failure("Zone IDs were lost while saving the map.");return;}
        for(const auto &id:map.zoneIds())for(int floor=0;floor<16;++floor)
            if(map.zoneGeometry(id.toInt(),floor)!=verify.zoneGeometry(id.toInt(),floor)){failure("Zone geometry differs after saving.");return;}
        if(promise.isCanceled())return;
        QString name="zones-package-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
        int suffix=1;while(QFileInfo::exists(QDir(directory).filePath(name)))name+="-"+QString::number(suffix++);
        const QString target=QDir(directory).filePath(name);
        if(!QDir().rename(stage.path(),target)){failure("Cannot publish the completed export directory.");return;}
        stage.setAutoRemove(false);promise.setProgressValue(100);promise.addResult(target);
    }));
    emit operationChanged();return true;
}
void WeatherRegionDocument::importZoneWeather(const QString &source,const QVariantMap &mapping)
{
    if(mapping.isEmpty())return;
    QString file=source+".weather.json";
    if(!QFileInfo::exists(file))file=QFileInfo(source).dir().filePath("environment.json");
    if(!QFileInfo::exists(file))return;
    QFile input(file);if(!input.open(QIODevice::ReadOnly) || input.size()>4*1024*1024){fail("Map imported, but its weather configuration could not be read.");return;}
    QJsonParseError error;const auto root=QJsonDocument::fromJson(input.readAll(),&error).object();
    if(error.error!=QJsonParseError::NoError || root.value("version").toInt()!=1){fail("Map imported, but its weather JSON is invalid.");return;}
    auto presets=m_environment.value("presets").toObject();QMap<QString,QString> names;
    const auto incoming=root.value("presets").toObject();
    for(auto it=incoming.constBegin();it!=incoming.constEnd();++it){if(!validPreset(it.key(),it.value().toObject())){fail("Map imported, invalid weather preset skipped.");return;}
        QString id=it.key();if(presets.contains(id) && presets.value(id)!=it.value())id="import_"+QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(it.value().toObject()).toJson(QJsonDocument::Compact),QCryptographicHash::Sha1).toHex().left(24));
        names[it.key()]=id;presets.insert(id,it.value());}
    QList<Region> additions;QSet<QString> used;for(const auto &r:m_regions)used.insert(r.definition.value("id").toString());
    for(const auto &entry:root.value("regions").toArray()){auto d=entry.toObject();if(!d.contains("zoneId"))continue;
        const auto zone=mapping.value(QString::number(d.value("zoneId").toInt()));if(!zone.isValid())continue;
        QString id=d.value("id").toString();if(!idValid(id) || d.value("name").toString().isEmpty() || !finite(d.value("priority"),-1000000,1000000)){fail("Map imported, invalid weather region skipped.");return;}
        if(used.contains(id))id="import_"+QString::fromLatin1(QCryptographicHash::hash((source+id+zone.toString()).toUtf8(),QCryptographicHash::Sha1).toHex().left(24));
        used.insert(id);d["id"]=id;d["zoneId"]=zone.toInt();
        if(d.contains("climate")){auto c=d.value("climate").toObject();if(!validClimate(c,incoming)){fail("Map imported, invalid weather climate skipped.");return;}
            c["defaultPreset"]=names.value(c.value("defaultPreset").toString());auto weights=c.value("presets").toArray();for(int i=0;i<weights.size();++i){auto w=weights[i].toObject();w["preset"]=names.value(w.value("preset").toString());weights[i]=w;}c["presets"]=weights;d["climate"]=c;}
        additions.append({d,{}});
    }
    if(presets.size()>128 || m_regions.size()+additions.size()>1024){fail("Map imported, weather configuration exceeds limits.");return;}
    if(additions.isEmpty())return;
    const auto before=snapshot();m_environment["presets"]=presets;m_regions.append(additions);commit(before);emit selectionChanged();
}
#include "activityservice.h"
#include "documentmanager.h"
#include "otbmreader.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace { QString id(){return QUuid::createUuid().toString(QUuid::WithoutBraces);} }
ActivityService::ActivityService(DocumentManager *documents,QObject *parent):QObject(parent),m_documents(documents) {
    m_clock.start();m_anchor=QDateTime::currentMSecsSinceEpoch();
    QSettings settings;m_server=settings.value("activity/server").toString();m_token=settings.value("activity/token").toString();m_device=settings.value("activity/device").toString();m_accountName=settings.value("activity/accountName").toString();
    m_connection="activity-"+id();m_storage=new QObject;m_storage->moveToThread(&m_storageThread);
    connect(&m_storageThread,&QThread::finished,m_storage,&QObject::deleteLater);m_storageThread.start();
    const QString folder=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QPointer<ActivityService> guard(this);const QString connection=m_connection;
    QMetaObject::invokeMethod(m_storage,[guard,folder,connection]{
        QDir().mkpath(folder);auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(folder+"/activity.sqlite");
        QString error;
        if(!db.open()) error=db.lastError().text();
        else {QSqlQuery q(db);q.exec("PRAGMA journal_mode=WAL");q.exec("PRAGMA synchronous=FULL");
            if(!q.exec("CREATE TABLE IF NOT EXISTS queue(id TEXT PRIMARY KEY, server TEXT NOT NULL, token TEXT NOT NULL, payload TEXT NOT NULL, blocked INTEGER NOT NULL DEFAULT 0)")) error=q.lastError().text();
            q.exec("PRAGMA table_info(queue)");bool hasBlocked=false;while(q.next())if(q.value(1).toString()=="blocked")hasBlocked=true;
            if(!hasBlocked&&!q.exec("ALTER TABLE queue ADD COLUMN blocked INTEGER NOT NULL DEFAULT 0"))error=q.lastError().text();}
        QSqlQuery countQuery(db);countQuery.exec("SELECT COUNT(*),COALESCE(SUM(blocked),0) FROM queue");int count=0,blocked=0;if(countQuery.next()){count=countQuery.value(0).toInt();blocked=countQuery.value(1).toInt();}
        if(guard) QMetaObject::invokeMethod(guard,[guard,error,count,blocked]{if(!guard)return;guard->m_pending=count;guard->m_blocked=blocked;if(!error.isEmpty())guard->fail("Activity queue: "+error);emit guard->changed();},Qt::QueuedConnection);
    },Qt::QueuedConnection);
    QCoreApplication::instance()->installEventFilter(this);
    connect(documents,&DocumentManager::currentChanged,this,&ActivityService::switchDocument);
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,this,[this]{endSession();if(linked()){auto e=base("heartbeat");e["status"]="offline";enqueue(e);}});
    m_tick.setInterval(1000);connect(&m_tick,&QTimer::timeout,this,[this]{
        const qint64 now=m_clock.elapsed();const QString state=!linked()?"Not connected":m_session.isEmpty()?"Editor open":now-m_lastInput>=300000?"AFK":"Recording";
        if(state!=m_status){m_status=state;emit changed();}
        if(linked() && now-m_checkpoint>=30000){checkpoint();auto e=base("heartbeat");e["status"]=m_session.isEmpty()?"editor_open":now-m_lastInput>=300000?"afk":"active";enqueue(e);refreshMaps();}
    });m_tick.start();
    m_sync.setInterval(15000);connect(&m_sync,&QTimer::timeout,this,&ActivityService::upload);m_sync.start();
    switchDocument();if(linked()) QTimer::singleShot(0,this,&ActivityService::refreshMaps);
}
ActivityService::~ActivityService(){
    QCoreApplication::instance()->removeEventFilter(this);endSession();
    const QString connection=m_connection;
    // Drain queued writes before closing SQLite; no GUI input handler waits on disk.
    QMetaObject::invokeMethod(m_storage,[connection]{ {auto db=QSqlDatabase::database(connection);db.close();} QSqlDatabase::removeDatabase(connection);},Qt::BlockingQueuedConnection);
    m_storageThread.quit();m_storageThread.wait();
}
void ActivityService::fail(const QString &error){m_error=error;emit changed();}
QString ActivityService::utc(qint64 elapsed)const{return QDateTime::fromMSecsSinceEpoch(m_anchor+elapsed,QTimeZone::UTC).toString(Qt::ISODateWithMs);}
QJsonObject ActivityService::base(const QString &kind){
    return {{"id",id()},{"sessionId",m_session.isEmpty()?QJsonValue():QJsonValue(m_session)},
        {"mapId",m_mapId.isEmpty()?QJsonValue():QJsonValue(m_mapId)},{"seq",++m_seq},{"at",utc(m_clock.elapsed())},{"kind",kind}};
}
void ActivityService::enqueue(QJsonObject event){
    if(!linked())return;
    const QString connection=m_connection,server=m_server,secret=m_token;
    const QByteArray payload=QJsonDocument(event).toJson(QJsonDocument::Compact);const QPointer<ActivityService> guard(this);
    QMetaObject::invokeMethod(m_storage,[connection,server,secret,payload,event,guard]{
        auto db=QSqlDatabase::database(connection);QSqlQuery q(db);q.prepare("INSERT INTO queue(id,server,token,payload) VALUES(?,?,?,?)");q.addBindValue(event["id"].toString());q.addBindValue(server);q.addBindValue(secret);q.addBindValue(QString::fromUtf8(payload));
        QString error;if(!q.exec())error=q.lastError().text();q.exec("SELECT COUNT(*) FROM queue");int count=q.next()?q.value(0).toInt():0;
        if(guard)QMetaObject::invokeMethod(guard,[guard,count,error]{if(!guard)return;guard->m_pending=count;if(!error.isEmpty())guard->m_error="Activity queue: "+error;emit guard->changed();},Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void ActivityService::checkpoint(){
    const qint64 now=m_clock.elapsed();
    if(!m_session.isEmpty()){
        const qint64 activeEnd=qBound(m_checkpoint,m_lastInput+300000,now);
        auto segment=[this](qint64 start,qint64 end,bool active){while(end>start){const qint64 next=qMin(end,start+3600000);auto e=base("duration");e["start"]=utc(start);e["at"]=utc(next);e[active?"activeMs":"idleMs"]=next-start;enqueue(e);start=next;}};
        segment(m_checkpoint,activeEnd,true);segment(activeEnd,now,false);
    }
    m_checkpoint=now;
}
bool ActivityService::eventFilter(QObject *,QEvent *event){
    switch(event->type()) {case QEvent::KeyPress:case QEvent::MouseButtonPress:case QEvent::MouseMove:case QEvent::Wheel:case QEvent::TouchBegin:case QEvent::TouchUpdate:
        if(!m_session.isEmpty() && m_clock.elapsed()-m_lastInput>=300000)checkpoint();
        m_lastInput=m_clock.elapsed();break;default:break;}
    return false;
}
QString ActivityService::detectedMap(OtbmReader *document,bool *ambiguous)const{
    if(ambiguous)*ambiguous=false;
    if(!linked()||!document||!document->isLoaded()||document->filePath().isEmpty())return {};
    const QString filename=QFileInfo(document->filePath()).fileName();QString match;
    for(const auto &entry:m_maps){const auto rule=entry.toMap();
        if(!rule.value("tracked").toBool()||filename.compare(rule.value("filename").toString(),Qt::CaseInsensitive)!=0)continue;
        if(!match.isEmpty()){if(ambiguous)*ambiguous=true;return {};}
        match=rule.value("id").toString();
    }
    return match;
}
QString ActivityService::currentMapName()const{
    for(const auto &entry:m_maps){const auto rule=entry.toMap();if(rule.value("id").toString()==m_mapId)return rule.value("project").toString()+" / "+rule.value("name").toString();}
    return {};
}
void ActivityService::record(const QString &kind,qint64 tiles){
    if(m_session.isEmpty())return;auto e=base(kind);if(kind=="edit")e["tiles"]=tiles;enqueue(e);
}
void ActivityService::endSession(){if(m_session.isEmpty())return;checkpoint();record("end");m_session.clear();m_mapId.clear();emit changed();}
void ActivityService::switchDocument(){
    endSession();m_current=m_documents->current();
    if(m_current){
        auto *doc=m_current.data();
        if(!m_observed.contains(doc)){
            m_observed.insert(doc);
            connect(doc,&QObject::destroyed,this,[this,doc]{m_observed.remove(doc);m_lastSessions.remove(doc);});
            connect(doc,&OtbmReader::activityOperation,this,[this,doc](const QString &kind,qint64 tiles){if(doc==m_current)record(kind,tiles);});
            connect(doc,&OtbmReader::activitySave,this,[this,doc](bool automatic,bool success){
                const QString kind=success?(automatic?"autosave":"save"):(automatic?"autosave_error":"save_error");
                if(doc==m_current)record(kind);
                else if(m_lastSessions.contains(doc)&&detectedMap(doc)==m_lastSessions.value(doc).second){auto e=base(kind);const auto previous=m_lastSessions.value(doc);e["sessionId"]=previous.first;e["mapId"]=previous.second;enqueue(e);}
            });
            connect(doc,&OtbmReader::filePathChanged,this,[this,doc]{if(doc==m_current&&detectedMap(doc)!=m_mapId)switchDocument();});
            connect(doc,&OtbmReader::loadedChanged,this,[this,doc]{if(doc==m_current&&detectedMap(doc)!=m_mapId)switchDocument();});
        }
        bool ambiguous=false;m_mapId=detectedMap(doc,&ambiguous);
        if(ambiguous)fail("Multiple tracked maps match this filename. Resolve the conflict in the admin panel.");
        if(!m_mapId.isEmpty()) {m_session=id();m_lastSessions[doc]={m_session,m_mapId};m_checkpoint=m_lastInput=m_clock.elapsed();record("start");}
    }
    emit changed();
}
void ActivityService::disconnectDevice(){endSession();if(linked())enqueue(base("logout"));m_lastSessions.clear();QSettings s;s.remove("activity/token");s.remove("activity/device");s.remove("activity/accountName");m_token.clear();m_device.clear();m_accountName.clear();m_maps.clear();m_summary.clear();m_summaryError.clear();m_summaryLoading=false;m_status="Not connected";emit summaryChanged();emit changed();upload();}
void ActivityService::refreshSummary(){
    if(!linked()||m_summaryLoading)return;
    const QString secret=m_token;
    QNetworkRequest request(QUrl(m_server+"/api/device/summary"));
    request.setRawHeader("Authorization",("Bearer "+secret).toUtf8());request.setTransferTimeout(15000);
    m_summaryLoading=true;emit summaryChanged();
    auto *reply=m_network.get(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply,secret]{
        const auto result=QJsonDocument::fromJson(reply->readAll());
        const auto error=reply->error();const QString message=reply->errorString();
        reply->deleteLater();
        if(secret!=m_token)return;
        m_summaryLoading=false;
        if(error==QNetworkReply::NoError&&result.isObject()&&result.object().value("totals").isObject()){
            m_summary=result.object().toVariantMap();m_summaryError.clear();
        }else{
            m_summaryError="Cannot refresh your activity report: "+message;
            if(error==QNetworkReply::AuthenticationRequiredError)disconnectDevice();
        }
        emit summaryChanged();
    });
}
void ActivityService::signIn(const QString &url,const QString &email,const QString &password,const QString &deviceName){
    if(m_busy||linked()){fail("Sign out before logging in to another account.");return;}
    if(email.trimmed().isEmpty()||password.isEmpty()){fail("Enter your account email and password.");return;}
    QUrl parsed(url.trimmed());
    const bool local=parsed.host()=="localhost"||parsed.host()=="127.0.0.1"||parsed.host()=="::1";
    if((parsed.scheme()!="https"&&!(local&&parsed.scheme()=="http"))||parsed.host().isEmpty()||!parsed.userInfo().isEmpty()||parsed.hasQuery()||parsed.hasFragment()||(parsed.path()!=""&&parsed.path()!="/")){fail("Enter the HTTPS panel origin (without a path).");return;}
    const QString server=parsed.toString(QUrl::RemovePath|QUrl::RemoveQuery|QUrl::RemoveFragment);
    QNetworkRequest request(QUrl(server+"/api/device/login"));request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setTransferTimeout(15000);
    m_busy=true;m_error.clear();emit changed();auto *reply=m_network.post(request,QJsonDocument(QJsonObject{{"email",email.trimmed()},{"password",password},{"name",deviceName.trimmed()}}).toJson());
    connect(reply,&QNetworkReply::finished,this,[this,reply,server]{m_busy=false;const auto result=QJsonDocument::fromJson(reply->readAll()).object();
        if(reply->error()!=QNetworkReply::NoError||result["token"].toString().isEmpty())fail(result["error"].toString(reply->errorString()));
        else {m_server=server;m_token=result["token"].toString();m_device=result["deviceId"].toString();m_accountName=result["accountName"].toString();QSettings s;s.setValue("activity/server",m_server);s.setValue("activity/token",m_token);s.setValue("activity/device",m_device);s.setValue("activity/accountName",m_accountName);switchDocument();refreshMaps();}
        reply->deleteLater();emit changed();});
}
void ActivityService::refreshMaps(){
    if(!linked()||m_busy)return;QNetworkRequest request(QUrl(m_server+"/api/device/maps"));request.setRawHeader("Authorization",("Bearer "+m_token).toUtf8());request.setTransferTimeout(15000);m_busy=true;emit changed();
    auto *reply=m_network.get(request);connect(reply,&QNetworkReply::finished,this,[this,reply]{m_busy=false;
        if(reply->error()==QNetworkReply::NoError){
            m_maps=QJsonDocument::fromJson(reply->readAll()).array().toVariantList();m_error.clear();
            bool ambiguous=false;const QString detected=detectedMap(m_current,&ambiguous);
            if(detected!=m_mapId||(!detected.isEmpty()&&m_session.isEmpty()))switchDocument();
            if(ambiguous)fail("Multiple tracked maps match this filename. Resolve the conflict in the admin panel.");
            const QString connection=m_connection,secret=m_token;
            QMetaObject::invokeMethod(m_storage,[connection,secret]{QSqlQuery q(QSqlDatabase::database(connection));q.prepare("UPDATE queue SET blocked=0 WHERE token=?");q.addBindValue(secret);q.exec();},Qt::QueuedConnection);
            upload();
        }
        else fail("Cannot refresh project maps: "+reply->errorString());reply->deleteLater();emit changed();});
}
void ActivityService::upload(){
    if(m_uploading)return;m_uploading=true;const QPointer<ActivityService> guard(this);const QString connection=m_connection,activeServer=m_server,activeToken=m_token;
    QMetaObject::invokeMethod(m_storage,[guard,connection,activeServer,activeToken]{
        QSqlQuery q(QSqlDatabase::database(connection));q.prepare("SELECT server,token FROM queue WHERE blocked=0 ORDER BY CASE WHEN server=? AND token=? THEN 0 ELSE 1 END,rowid LIMIT 1");q.addBindValue(activeServer);q.addBindValue(activeToken);q.exec();QString server,secret;QJsonArray events;
        if(q.next()){server=q.value(0).toString();secret=q.value(1).toString();q.prepare("SELECT payload FROM queue WHERE server=? AND token=? AND blocked=0 ORDER BY rowid LIMIT 500");q.addBindValue(server);q.addBindValue(secret);if(q.exec())while(q.next())events.append(QJsonDocument::fromJson(q.value(0).toByteArray()).object());}
        if(guard)QMetaObject::invokeMethod(guard,[guard,server,secret,events,connection]{
            if(!guard)return;if(events.isEmpty()){guard->m_uploading=false;return;}
            QNetworkRequest request(QUrl(server+"/api/device/events"));request.setRawHeader("Authorization",("Bearer "+secret).toUtf8());request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setTransferTimeout(15000);
            auto *reply=guard->m_network.post(request,QJsonDocument(QJsonObject{{"events",events}}).toJson(QJsonDocument::Compact));
            connect(reply,&QNetworkReply::finished,guard,[guard,reply,connection,secret]{
                const auto result=QJsonDocument::fromJson(reply->readAll()).object();
                if(reply->error()!=QNetworkReply::NoError){
                    guard->m_uploading=false;guard->fail("Activity sync: "+result["error"].toString(reply->errorString()));
                    const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if(code==400||code==401||code==403||code==409)QMetaObject::invokeMethod(guard->m_storage,[guard,connection,secret,code]{
                        QSqlQuery q(QSqlDatabase::database(connection));q.prepare("UPDATE queue SET blocked=1 WHERE token=?");q.addBindValue(secret);q.exec();q.exec("SELECT COALESCE(SUM(blocked),0) FROM queue");const int blocked=q.next()?q.value(0).toInt():0;
                        if(guard)QMetaObject::invokeMethod(guard,[guard,blocked,secret,code]{if(guard){guard->m_blocked=blocked;emit guard->changed();if(code==401&&guard->m_token==secret)guard->disconnectDevice();}},Qt::QueuedConnection);
                    },Qt::QueuedConnection);
                }
                else {const QJsonArray accepted=result["accepted"].toArray();QMetaObject::invokeMethod(guard->m_storage,[guard,accepted,connection]{
                    auto db=QSqlDatabase::database(connection);db.transaction();QSqlQuery q(db);q.prepare("DELETE FROM queue WHERE id=?");bool ok=true;for(const auto &value:accepted){q.bindValue(0,value.toString());ok=q.exec()&&ok;}if(ok)ok=db.commit();if(!ok)db.rollback();
                    q.exec("SELECT COUNT(*),COALESCE(SUM(blocked),0) FROM queue");int count=0,blocked=0;if(q.next()){count=q.value(0).toInt();blocked=q.value(1).toInt();}
                    if(guard)QMetaObject::invokeMethod(guard,[guard,count,blocked,ok]{if(!guard)return;guard->m_pending=count;guard->m_blocked=blocked;guard->m_uploading=false;if(ok)guard->m_error.clear();else guard->m_error="Could not acknowledge activity queue";emit guard->changed();if(ok&&count>blocked)QTimer::singleShot(0,guard,&ActivityService::upload);},Qt::QueuedConnection);
                },Qt::QueuedConnection);}
                reply->deleteLater();
            });
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}

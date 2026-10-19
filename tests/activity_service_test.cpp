#include "activityservice.h"
#include "documentmanager.h"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QUuid>
#include <QTcpServer>
#include <QTcpSocket>

class ActivityServiceTest {
public:
 static int isolation() {
    for(int rejectedCode : {400,403,409}) {
        QTemporaryDir temp;
        QCoreApplication::setApplicationName("Isolation-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
        qputenv("DME_RECOVERY_DIR",temp.filePath("recovery").toUtf8());
        DocumentManager documents;documents.configureAutosave(false,1);
        ActivityService service(&documents);service.m_tick.stop();service.m_sync.stop();
        QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 30;
        const QString bad=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSet<QString> accepted;int requests=0;bool acknowledge=true;
        QObject::connect(&server,&QTcpServer::newConnection,&service,[&]{
            auto *socket=server.nextPendingConnection();
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                QByteArray buffer=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",buffer);
                const int split=buffer.indexOf("\r\n\r\n");if(split<0)return;
                int length=0;for(const auto &line:buffer.left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
                if(buffer.size()<split+4+length)return;
                const auto events=QJsonDocument::fromJson(buffer.mid(split+4,length)).object()["events"].toArray();
                ++requests;bool invalid=false;for(const auto &e:events)if(e.toObject()["id"].toString()==bad)invalid=true;
                QJsonArray ids;if(!invalid&&acknowledge)for(const auto &e:events){ids.append(e.toObject()["id"]);accepted.insert(e.toObject()["id"].toString());}
                const QByteArray body=invalid?QByteArray("{\"error\":\"one rejected event\"}"):QJsonDocument(QJsonObject{{"accepted",ids}}).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 "+QByteArray::number(invalid?rejectedCode:200)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
            });
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        service.m_server=QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());service.m_token="isolated-token";
        auto first=service.base("edit"),denied=service.base("edit"),last=service.base("edit");denied["id"]=bad;
        service.enqueue(first);service.enqueue(denied);service.enqueue(last);
        QMetaObject::invokeMethod(service.m_storage,[]{},Qt::BlockingQueuedConnection);
        service.upload();QElapsedTimer timeout;timeout.start();
        while(timeout.elapsed()<5000&&(accepted.size()!=2||service.m_uploading||service.m_blocked!=1)){QCoreApplication::processEvents();QThread::msleep(1);}
        if(accepted.size()!=2||service.pendingCount()!=1||service.m_blocked!=1||!service.linked()||accepted.contains(bad)||requests>12){qCritical()<<"Batch isolation failed"<<rejectedCode<<requests<<service.pendingCount()<<service.errorString();return 31;}
        if(rejectedCode==403) {
            acknowledge=false;auto noAck=service.base("edit");service.enqueue(noAck);
            QMetaObject::invokeMethod(service.m_storage,[]{},Qt::BlockingQueuedConnection);service.upload();
            timeout.restart();while(timeout.elapsed()<3000&&service.m_uploading){QCoreApplication::processEvents();QThread::msleep(1);}
            const int before=requests;timeout.restart();while(timeout.elapsed()<100){QCoreApplication::processEvents();QThread::msleep(1);}
            if(requests!=before||service.pendingCount()!=2||service.errorString().isEmpty())return 32;
        }
        service.m_token.clear();
    }
    return 0;
 }
 static int network(ActivityService &service, DocumentManager &documents,const QString &folder) {
    auto *map=documents.current();map->newMap(1024,1024,1098,3,57);map->saveFile(folder+"/pilot.otbm");
    service.signIn("http://localhost:3000",QString::fromUtf8(qgetenv("DME_ACTIVITY_EMAIL")),QString::fromUtf8(qgetenv("DME_ACTIVITY_PASSWORD")),"Automated DME pilot");
    QElapsedTimer wait;wait.start();
    while(wait.elapsed()<16000 && (service.busy()||service.maps().isEmpty())){QCoreApplication::processEvents();QThread::msleep(1);}
    if(!service.linked()||service.maps().isEmpty()){qCritical()<<service.errorString();return 10;}
    const QString filename=service.maps().first().toMap().value("filename").toString();
    if(filename.isEmpty())return 11;
    map->saveFile(folder+"/"+filename);
    if(service.m_session.isEmpty())return 11;
    map->beginUndoGroup();for(int i=0;i<10;i++)map->placeItem(100+i,100,7,100,0,false,true);map->endUndoGroup();
    map->undo();map->redo();map->saveFile(folder+"/"+filename);map->saveRecoveryFile(folder+"/autosave.otbm");
    QThread::msleep(30);service.endSession();
    auto heartbeat=service.base("heartbeat");heartbeat["status"]="offline";service.enqueue(heartbeat);
    QMetaObject::invokeMethod(service.m_storage,[]{},Qt::BlockingQueuedConnection);QCoreApplication::processEvents();service.upload();
    wait.restart();while(wait.elapsed()<16000 && (service.m_uploading||service.pendingCount()>0)){QCoreApplication::processEvents();QThread::msleep(1);}
    if(service.pendingCount()>0){qCritical()<<service.errorString();return 12;}
    documents.markCleanShutdown();return 0;
 }
 static int run() {
    QTemporaryDir temp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    qputenv("DME_RECOVERY_DIR",(temp.path()+"/recovery").toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("DME-Activity-Test");
    QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    DocumentManager documents;documents.configureAutosave(false,3);
    ActivityService service(&documents);service.m_tick.stop();service.m_sync.stop();
    if(!qgetenv("DME_ACTIVITY_EMAIL").isEmpty())return network(service,documents,temp.path());
    service.m_server="http://localhost:1";service.m_token="test-secret";
    auto *map=documents.current();map->newMap(1024,1024,1098,3,57);map->saveFile(temp.path()+"/map.otbm");
    const QString mapId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    service.m_maps={QVariantMap{{"id",mapId},{"name","Test"},{"filename","MAP.otbm"},{"tracked",true}},QVariantMap{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"name","Second"},{"filename","second.otbm"},{"tracked",true}}};
    service.switchDocument();if(service.m_session.isEmpty())return 1;
    const QString firstSession=service.m_session;
    map->beginUndoGroup();for(int i=0;i<10;i++)map->placeItem(100+i,100,7,100,0,false,true);map->endUndoGroup();
    map->undo();map->redo();map->saveFile(temp.path()+"/map.otbm");map->saveRecoveryFile(temp.path()+"/auto.otbm");map->saveFile(temp.path()+"/missing/map.otbm");
    // Exercise both sides of the 5-minute AFK boundary without a 5-minute sleep.
    const qint64 now=service.m_clock.elapsed();service.m_checkpoint=now-360000;service.m_lastInput=now-360000;service.checkpoint();
    auto *second=documents.newDocument();second->newMap(1024,1024,1098,3,57);second->saveFile(temp.path()+"/second.otbm");
    if(service.m_session==firstSession)return 2;
    map->saveRecoveryFile(temp.path()+"/background.otbm");
    service.endSession();
    QMetaObject::invokeMethod(service.m_storage,[]{},Qt::BlockingQueuedConnection);
    QCoreApplication::processEvents();
    auto db=QSqlDatabase::addDatabase("QSQLITE","test-reader");db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/activity.sqlite");if(!db.open())return 3;
    QSqlQuery q(db);q.exec("SELECT payload FROM queue ORDER BY rowid");
    QHash<QString,int> counts;qint64 active=0,idle=0,tiles=0;int starts=0,ends=0;
    while(q.next()){
        const auto e=QJsonDocument::fromJson(q.value(0).toByteArray()).object();const QString kind=e["kind"].toString();counts[kind]++;
        active+=e["activeMs"].toInteger();idle+=e["idleMs"].toInteger();if(kind=="edit")tiles+=e["tiles"].toInteger();
        starts+=kind=="start";ends+=kind=="end";
        if(e.contains("path")||e.contains("keys"))return 4;
    }
    if(counts["edit"]!=1||tiles!=10||counts["undo"]!=1||counts["redo"]!=1||counts["save"]!=2||counts["autosave"]!=2||counts["save_error"]!=1||starts!=2||ends!=2||active<300000||idle<60000){qCritical()<<counts<<active<<idle<<tiles<<starts<<ends;return 5;}
    const int pending=service.pendingCount();
    service.upload();QElapsedTimer timer;timer.start();while(timer.elapsed()<16000 && service.errorString().isEmpty()){QCoreApplication::processEvents();QThread::msleep(1);}
    if(service.pendingCount()!=pending||service.errorString().isEmpty())return 6;
    // A new service keeps the durable queue but never resumes a crashed session.
    service.m_token.clear();
    { ActivityService restarted(&documents); restarted.m_tick.stop(); restarted.m_sync.stop(); QMetaObject::invokeMethod(restarted.m_storage,[]{},Qt::BlockingQueuedConnection); QCoreApplication::processEvents(); if(restarted.pendingCount()!=pending || !restarted.m_session.isEmpty())return 7; }
    const QVariantList savedRules=service.m_maps;
    QTcpServer denied;denied.listen(QHostAddress::LocalHost,0);
    QObject::connect(&denied,&QTcpServer::newConnection,&service,[&denied]{auto *socket=denied.nextPendingConnection();QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket]{socket->readAll();socket->write("HTTP/1.1 401 Unauthorized\r\nContent-Type: application/json\r\nContent-Length: 19\r\nConnection: close\r\n\r\n{\"error\":\"revoked\"}");socket->disconnectFromHost();});QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);});
    service.m_server=QStringLiteral("http://127.0.0.1:%1").arg(denied.serverPort());service.m_token="unauthorized-token";
    auto heartbeat=service.base("heartbeat");heartbeat["status"]="editor_open";service.enqueue(heartbeat);
    QMetaObject::invokeMethod(service.m_storage,[]{},Qt::BlockingQueuedConnection);QCoreApplication::processEvents();service.upload();
    timer.restart();while(timer.elapsed()<5000 && (service.linked()||service.m_blocked==0)){QCoreApplication::processEvents();QThread::msleep(1);}
    if(service.linked()||service.m_blocked<1)return 8;
    // Central rules replace local manual assignments, including conflicts and
    // Save As to a different filename. Never guess which duplicate map to use.
    service.m_token="matching-test";service.m_maps=savedRules;
    if(service.detectedMap(map)!=mapId)return 20;
    auto rule=service.m_maps[0].toMap();rule["tracked"]=false;service.m_maps[0]=rule;
    if(!service.detectedMap(map).isEmpty())return 21;
    rule["tracked"]=true;service.m_maps[0]=rule;
    auto duplicate=rule;duplicate["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);service.m_maps.append(duplicate);
    bool ambiguous=false;if(!service.detectedMap(map,&ambiguous).isEmpty()||!ambiguous)return 22;
    service.m_maps.removeLast();documents.setCurrentIndex(0);
    if(service.m_session.isEmpty()||service.currentMapId()!=mapId)return 23;
    map->saveFile(temp.path()+"/untracked.otbm");
    if(!service.m_session.isEmpty()||!service.currentMapId().isEmpty())return 24;
    QTcpServer summaryServer;summaryServer.listen(QHostAddress::LocalHost,0);
    bool requestedSummary=false;
    QObject::connect(&summaryServer,&QTcpServer::newConnection,&service,[&]{
        auto *socket=summaryServer.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
            const QByteArray request=socket->readAll();
            requestedSummary=request.contains("GET /api/device/summary ")&&request.contains("Bearer summary-test");
            const QByteArray body=R"({"day":"2026-10-12","totals":{"activeMs":90000,"edits":3,"tiles":12}})";
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
            socket->disconnectFromHost();
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    service.m_server=QStringLiteral("http://127.0.0.1:%1").arg(summaryServer.serverPort());service.m_token="summary-test";
    service.refreshSummary();timer.restart();
    while(timer.elapsed()<3000&&service.summaryLoading()){QCoreApplication::processEvents();QThread::msleep(1);}
    if(!requestedSummary||service.summary().value("totals").toMap().value("edits").toInt()!=3)return 25;
    service.m_server=QStringLiteral("http://127.0.0.1:%1").arg(denied.serverPort());
    service.refreshSummary();timer.restart();
    while(timer.elapsed()<3000&&service.summaryLoading()){QCoreApplication::processEvents();QThread::msleep(1);}
    if(service.linked()||!service.summary().isEmpty())return 26;
    service.m_token.clear();
    q.finish();db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase("test-reader");
    return 0;
 }
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);int result=ActivityServiceTest::run();return result?result:ActivityServiceTest::isolation();}

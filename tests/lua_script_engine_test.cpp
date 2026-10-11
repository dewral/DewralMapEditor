#include "luascriptengine.h"
#include "customgeneratorprofiles.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    QTemporaryDir directory;
    auto flag=std::make_shared<std::atomic_bool>(false);
    auto run=[&](const QByteArray &code, int timeout=1000) {
        const QString path=directory.filePath("test.lua");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) qFatal("Cannot write fixture");
        file.write(code); file.close();
        return LuaScriptEngine::execute(path,"script",123,flag,timeout);
    };
    auto require=[](bool condition,const char *message) { if (!condition) qFatal("%s",message); };
    auto result=run("dme.setGround({x=10,y=20,z=7},100); dme.addItem({x=10,y=20,z=7},200,3)");
    require(result.value("success").toBool(),"Basic Lua patch failed");
    require(result.value("itemCount").toInt()==2 && result.value("tileCount").toInt()==1,"Lost generated items");
    const auto items=result.value("tiles").toList().first().toMap().value("items").toList();
    require(items.last().toMap().value("count").toInt()==3,"Lost stack count");
    result=run("dme.setGround({x=1,y=1,z=7},100); error('failure after staging')");
    require(!result.value("success").toBool() && !result.contains("tiles"),"Failed script exposed a partial patch");
    require(!run("dme.addItem({x=1,y=1,z=16},100)").value("success").toBool(),"Accepted invalid floor");
    require(!run("while true do end",20).value("success").toBool(),"Infinite loop was not stopped");
    flag->store(true);
    require(!run("dme.setGround({x=1,y=1,z=7},100)").value("success").toBool(),"Cancelled run succeeded");
    flag->store(false);
    const QByteArray random="dme.setGround({x=1,y=1,z=7},math.random(1,1000))";
    require(run(random).value("tiles")==run(random).value("tiles"),"Seed does not reproduce results");
    const QString asyncPath=directory.filePath("async.lua");
    QFile asyncFile(asyncPath); require(asyncFile.open(QIODevice::WriteOnly),"Cannot write async fixture");
    asyncFile.write("while true do end"); asyncFile.close();
    LuaScriptEngine engine;
    QEventLoop loop;
    QVariantMap asyncResult;
    QObject::connect(&engine,&LuaScriptEngine::finished,&loop,[&](const QVariantMap &value) {
        asyncResult=value; loop.quit();
    });
    engine.run(asyncPath,"script",123);
    require(engine.busy(),"Background run did not start");
    QTimer::singleShot(10,&engine,&LuaScriptEngine::cancel);
    QTimer::singleShot(5000,&loop,&QEventLoop::quit);
    loop.exec();
    require(!engine.busy() && !asyncResult.value("success").toBool()
        && asyncResult.value("error").toString().contains("cancel",Qt::CaseInsensitive),
        "Background cancellation failed or blocked the event loop");
    if (argc>1) {
        const auto generated=LuaScriptEngine::execute(QString::fromLocal8Bit(argv[1]),"otmapgen",123,flag);
        if (!generated.value("success").toBool()) qFatal("Generator failed: %s",qPrintable(generated.value("error").toString()));
        require(generated.value("itemCount").toInt()>100,"Generator integration produced no useful output");
        qInfo() << "otMapGen integration:" << generated.value("tileCount") << "tiles," << generated.value("itemCount") << "items";
        QDir generatorRoot(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath());
        generatorRoot.cdUp(); generatorRoot.cdUp();
        QVariantMap materials{{"background",QVariantList{QVariantMap{{"id",50001},{"weight",1}}}},
            {"floor",QVariantList{QVariantMap{{"id",50002},{"weight",100}},QVariantMap{{"id",50003},{"weight",1}}}},
            {"decorations",QVariantList{QVariantMap{{"id",50004},{"weight",1}},QVariantMap{{"id",50005},{"weight",1}}}}};
        QVariantMap info;
        for (int id=50001;id<=50005;++id) info[QString::number(id)]=QVariantMap{
            {"valid",true},{"ground",id<=50003},{"blocked",id==50005}};
        QVariantMap profile{{"version",1},{"name","Midhem test"},{"kind","dungeon"},
            {"width",40},{"height",40},{"rooms",4},{"seed",123},{"density",15},
            {"x",1000},{"y",1000},{"z",7},{"materials",materials},{"itemInfo",info}};
        for (const QString &kind : {QString("dungeon"),QString("cave")}) {
            profile["kind"]=kind;
            auto custom=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,profile);
            if (!custom.value("success").toBool()) qFatal("Custom %s failed: %s",qPrintable(kind),qPrintable(custom.value("error").toString()));
            require(custom.value("tileCount").toInt()==1600,"Custom dimensions were not respected");
            QHash<int,int> frequency;
            for (const QVariant &tile : custom.value("tiles").toList()) for (const QVariant &item : tile.toMap().value("items").toList()) {
                const int id=item.toMap().value("id").toInt();
                require(id>=50001 && id<=50005,"Custom generator emitted an unselected Tibia item");
                ++frequency[id];
            }
            QSet<int> passable;
            for (const QVariant &tileVariant : custom.value("tiles").toList()) {
                const auto tile=tileVariant.toMap(); bool floor=false,blocked=false;
                for (const QVariant &item : tile.value("items").toList()) {
                    const int id=item.toMap().value("id").toInt();
                    floor|=id==50002 || id==50003; blocked|=id==50005;
                }
                if (floor && !blocked) passable.insert(tile.value("y").toInt()*65536+tile.value("x").toInt());
            }
            require(!passable.isEmpty(),"No walkable region");
            QList<int> queue{*passable.begin()}; QSet<int> seen{queue.first()};
            for (int head=0;head<queue.size();++head) for (int offset : {1,-1,65536,-65536}) {
                const int next=queue[head]+offset;
                if (passable.contains(next) && !seen.contains(next)) { seen.insert(next); queue.append(next); }
            }
            require(seen.size()==passable.size(),"Decorations or room layout disconnected walking");
            require(frequency[50002]>frequency[50003]*10,"Material weights were ignored");
            auto again=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,profile);
            require(again.value("tiles")==custom.value("tiles"),"Custom seed is not deterministic");
            qInfo() << "Custom" << kind << "passed:" << custom.value("itemCount") << "items";
        }
        // Optional categories must never introduce upstream's stock IDs.
        auto full=profile;
        auto fullMaterials=materials; auto fullInfo=info;
        fullMaterials["secondary"]=QVariantList{QVariantMap{{"id",50006},{"weight",1}}};
        fullInfo["50006"]=QVariantMap{{"valid",true},{"ground",true},{"blocked",false}};
        for (int slot=1;slot<=16;++slot) {
            const QString role=slot<=4 ? "wall"+QString::number(slot) : "border"+QString::number(slot-4);
            const int itemId=50100+slot;
            fullMaterials[role]=QVariantList{QVariantMap{{"id",itemId},{"weight",1}}};
            fullInfo[QString::number(itemId)]=QVariantMap{{"valid",true},{"ground",false},{"blocked",false}};
        }
        full["materials"]=fullMaterials; full["itemInfo"]=fullInfo;
        full["wallsEnabled"]=true; full["bordersEnabled"]=true;
        for (const QString &kind : {QString("dungeon"),QString("cave")}) {
            full["kind"]=kind;
            const auto result=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,full);
            if (!result.value("success").toBool()) qFatal("Full %s recipe: %s",qPrintable(kind),qPrintable(result.value("error").toString()));
            QSet<int> used;
            for (const QVariant &tile : result.value("tiles").toList()) for (const QVariant &item : tile.toMap().value("items").toList()) {
                const int id=item.toMap().value("id").toInt();
                require(fullInfo.contains(QString::number(id)),"Optional slots introduced an unselected ID"); used.insert(id);
            }
            require(used.contains(50006),"Secondary floor was not used");
            require(used.contains(50101) && used.contains(50105),"Walls or borders were skipped");
            auto empty=profile; empty["kind"]=kind;
            auto pools=materials; pools.remove("decorations"); empty["materials"]=pools;
            const auto plain=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,empty);
            require(plain.value("success").toBool(),"Empty optional pools failed");
            for (const QVariant &tile : plain.value("tiles").toList()) for (const QVariant &item : tile.toMap().value("items").toList())
                require(item.toMap().value("id").toInt()<=50003,"Empty decorations used a default ID");
        }
        auto large=full; large["kind"]="dungeon"; large["width"]=70; large["height"]=70; large["rooms"]=8;
        auto blockingInfo=fullInfo;
        for (int slot=1;slot<=4;++slot) { auto item=blockingInfo[QString::number(50100+slot)].toMap(); item["blocked"]=true; blockingInfo[QString::number(50100+slot)]=item; }
        large["itemInfo"]=blockingInfo;
        const auto typical=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,large);
        if (!typical.value("success").toBool()) qFatal("Default-sized layout failed: %s",qPrintable(typical.value("error").toString()));
        require(typical.value("tileCount").toInt()==4900,"Default dimensions incorrect");
        auto invalid=profile; invalid["wallsEnabled"]=true;
        require(!CustomGeneratorProfiles::validate(invalid,&info).isEmpty(),"Incomplete walls accepted");
        invalid=profile; auto badInfo=info; badInfo["50002"]=QVariantMap{{"valid",true},{"ground",true},{"blocked",true}};
        require(!CustomGeneratorProfiles::validate(invalid,&badInfo).isEmpty(),"Blocked floors accepted");
        invalid=profile; invalid["materials"]=QVariantMap{};
        require(!CustomGeneratorProfiles::validate(invalid,&info).isEmpty(),"Missing materials accepted");
        auto missingInfo=info; missingInfo.remove("50002");
        require(!CustomGeneratorProfiles::validate(profile,&missingInfo).isEmpty(),"Unknown client item accepted");
        flag->store(true);
        require(!LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,profile).value("success").toBool(),"Cancelled profile produced a patch");
        flag->store(false);
        CustomGeneratorProfiles store(nullptr,directory.filePath("profiles"));
        const auto saved=store.save(profile);
        require(saved.value("success").toBool(),"Could not save profile");
        const QString id=saved.value("profile").toMap().value("id").toString();
        CustomGeneratorProfiles reopened(nullptr,directory.filePath("profiles"));
        require(reopened.load(id).value("materials")==profile.value("materials"),"Profile persistence lost materials");
        const QUrl exported=QUrl::fromLocalFile(directory.filePath("profile.json"));
        require(store.exportProfile(profile,exported).value("success").toBool(),"Profile export failed");
        require(store.importProfile(exported).value("success").toBool() && store.profiles().size()==2,"Import did not create an independent copy");
        require(store.remove(id).value("success").toBool() && store.profiles().size()==1,"Profile removal failed");
    }
    qInfo() << "Lua engine tests passed";
}

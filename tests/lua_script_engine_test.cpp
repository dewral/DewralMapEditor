#include "luascriptengine.h"
#include "customgeneratorprofiles.h"
#include "custommaprecipe.h"
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
    require(!run("_dme_tiles={setmetatable({}, {__index=function() error('bad result') end})}").value("success").toBool(),
            "Result conversion executed a throwing metamethod");
    require(!run("_dme_tiles={setmetatable({}, {__index=function() while true do end end})}").value("success").toBool(),
            "Result conversion executed a looping metamethod");
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
    // Exercise the recipe on a known connected layout without requiring an external checkout.
    const QByteArray brushFixture=R"lua(
DME_PROFILE={kind='dungeon',width=24,height=24,rooms=2,density=0,bordersEnabled=true,
 materials={background={{id=10000,weight=1}},floor={{id=10001,weight=1},{id=10002,weight=1}},secondary={{id=10003,weight=1}}},
 itemInfo={},groundBorders={}}
for id=10000,10003 do DME_PROFILE.itemInfo[tostring(id)]={ground=true,blocked=false} end
for group=1,3 do
 local set={groundIds=group==1 and {10001,10003} or {10002},to=group==3 and 'first ground' or '*',
  toGroundIds=group==3 and {10001} or {},align=group==3 and 'inner' or 'outer',zorder=group,materials={}}
 for slot=1,12 do
  local id=19000+group*1000+slot
  set.materials['border'..slot]={{id=id,weight=1}}
  DME_PROFILE.itemInfo[tostring(id)]={ground=false,blocked=false}
 end
 DME_PROFILE.groundBorders[group]=set
end
CLI_FINAL_MAP_TABLE={}
function doCreateItemMock(id,count,p)
 table.insert(CLI_FINAL_MAP_TABLE[p.x][p.y][p.z],{itemid=id,typeOrCount=count})
end
GroundMapper={new=function()
 return {doMainGround=function(_,items)
  for x=32,55 do CLI_FINAL_MAP_TABLE[x]={}; for y=32,55 do
   local id=(x>=38 and x<=50 and y>=38 and y<=50) and items[1][1] or items[0][1]
   CLI_FINAL_MAP_TABLE[x][y]={[7]={{itemid=id}}}
  end end
 end}
end}
CaveGroundMapper=GroundMapper
Cursor={new=function() return {} end}
WayPointer={new=function() return {_createPathBetweenTwoPointsTSP=function() end,_connectTwoPoints=function() end} end}
CaveWayPointer=WayPointer
DungeonRoomBuilder={new=function() return {createRooms=function() end} end}
CaveRoomBuilder=DungeonRoomBuilder
)lua";
    result=run(brushFixture+customMapRecipe);
    if (!result.value("success").toBool()) qFatal("Ground border fixture: %s",qPrintable(result.value("error").toString()));
    require(result.value("tileCount").toInt()==576,"Ground border recipe lost tiles");
    const auto generatedTiles=result.value("tiles").toList();
    QHash<int,int> grounds;
    for (const QVariant &value : generatedTiles) {
        const auto tile=value.toMap();
        for (const QVariant &entry : tile.value("items").toList())
            if (entry.toMap().value("ground").toBool()) grounds[tile.value("y").toInt()*65536+tile.value("x").toInt()]=entry.toMap().value("id").toInt();
    }
    QSet<int> borderFamilies;
    for (const QVariant &value : generatedTiles) {
        const auto tile=value.toMap();
        const int key=tile.value("y").toInt()*65536+tile.value("x").toInt();
        const int center=grounds.value(key);
        for (const QVariant &entry : tile.value("items").toList()) {
            const int id=entry.toMap().value("id").toInt();
            if (entry.toMap().value("ground").toBool()) continue;
            const int family=id/1000;
            borderFamilies.insert(family);
            require(family>=20 && family<=22,"Ground border fixture emitted an unselected item");
            require(family==22 ? center==10002 : family==20 ? center!=10001 && center!=10003 : center!=10002,
                "Border was placed on the wrong ground");
            bool neighbourMatches=false;
            for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
                if (!dx && !dy) continue;
                const int other=grounds.value(key+dy*65536+dx);
                neighbourMatches |= family==20 ? other==10001 || other==10003 : family==21 ? other==10002 : other==10001;
            }
            require(neighbourMatches,"A border is unrelated to its neighbouring ground");
        }
    }
    require(borderFamilies==QSet<int>{20,21,22},"A ground border set was skipped");
    require(result.value("tiles")==run(brushFixture+customMapRecipe).value("tiles"),"Ground border selection is not deterministic");
    // Percentages change actual floor selection both with and without ground-specific borders.
    for (const bool borders : {false,true}) for (const int percent : {0,100}) {
        const QByteArray settings="DME_PROFILE.secondaryDensity="+QByteArray::number(percent)
            +"; DME_PROFILE.bordersEnabled="+(borders ? QByteArray("true") : QByteArray("false"))+"\n";
        const auto coverage=run(brushFixture+settings+customMapRecipe);
        require(coverage.value("success").toBool(),"Secondary floor coverage failed");
        int secondaryCount=0,primaryCount=0;
        for (const QVariant &tile : coverage.value("tiles").toList()) for (const QVariant &item : tile.toMap().value("items").toList()) {
            const int id=item.toMap().value("id").toInt();
            secondaryCount+=id==10003; primaryCount+=id==10001 || id==10002;
        }
        require(percent==0 ? secondaryCount==0 && primaryCount>0 : secondaryCount>0 && primaryCount==0,
            "Secondary floor percentage was ignored");
    }
    const QByteArray advancedFixture=brushFixture+R"lua(
DME_PROFILE.rooms=4; DME_PROFILE.roomSpacing=6; DME_PROFILE.roomWidth=5; DME_PROFILE.roomHeight=9
DME_PROFILE.corridorWidth=5; DME_PROFILE.connectionMode='nearest'; DME_PROFILE.extraConnections=2
local connections={}
WayPointer.new=function()
 return {_createPathBetweenTwoPointsTSP=function(_,items,a,b,width)
  assert(width==5,'Corridor width not forwarded')
  local ka,kb=a.x..':'..a.y,b.x..':'..b.y
  local k=ka<kb and ka..'/'..kb or kb..'/'..ka
  assert(not connections[k],'Duplicate room connection'); connections[k]=true
 end}
end
DungeonRoomBuilder.new=function()
 return {createRooms=function(_,items,shapes)
  assert(shapes[1].width==5 and shapes[1].height==9,'Room dimensions not forwarded')
 end}
end
function checkConnections() local count=0; for _ in pairs(connections) do count=count+1 end; assert(count==5,'Extra connections not generated') end
)lua";
    result=run(advancedFixture+customMapRecipe+"\ncheckConnections()\n");
    require(result.value("success").toBool(),"Advanced layout parameters were not used");
    require(!run(brushFixture+"for _,item in pairs(DME_PROFILE.itemInfo) do if not item.ground then item.blocked=true end end\n"+customMapRecipe)
        .value("success").toBool(),"Blocking ground borders bypassed the connectivity check");

    QVariantMap borderMaterials;
    QVariantMap profileInfo{{"10000",QVariantMap{{"valid",true},{"ground",true},{"blocked",false}}},
        {"10001",QVariantMap{{"valid",true},{"ground",true},{"blocked",false}}}};
    for (int slot=1;slot<=12;++slot) {
        borderMaterials["border"+QString::number(slot)]=QVariantList{QVariantMap{{"id",20000+slot},{"weight",25}}};
        profileInfo[QString::number(20000+slot)]=QVariantMap{{"valid",true},{"ground",false},{"blocked",false}};
    }
    const QVariantMap borderSet{{"name","First ground"},{"groundIds",QVariantList{10001}},{"align","outer"},
        {"to","*"},{"toGroundIds",QVariantList{}},{"zorder",1},{"materials",borderMaterials}};
    const QVariantMap associated{{"version",1},{"name","Ground border profile"},{"kind","dungeon"},
        {"width",24},{"height",24},{"rooms",2},{"seed",123},{"density",0},{"x",1000},{"y",1000},{"z",7},
        {"bordersEnabled",true},{"groundBorders",QVariantList{borderSet}},
        {"materials",QVariantMap{{"background",QVariantList{QVariantMap{{"id",10000},{"weight",1}}}},
            {"floor",QVariantList{QVariantMap{{"id",10001},{"weight",1}}}}}}};
    require(CustomGeneratorProfiles::validate(associated,&profileInfo).isEmpty(),"Valid ground border profile rejected");
    auto advanced=associated;
    const QVariantMap advancedValues{{"roomWidth",9},{"roomHeight",11},{"roomSpacing",12},{"corridorWidth",5},
        {"secondaryDensity",40},{"connectionMode","nearest"},{"extraConnections",4},{"cavePaths","direct"}};
    for (auto it=advancedValues.cbegin();it!=advancedValues.cend();++it) advanced[it.key()]=it.value();
    advanced["width"]=70; advanced["height"]=70; advanced["rooms"]=20;
    require(CustomGeneratorProfiles::validate(advanced,&profileInfo).isEmpty(),"Advanced settings rejected");
    for (const QString &key : {QString("roomWidth"),QString("roomHeight"),QString("roomSpacing"),QString("corridorWidth"),QString("secondaryDensity"),QString("extraConnections")}) {
        auto invalid=advanced; invalid[key]=-1;
        require(!CustomGeneratorProfiles::validate(invalid).isEmpty(),"Invalid advanced value accepted");
        invalid[key]=2.5;
        require(!CustomGeneratorProfiles::validate(invalid).isEmpty(),"Fractional advanced value accepted");
    }
    for (const QString &key : {QString("connectionMode"),QString("cavePaths")}) {
        auto invalid=advanced; invalid[key]="unknown";
        require(!CustomGeneratorProfiles::validate(invalid).isEmpty(),"Unknown layout mode accepted");
    }
    auto tooLarge=associated; tooLarge["roomWidth"]=21;
    require(!CustomGeneratorProfiles::validate(tooLarge).isEmpty(),"Rooms exceeding map bounds accepted");
    auto tooSparse=associated; tooSparse["roomSpacing"]=40;
    require(!CustomGeneratorProfiles::validate(tooSparse).isEmpty(),"Too many rooms for advanced spacing accepted");
    require(CustomGeneratorProfiles::itemIds(associated).contains(20012),"Border items missing from client validation");
    auto invalidBorderProfile=associated; auto invalidSet=borderSet;
    invalidSet["groundIds"]=QVariantList{10002}; invalidBorderProfile["groundBorders"]=QVariantList{invalidSet};
    require(!CustomGeneratorProfiles::validate(invalidBorderProfile).isEmpty(),"Border accepted for an unselected ground");
    auto missingBorderInfo=profileInfo; missingBorderInfo.remove("20001");
    require(!CustomGeneratorProfiles::validate(associated,&missingBorderInfo).isEmpty(),"Unknown border item accepted");
    auto sparse=associated; auto sparseSet=borderSet;
    sparseSet["materials"]=QVariantMap{{"border1",borderMaterials.value("border1")}};
    sparse["groundBorders"]=QVariantList{sparseSet};
    require(CustomGeneratorProfiles::validate(sparse,&profileInfo).isEmpty(),"Sparse brush border slots rejected");
    CustomGeneratorProfiles borderStore(nullptr,directory.filePath("ground-border-profiles"));
    const auto storedBorder=borderStore.save(associated);
    require(storedBorder.value("success").toBool(),"Ground border profile save failed");
    CustomGeneratorProfiles reopenedBorders(nullptr,directory.filePath("ground-border-profiles"));
    const QString storedBorderId=storedBorder.value("profile").toMap().value("id").toString();
    require(reopenedBorders.load(storedBorderId).value("groundBorders")==associated.value("groundBorders"),"Saved ground borders were lost");
    const QUrl borderExport=QUrl::fromLocalFile(directory.filePath("ground-border-profile.json"));
    require(borderStore.exportProfile(associated,borderExport).value("success").toBool(),"Ground border profile export failed");
    const auto importedBorder=borderStore.importProfile(borderExport);
    require(importedBorder.value("success").toBool() && importedBorder.value("profile").toMap().value("groundBorders")==associated.value("groundBorders"),
        "Import lost ground border ownership or variants");
    const auto savedAdvanced=borderStore.save(advanced);
    require(savedAdvanced.value("success").toBool(),"Advanced profile save failed");
    CustomGeneratorProfiles restoredAdvanced(nullptr,directory.filePath("ground-border-profiles"));
    const auto advancedProfile=restoredAdvanced.load(savedAdvanced.value("profile").toMap().value("id").toString());
    require(borderStore.exportProfile(advanced,borderExport).value("success").toBool(),"Advanced profile export failed");
    const auto importedAdvanced=borderStore.importProfile(borderExport).value("profile").toMap();
    for (auto it=advancedValues.cbegin();it!=advancedValues.cend();++it)
        require(advancedProfile.value(it.key())==it.value() && importedAdvanced.value(it.key())==it.value(),"Advanced settings lost during save or import/export");
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
            auto defaults=profile;
            const QVariantMap defaultAdvanced{{"roomWidth",7},{"roomHeight",7},{"roomSpacing",8},{"corridorWidth",3},
                {"secondaryDensity",15},{"connectionMode","chain"},{"extraConnections",0},{"cavePaths","winding"}};
            for (auto it=defaultAdvanced.cbegin();it!=defaultAdvanced.cend();++it) defaults[it.key()]=it.value();
            const auto defaultOutput=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,defaults);
            require(defaultOutput.value("success").toBool() && defaultOutput.value("tiles")==custom.value("tiles"),"Advanced defaults changed legacy profiles");
            auto tuned=profile;
            tuned["width"]=70; tuned["height"]=70; tuned["rooms"]=12; tuned["density"]=0;
            tuned["roomWidth"]=11; tuned["roomHeight"]=9; tuned["roomSpacing"]=12; tuned["corridorWidth"]=5;
            tuned["connectionMode"]="nearest"; tuned["extraConnections"]=3; tuned["cavePaths"]="direct"; tuned["secondaryDensity"]=100;
            auto tunedMaterials=materials; tunedMaterials["secondary"]=QVariantList{QVariantMap{{"id",50006},{"weight",1}}};
            auto tunedInfo=info; tunedInfo["50006"]=QVariantMap{{"valid",true},{"ground",true},{"blocked",false}};
            tuned["materials"]=tunedMaterials; tuned["itemInfo"]=tunedInfo;
            const auto tunedOutput=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,tuned);
            if (!tunedOutput.value("success").toBool()) qFatal("Advanced %s recipe: %s",qPrintable(kind),qPrintable(tunedOutput.value("error").toString()));
            require(tunedOutput.value("tileCount").toInt()==4900,"Advanced layout changed requested map bounds");
            int tunedFloorCount=0;
            for (const QVariant &tile : tunedOutput.value("tiles").toList()) for (const QVariant &item : tile.toMap().value("items").toList()) {
                const int id=item.toMap().value("id").toInt();
                require(tunedInfo.contains(QString::number(id)),"Advanced layout emitted an unselected item");
                require(id!=50002 && id!=50003,"100 percent secondary floor ignored in upstream integration");
                tunedFloorCount+=id==50006;
            }
            require(tunedFloorCount>300,"Advanced layout produced no useful floor area");
            const auto tunedAgain=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,tuned);
            require(tunedAgain.value("success").toBool() && tunedAgain.value("tiles")==tunedOutput.value("tiles"),"Advanced layout is not deterministic");
            qInfo() << "Custom" << kind << "passed:" << custom.value("itemCount") << "items";
        }
        // Optional categories must never introduce upstream's stock IDs.
        auto maximum=profile;
        maximum["kind"]="cave"; maximum["width"]=200; maximum["height"]=200; maximum["rooms"]=64; maximum["density"]=0;
        maximum["roomWidth"]=21; maximum["roomHeight"]=21; maximum["roomSpacing"]=20; maximum["corridorWidth"]=5;
        maximum["connectionMode"]="nearest"; maximum["extraConnections"]=16; maximum["cavePaths"]="direct";
        const auto maximumOutput=LuaScriptEngine::execute(generatorRoot.filePath("config.lua"),"profile",123,flag,120000,maximum);
        if (!maximumOutput.value("success").toBool()) qFatal("Maximum advanced map failed: %s",qPrintable(maximumOutput.value("error").toString()));
        require(maximumOutput.value("tileCount").toInt()==40000,"Maximum advanced map changed output bounds");
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

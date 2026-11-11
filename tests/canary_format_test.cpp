#include "otbmreader.h"
#include "appearancesreader.h"
#include "LzmaEnc.h"
#include "otbreader.h"
#include "sprreader.h"
#include <cstdlib>
#include <cstring>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#define REQUIRE(x) do { if (!(x)) { qCritical() << __LINE__ << #x; return 1; } } while(false)
static QByteArray bytes(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
static bool put(const QString &path, const QByteArray &data) { QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(data)==data.size(); }
static void *allocLzma(ISzAllocPtr,size_t size) { return std::malloc(size); }
static void freeLzma(ISzAllocPtr,void *ptr) { std::free(ptr); }
static QByteArray syntheticSheet() {
 QByteArray bmp(54+384*384*4,0); const uint32_t offset=54; std::memcpy(bmp.data()+10,&offset,4);
 for(int i=54;i<bmp.size();i+=4) { bmp[i]=char(255);bmp[i+2]=char(255);bmp[i+3]=char(255); }
 const int pixel=54+383*384*4; bmp[pixel]=3;bmp[pixel+1]=2;bmp[pixel+2]=1;
 QByteArray packed(bmp.size()+1024,0); SizeT count=packed.size(),propsSize=5; Byte props[5];
 CLzmaEncProps options; LzmaEncProps_Init(&options); options.numThreads=1;
 const ISzAlloc allocator={allocLzma,freeLzma};
 if(LzmaEncode(reinterpret_cast<Byte*>(packed.data()),&count,reinterpret_cast<const Byte*>(bmp.constData()),bmp.size(),&options,props,&propsSize,1,nullptr,&allocator,&allocator)!=SZ_OK)return {};
 packed.resize(count); QByteArray header=QByteArray::fromHex("700afa802400"); header.append(reinterpret_cast<const char*>(props),5); header.append(QByteArray(8,0)); return header+packed;
}
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); QTemporaryDir dir; REQUIRE(dir.isValid());
 const QString source=dir.filePath("source.otbm"), output=dir.filePath("output.otbm");
 OtbmReader map; REQUIRE(map.newMap(512,512,1511,4,4,0)); REQUIRE(map.otbmVersion()==2);
 REQUIRE(map.newMap(512,512,1511,4,4,1)); REQUIRE(map.otbmVersion()==4);
 REQUIRE(map.addItem(100,100,7,100)); REQUIRE(map.setCreatureAt(100,100,7,"Rat",60,false,2));
 REQUIRE(map.setCreatureAt(102,100,7,"Guide",60,true,1));
 map.setNumberedZonesEnabled(true); TileZoneIds zones; zones.add(9); REQUIRE(map.setTileZoneIds(100,100,7,zones));
 const int house=map.addHouse(1); REQUIRE(house>0); REQUIRE(map.setHouseTileAt(100,100,7,house));
 REQUIRE(map.saveFile(source));
 REQUIRE(put(dir.filePath(map.spawnFile()),R"xml(<monsters><monster centerx="100" centery="100" centerz="7" radius="5"><monster name="Rat" x="0" y="0" spawntime="60" weight="10"/><monster name="Cave Rat" x="0" y="0" spawntime="90" weight="3"/></monster></monsters>)xml"));
 REQUIRE(put(dir.filePath(map.houseFile()),R"xml(<houses><house houseid="1" name="Test" townid="1" beds="0" size="99" clientid="42"/></houses>)xml"));
 REQUIRE(put(dir.filePath(map.npcFile()),R"xml(<npcs><npc centerx="100" centery="100" centerz="7" radius="5"><npc name="Guide" x="1" y="0" spawntime="60" direction="1"/></npc></npcs>)xml"));
 OtbmReader classic; REQUIRE(classic.newMap(512,512,860,3,22,0));
 REQUIRE(!classic.importFile(source,0,0,0,false,true,1).value("success").toBool());
 OtbmReader loaded; OtbmItemTypes types; types.houseGeometryKnown=true; types.walls.set(101); loaded.setItemTypes(types);
 REQUIRE(loaded.loadFile(source)); REQUIRE(loaded.errorString().isEmpty());
 REQUIRE(loaded.addItem(101,100,7,100)); REQUIRE(loaded.setHouseTileAt(101,100,7,house));
 REQUIRE(loaded.addItem(102,100,7,101)); REQUIRE(loaded.setHouseTileAt(102,100,7,house));
 REQUIRE(loaded.setNpcSpawnAt(100,100,7,9));
 REQUIRE(loaded.npcSpawnRadiusAt(100,100,7)==9);
 REQUIRE(loaded.setNpcSpawnAt(100,100,7,12)); REQUIRE(loaded.undo());
 REQUIRE(loaded.npcSpawnRadiusAt(100,100,7)==9);
 REQUIRE(loaded.setNpcSpawnAt(200,200,7,4)); REQUIRE(loaded.undo()); REQUIRE(loaded.npcSpawnRadiusAt(200,200,7)==0);
 REQUIRE(loaded.saveFile(output));
 REQUIRE(bytes(dir.filePath(loaded.npcFile())).contains(R"(radius="9")"));
 REQUIRE(bytes(dir.filePath(loaded.zoneFile())).contains(R"(zoneid="9")"));
 OtbmReader again; again.setItemTypes(types); REQUIRE(again.loadFile(output));
 REQUIRE(again.tileAt(100,100,7)->zone_ids.values()==zones.values());
 REQUIRE(again.npcSpawnRadiusAt(100,100,7)==9);
 const QString recovery=dir.filePath("recovery.otbm"); REQUIRE(loaded.saveRecoveryFile(recovery));
 OtbmReader recovered; recovered.setItemTypes(types); REQUIRE(recovered.loadFile(recovery));
 recovered.adoptRecoveryIdentity(output,loaded.spawnFile(),loaded.houseFile(),loaded.npcFile(),loaded.zoneFile());
 REQUIRE(recovered.npcFile()==loaded.npcFile()); REQUIRE(recovered.zoneFile()==loaded.zoneFile());
 REQUIRE(recovered.saveFile(output)); REQUIRE(bytes(dir.filePath(recovered.spawnFile())).contains("Cave Rat"));
 const auto houses=bytes(dir.filePath(loaded.houseFile())); REQUIRE(houses.contains(R"(beds="0")")); REQUIRE(houses.contains(R"(size="2")")); REQUIRE(houses.contains(R"(clientid="42")"));
 const auto monsters=bytes(dir.filePath(loaded.spawnFile())); REQUIRE(monsters.contains("Cave Rat")); REQUIRE(monsters.contains(R"(weight="10")")); REQUIRE(monsters.contains(R"(weight="3")"));
 REQUIRE(loaded.clearCreatureAt(100,100,7)); REQUIRE(loaded.undo()); REQUIRE(loaded.saveFile(output)); REQUIRE(bytes(dir.filePath(loaded.spawnFile())).contains("Cave Rat"));
 const QString catalog=dir.filePath("catalog-content.json");
 REQUIRE(put(catalog,R"json([{"type":"appearances","file":"appearances.dat"},{"type":"sprite","file":"missing.lzma","spritetype":0,"firstspriteid":1,"lastspriteid":1}])json"));
 AppearanceSpriteSource sprites; QString error; REQUIRE(!sprites.loadCatalog(catalog,&error)); REQUIRE(!error.isEmpty());
 REQUIRE(put(dir.filePath("missing.lzma"),"invalid")); REQUIRE(sprites.loadCatalog(catalog,&error)); REQUIRE(!sprites.validateSheets(&error)); REQUIRE(error.contains("LZMA"));
 REQUIRE(!sprites.validateSheets(&error,[]{return true;})); REQUIRE(error.contains("cancelled"));
 REQUIRE(put(dir.filePath("missing.lzma"),syntheticSheet())); REQUIRE(sprites.validateSheets(&error));
 sprites.buildVirtualTable({1}); const auto image=sprites.image(sprites.virtualId(1,0,0));
 REQUIRE(image.pixelColor(0,0)==QColor(1,2,3,255)); REQUIRE(image.pixelColor(1,0).alpha()==0);
 const auto appearance=QByteArray::fromHex("0864120608021a0228011a040a020864220454696c65");
 QByteArray protobuf(1,char(0x0a)); protobuf.append(char(appearance.size())); protobuf.append(appearance);
 REQUIRE(put(dir.filePath("appearances.dat"),protobuf));
 AppearancesReader appearances; DatReader definitions;
 REQUIRE(definitions.loadAppearances(appearances,dir.path())); REQUIRE(definitions.isAppearanceClient());
 OtbReader items; items.setDatReader(&definitions); REQUIRE(items.buildFromAppearances(definitions,appearances.takeItemInfo()));
 REQUIRE(items.clientIdForServerId(100)==100); REQUIRE(items.isClientGroundForServerId(100));
 AppearanceSpriteSource::registerSource(appearances.source()); SprReader decoder;
 REQUIRE(decoder.loadFile(catalog)); REQUIRE(decoder.isAppearanceSource());
 REQUIRE(decoder.loadSprite(1)->image.pixelColor(0,0)==QColor(1,2,3,255));
 qInfo()<<"Canary mode, weighted spawns, undo, house geometry, beds and asset validation passed"; return 0;
}

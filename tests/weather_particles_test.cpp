#include "weatherparticles.h"
#include "weatherexposure.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>
#include <limits>
#define CHECK(expr) do { if(!(expr)) { qCritical()<<__LINE__<<#expr; return false; } } while(false)
namespace {
bool tests() {
    auto open=[](int,int){return true;};
    WeatherParticles scene;
    scene.setCamera(100,100,32,{1000,800});
    CHECK(scene.particleCount()==0 && scene.geometry(open).empty());
    WeatherOptions options; options.levels={1,1,1,1,1,1}; options.quality=2;
    scene.configure(options);
    CHECK(scene.particleCount()>0 && scene.particleCount()<=WeatherParticles::particleLimit);
    auto geometry=scene.geometry(open);
    CHECK(!geometry.empty() && geometry.size()<=WeatherParticles::vertexLimit && geometry.size()%3==0);
    for(auto vertex:geometry) CHECK(std::isfinite(vertex.x) && std::isfinite(vertex.y)
        && vertex.r<=vertex.a && vertex.g<=vertex.a && vertex.b<=vertex.a);
    CHECK(scene.geometry([](int,int){return false;}).empty());
    const auto position=scene.position(0);
    scene.advance(0);
    CHECK(scene.position(0)==position && scene.elapsed()==0);
    scene.advance(.05);
    CHECK(scene.position(0)!=position && scene.elapsed()==.05);
    auto longFrame=scene, normalFrame=scene;
    longFrame.advance(3600); normalFrame.advance(.1);
    CHECK(longFrame.position(0)==normalFrame.position(0));
    const auto beforePan=scene.position(0);
    scene.setCamera(101,101,32,{1000,800});
    CHECK((scene.position(0)-beforePan-QPointF(1,1)).manhattanLength()<1e-9);
    const auto screenPosition=(scene.position(0)-QPointF(101,101))*32;
    scene.setCamera(101,101,64,{1000,800});
    CHECK(((scene.position(0)-QPointF(101,101))*64-screenPosition).manhattanLength()<1e-9);
    // Pool entries are recycled through repeated lifetimes without growth or NaNs.
    const int poolSize=scene.particleCount();
    for(int frame=0;frame<2400;++frame) scene.advance(1.0/60);
    CHECK(scene.particleCount()==poolSize);
    for(auto vertex:scene.geometry(open)) CHECK(std::isfinite(vertex.x) && std::isfinite(vertex.y)
        && vertex.r<=vertex.a && vertex.g<=vertex.a && vertex.b<=vertex.a);
    scene.setCamera(100,100,4,{1000,800});
    CHECK(scene.geometry(open).empty());
    scene.setCamera(100,100,32,{4000,2200});
    CHECK(scene.particleCount()<=WeatherParticles::particleLimit);
    for(int kind:{0,1,2,3,4,5}) {
        WeatherOptions one; one.levels[kind]=1;
        scene.setCamera(100,100,32,{1000,800}); scene.configure(one);
        CHECK(!scene.geometry(open).empty());
    }
    options.levels.fill(0); scene.configure(options);
    CHECK(scene.particleCount()==0 && scene.geometry(open).empty());
    options.levels.fill(std::numeric_limits<double>::quiet_NaN());
    options.wind=std::numeric_limits<double>::infinity(); scene.configure(options);
    CHECK(scene.particleCount()==0 && scene.geometry(open).empty());

    OtbmReader map; CHECK(map.newMap(512,512,860,2,22));
    map.placeItem(100,100,7,100,0,false,true);
    map.placeItem(101,100,7,100,0,false,true);
    CHECK(weatherTileExposed(map,100,100,7,true));
    CHECK(!weatherTileExposed(map,99,100,7,true));
    map.placeItem(101,101,6,100,0,false,true);
    CHECK(!weatherTileExposed(map,100,100,7,true));
    CHECK(weatherTileExposed(map,100,100,7,false));
    CHECK(weatherTileExposed(map,101,100,7,true));
    map.placeItem(100,100,8,100,0,false,true);
    CHECK(!weatherTileExposed(map,100,100,8,true));
    CHECK(weatherTileExposed(map,100,100,8,false));
    WeatherOptions fog; fog.levels[3]=1;
    scene.configure(fog); scene.setCamera(100,100,32,{64,32});
    const auto editCount=map.editOperationCount();
    const auto dirty=map.isDirty();
    geometry=scene.geometry([&](int x,int y){return weatherTileExposed(map,x,y,7,true);});
    CHECK(geometry.size()==6); // exactly the uncovered tile; no fog bleeding indoors
    for(auto vertex:geometry) CHECK(vertex.x>=32 && vertex.x<=64 && vertex.y>=0 && vertex.y<=32);
    CHECK(map.editOperationCount()==editCount && map.isDirty()==dirty);
    return true;
}
}
int main(int argc,char **argv) { QCoreApplication app(argc,argv); return tests()?0:1; }

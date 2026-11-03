// SPDX-License-Identifier: MIT
// Weather-only visual primitives shared verbatim with DME. No renderer or map dependency.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace WeatherVisuals {
inline double safe(double v,double lo,double hi,double fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;}
inline double smooth(double t) { t=std::clamp(t,0.0,1.0);return t*t*(3-2*t); }
inline double noise(uint32_t seed,int64_t cell) {
    uint32_t h=uint32_t(cell)+seed;h=h*1664525u+1013904223u;h^=h>>16;h*=2246822519u;h^=h>>13;
    return double(h)/4294967295.0;
}
inline double flow(double time,double period,uint32_t seed) {
    const double phase=time/std::max(0.25,period);const auto cell=int64_t(std::floor(phase));
    const double t=smooth(phase-cell);
    return (noise(seed,cell)*(1-t)+noise(seed,cell+1)*t)*2-1;
}
inline double gust(double time,double period,uint32_t seed=0x57544852u) {
    return 0.75*flow(time,period,seed)+0.25*flow(time,period*0.37,seed+17);
}
// Original, non-tiled masks: three irregular horizontal bands, softened to zero at every edge.
inline std::vector<uint8_t> mask(int variant,int width=256,int height=128) {
    std::vector<uint8_t> pixels(size_t(width)*height*4);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const double u=(x+0.5)/width,v=(y+0.5)/height;
        const double center=0.5+0.12*std::sin(u*(5+variant*2)+variant*2.1)+0.05*std::sin(u*19+variant);
        double n=0,weight=0,amplitude=0.55;
        for(int octave=0;octave<4;++octave){
            const double f=double(2<<octave),px=u*f,py=v*f;
            const int ix=int(px),iy=int(py);const double tx=smooth(px-ix),ty=smooth(py-iy);
            auto sample=[&](int a,int b){return noise(0x7261696eu+variant*937u+octave*71u,int64_t(a)+int64_t(b)*4099);};
            n+=amplitude*((sample(ix,iy)*(1-tx)+sample(ix+1,iy)*tx)*(1-ty)+(sample(ix,iy+1)*(1-tx)+sample(ix+1,iy+1)*tx)*ty);
            weight+=amplitude;amplitude*=0.5;
        }
        const double band=std::exp(-std::pow((v-center)/(0.14+variant*0.025),2));
        const double edge=smooth(std::min(u,1-u)/0.22)*smooth(std::min(v,1-v)/0.17);
        const double alpha=std::clamp(band*edge*(0.25+0.9*n/weight),0.0,1.0);
        const size_t i=(size_t(y)*width+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=uint8_t(alpha*255);
    }
    return pixels;
}
struct FogOptions {
    double density=0,scale=1,speed=1,turbulence=25,wind=0,opacity=100;
    int limit=96;
};
struct FogPatch { double x,y,width,height,alpha;int texture; };
class Fog {
    struct Wisp { double x,y,age,life,width,height,drift,fade=1;uint32_t id;bool retiring=false; };
    std::vector<Wisp> particles;
    uint32_t seed=0x74657269u,sequence=0;
    double x=0,y=0,width=0,height=0,pixels=32,time=0;
    bool initialized=false;
    FogOptions options;
    double random() { return noise(seed,++sequence); }
    Wisp spawn(bool prewarm) {
        const bool broad=(sequence%3)==0;
        Wisp p;p.id=sequence;p.width=(broad?420:230)*(0.8+random()*0.4)/32;
        p.height=(broad?155:100)*(0.8+random()*0.4)/32;
        p.life=14+random()*8;p.age=prewarm?random()*p.life:0;p.drift=(random()-0.5)*0.08;
        const double margin=7*options.scale;
        p.x=x-margin+random()*(width+2*margin);p.y=y-margin*0.4+random()*(height+0.8*margin);
        return p;
    }
public:
    void setSeed(uint32_t value){seed=value;sequence=0;particles.clear();initialized=false;time=0;}
    void configure(FogOptions value){
        value.density=safe(value.density,0,1,0);value.scale=safe(value.scale,0.25,3,1);
        value.speed=safe(value.speed,0,3,1);value.opacity=safe(value.opacity,0,200,100);value.wind=safe(value.wind,-160,160,0);value.turbulence=safe(value.turbulence,0,100,25);
        value.limit=std::clamp(value.limit,0,192);options=value;
        if(value.density<=0 || value.limit==0){particles.clear();initialized=false;}
        if(particles.size()>size_t(value.limit))particles.resize(size_t(value.limit));
    }
    // Origin and dimensions in world tiles; pixels is the displayed tile width.
    void view(double originX,double originY,double viewWidth,double viewHeight,double tilePixels){
        if(!std::isfinite(originX)||!std::isfinite(originY)||!std::isfinite(tilePixels)||tilePixels<=0){clear();width=height=0;return;}
        viewWidth=safe(viewWidth,0,512,0);viewHeight=safe(viewHeight,0,512,0);
        if(initialized && (std::abs(originX-x)>std::max(1.0,width) || std::abs(originY-y)>std::max(1.0,height))) {
            particles.clear();initialized=false;
        }
        x=originX;y=originY;width=viewWidth;height=viewHeight;pixels=tilePixels;
    }
    void clear(){particles.clear();initialized=false;}
    int count()const{return int(particles.size());}
    void advance(double dt){
        if(width<=0 || height<=0 || options.density<=0 || options.limit<=0)return;
        dt=safe(dt,0,0.25,0);time+=dt;
        const double margin=7*options.scale;
        const int target=std::min(options.limit,int(std::ceil((width+2*margin)*(height+margin)*0.055*options.density)));
        int active=0;for(const auto &p:particles)if(!p.retiring)++active;
        for(auto it=particles.rbegin();it!=particles.rend() && active>target;++it)if(!it->retiring){it->retiring=true;--active;}
        while(active<target && particles.size()<size_t(options.limit)){particles.push_back(spawn(!initialized));++active;}
        initialized=true;
        for(auto &p:particles){
            p.age+=dt;
            const double local=flow(time,5.0,p.id+seed)*options.turbulence/100;
            p.x+=(options.wind/32*0.2+p.drift+local*0.11)*options.speed*dt;
            p.y+=flow(time,8.0,p.id+seed+41)*options.turbulence/100*0.045*options.speed*dt;
            if(p.retiring)p.fade=std::max(0.0,p.fade-dt*2);
            if(!p.retiring && (p.age>=p.life || p.x<x-margin || p.x>x+width+margin || p.y<y-margin || p.y>y+height+margin))p=spawn(false);
        }
        particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Wisp &p){return p.retiring && p.fade<=0;}),particles.end());
    }
    std::vector<FogPatch> patches()const{
        std::vector<FogPatch> out;out.reserve(particles.size());
        for(const auto &p:particles){
            const double fade=smooth(std::min(p.age,p.life-p.age)/2.5)*p.fade;
            const double w=p.width*options.scale*pixels,h=p.height*options.scale*pixels;
            out.push_back({(p.x-x)*pixels-w/2,(p.y-y)*pixels-h/2,w,h,
                fade*(p.width>10?0.095:0.17)*options.opacity/100,int(p.id%3)});
        }
        return out;
    }
};
}

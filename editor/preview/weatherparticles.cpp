#include "weatherparticles.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double pi=3.14159265358979323846;
double finiteClamp(double value,double lo,double hi) { return std::isfinite(value) ? std::clamp(value,lo,hi) : lo; }
double wrap(double value,double lo,double length) { return lo+std::fmod(std::fmod(value-lo,length)+length,length); }
}
void WeatherParticles::configure(WeatherOptions options)
{
    for(auto &level:options.levels) level=finiteClamp(level,0,1);
    options.wind=finiteClamp(options.wind,-160,160);
    options.quality=std::clamp(options.quality,0,2);
    m_options=options;
    reconcile();
}
void WeatherParticles::setCamera(double x,double y,double tileSize,QSizeF size)
{
    const bool resized=m_size!=size || m_scale!=tileSize;
    const double oldX=m_x,oldY=m_y,oldScale=m_scale;
    m_x=std::isfinite(x)?x:0; m_y=std::isfinite(y)?y:0;
    m_scale=finiteClamp(tileSize,1,256);
    m_size={std::max(0.0,size.width()),std::max(0.0,size.height())};
    m_bounds={m_x-4,m_y-4,m_size.width()/m_scale+8,m_size.height()/m_scale+8};
    // Precipitation follows the viewport, as in TibiaClient: walking must not
    // add camera velocity to the wind. Fog is sampled separately in world space.
    for(auto &p:m_particles) {
        p.x=m_x+(p.x-oldX)*oldScale/m_scale;
        p.y=m_y+(p.y-oldY)*oldScale/m_scale;
    }
    if(resized) reconcile();
}
double WeatherParticles::random() { return std::generate_canonical<double,32>(m_random); }
void WeatherParticles::respawn(Particle &p,bool prewarm)
{
    p.x=m_bounds.left()+random()*m_bounds.width();
    p.y=m_bounds.top()+random()*m_bounds.height();
    p.phase=random()*2*pi;
    p.speed=0.7+random()*0.6;
    p.lifetime=p.kind==0?0.6+random()*0.5:4+random()*6;
    p.age=prewarm?random()*p.lifetime:0;
}
void WeatherParticles::reconcile()
{
    const double quality[]={0.35,0.65,1.0};
    const double area=std::clamp(m_size.width()*m_size.height()/800000.0,0.0,2.0);
    const int kinds[]={0,2,4,5};
    const int counts[]={550,300,110,95};
    std::vector<Particle> next;
    next.reserve(particleLimit);
    for(int i=0;i<4;++i) {
        const int kind=kinds[i];
        const double level=kind==0?std::max(m_options.levels[0],m_options.levels[1]):m_options.levels[kind];
        const int count=std::min(particleLimit-int(next.size()),int(counts[i]*area*quality[m_options.quality]*level));
        int kept=0;
        for(const auto &p:m_particles) if(p.kind==kind && kept<count) { next.push_back(p); ++kept; }
        while(kept++<count) {
            Particle particle {}; particle.kind=kind;
            respawn(particle,true); next.push_back(particle);
        }
    }
    m_particles=std::move(next);
}
void WeatherParticles::advance(double seconds)
{
    const double dt=finiteClamp(seconds,0,0.1);
    m_time+=dt;
    for(auto &p:m_particles) {
        p.age+=dt;
        if(p.age>=p.lifetime) respawn(p,false);
        const double wind=m_options.wind/32.0;
        if(p.kind==0) { p.x+=wind*dt; p.y+=(18+12*m_options.levels[1])*p.speed*dt; }
        else if(p.kind==2) { p.x+=(wind*0.35+std::sin(m_time+p.phase)*0.5)*dt; p.y+=1.3*p.speed*dt; }
        else if(p.kind==4) { p.x+=(wind*0.7+std::sin(m_time*1.7+p.phase))*dt; p.y+=0.8*p.speed*dt; }
        else { p.x+=std::sin(m_time*0.7+p.phase)*0.5*dt; p.y+=std::cos(m_time+p.phase)*0.3*dt; }
        p.x=wrap(p.x,m_bounds.left(),m_bounds.width());
        p.y=wrap(p.y,m_bounds.top(),m_bounds.height());
    }
}
std::vector<WeatherVertex> WeatherParticles::geometry(const std::function<bool(int,int)> &exposed) const
{
    std::vector<WeatherVertex> out;
    if(m_size.isEmpty() || m_scale<8) return out;
    out.reserve(std::min(vertexLimit,int(m_particles.size())*48+4096));
    auto vertex=[](QPointF p,QColor color,double opacity) {
        const double a=finiteClamp(opacity,0,1);
        return WeatherVertex{float(p.x()),float(p.y()),
            static_cast<unsigned char>(color.red()*a),static_cast<unsigned char>(color.green()*a),
            static_cast<unsigned char>(color.blue()*a),static_cast<unsigned char>(255*a)};
    };
    double particleOpacity=1;
    auto triangle=[&](QPointF a,QPointF b,QPointF c,QColor color,double aa,double ab,double ac) {
        if(out.size()+3>vertexLimit) return;
        out.push_back(vertex(a,color,aa*particleOpacity)); out.push_back(vertex(b,color,ab*particleOpacity)); out.push_back(vertex(c,color,ac*particleOpacity));
    };
    auto screen=[&](double x,double y) { return QPointF((x-m_x)*m_scale,(y-m_y)*m_scale); };
    // Continuous fog is sampled in world space and clipped per exposed tile.
    // Bound work on overview maps instead of scanning the complete document.
    const double fog=m_options.levels[3];
    if(fog>0) {
        const int x0=int(std::floor(m_x)),y0=int(std::floor(m_y));
        const int x1=int(std::ceil(m_x+m_size.width()/m_scale)),y1=int(std::ceil(m_y+m_size.height()/m_scale));
        const double budgets[]={1800,5000,12000};
        const int stride=std::max(1,int(std::ceil(std::sqrt(double((x1-x0+1)*(y1-y0+1))/budgets[m_options.quality]))));
        const double drift=m_time*m_options.wind/320.0;
        auto opacity=[&](int x,int y) { return fog*(0.08+0.18*(0.5+0.5*std::sin(x*0.19-drift+std::sin(y*0.23+m_time*0.06)))); };
        const int gridX=int(std::floor(double(x0)/stride))*stride;
        const int gridY=int(std::floor(double(y0)/stride))*stride;
        for(int y=gridY;y<y1;y+=stride) for(int x=gridX;x<x1;x+=stride) {
            bool outdoor=true;
            for(int sy=y;sy<y+stride && outdoor;++sy) for(int sx=x;sx<x+stride;++sx)
                if(!exposed(sx,sy)) { outdoor=false; break; }
            if(!outdoor) continue;
            auto a=screen(x,y),b=screen(x+stride,y),c=screen(x+stride,y+stride),d=screen(x,y+stride);
            const QColor color("#d5e2e8");
            triangle(a,b,c,color,opacity(x,y),opacity(x+stride,y),opacity(x+stride,y+stride));
            triangle(a,c,d,color,opacity(x,y),opacity(x+stride,y+stride),opacity(x,y+stride));
        }
    }
    for(const auto &p:m_particles) {
        const QPointF center=screen(p.x,p.y);
        if(center.x()<-32 || center.y()<-32 || center.x()>m_size.width()+32 || center.y()>m_size.height()+32) continue;
        if(!exposed(int(std::floor(p.x)),int(std::floor(p.y)))) continue;
        const double fade=std::clamp(std::min(p.age,p.lifetime-p.age)/std::min(0.35,p.lifetime*0.2),0.0,1.0);
        particleOpacity=fade*fade*(3-2*fade);
        if(p.kind==0) {
            const double length=(0.42+0.4*m_options.levels[1])*m_scale;
            QPointF direction(m_options.wind/32.0,18+12*m_options.levels[1]);
            const double norm=std::hypot(direction.x(),direction.y()); direction/=norm;
            const QPointF start=center-direction*length;
            if(!exposed(int(std::floor(m_x+start.x()/m_scale)),int(std::floor(m_y+start.y()/m_scale)))) continue;
            const QPointF normal(direction.y()*0.55,-direction.x()*0.55);
            const QColor color("#b6d9f0");
            triangle(start-normal,start+normal,center+normal,color,0.02,0.02,0.55);
            triangle(start-normal,center+normal,center-normal,color,0.02,0.55,0.55);
        } else if(p.kind==4) {
            const double angle=m_time*1.5+p.phase;
            const QPointF axis(std::cos(angle)*m_scale*0.15,std::sin(angle)*m_scale*0.15);
            const QPointF side(-axis.y()*0.45,axis.x()*0.45);
            const QColor color=p.phase<pi?QColor("#d9a651"):QColor("#8fb36b");
            triangle(center-axis,center+side,center+axis,color,0.85,0.9,0.85);
            triangle(center-axis,center+axis,center-side,color,0.85,0.85,0.9);
        } else {
            const bool firefly=p.kind==5;
            const double radius=firefly?std::max(2.0,m_scale*0.2):std::max(0.9,m_scale*0.065*p.speed);
            const double alpha=firefly?0.25+0.45*(0.5+0.5*std::sin(m_time*2+p.phase)):0.85;
            const QColor color=firefly?QColor("#e0f4a0"):QColor("#edf6ff");
            for(int i=0;i<8;++i) {
                const QPointF a=center+QPointF(std::cos(i*pi/4),std::sin(i*pi/4))*radius;
                const QPointF b=center+QPointF(std::cos((i+1)*pi/4),std::sin((i+1)*pi/4))*radius;
                triangle(center,a,b,color,alpha,0,0);
            }
        }
    }
    return out;
}

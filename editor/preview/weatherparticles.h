#pragma once
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <array>
#include <functional>
#include <random>
#include <vector>

struct WeatherOptions {
    // rain, storm, snow, fog, leaves, fireflies (0..1).
    std::array<double,6> levels {};
    double wind = 20; // pixels per second at 100% zoom
    int quality = 1;
};
struct WeatherVertex { float x,y; unsigned char r,g,b,a; };
class WeatherParticles {
public:
    static constexpr int particleLimit = 1600;
    static constexpr int vertexLimit = 200000;
    void configure(WeatherOptions options);
    void setCamera(double x,double y,double tileSize,QSizeF size);
    void advance(double seconds);
    std::vector<WeatherVertex> geometry(const std::function<bool(int,int)> &exposed) const;
    int particleCount() const { return int(m_particles.size()); }
    double elapsed() const { return m_time; }
    QPointF position(int i) const { return {m_particles.at(i).x,m_particles.at(i).y}; }
private:
    struct Particle { int kind; double x,y,phase,speed,age,lifetime; };
    double random();
    void reconcile();
    void respawn(Particle &particle, bool prewarm);
    WeatherOptions m_options;
    double m_time=0,m_x=0,m_y=0,m_scale=32;
    QSizeF m_size;
    QRectF m_bounds;
    std::vector<Particle> m_particles;
    std::mt19937 m_random {0xd3e510u};
};

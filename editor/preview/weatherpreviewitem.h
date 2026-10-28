#pragma once
#include "weatherparticles.h"
#include "otbmreader.h"
#include <QQuickItem>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class WeatherPreviewItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(WeatherPreview)
    Q_PROPERTY(OtbmReader *map READ map WRITE setMap NOTIFY mapChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration WRITE setConfiguration NOTIFY configurationChanged)
    Q_PROPERTY(double originX READ originX WRITE setOriginX NOTIFY cameraChanged)
    Q_PROPERTY(double originY READ originY WRITE setOriginY NOTIFY cameraChanged)
    Q_PROPERTY(double tileSize READ tileSize WRITE setTileSize NOTIFY cameraChanged)
    Q_PROPERTY(int floor READ floor WRITE setFloor NOTIFY cameraChanged)
    Q_PROPERTY(int particleCount READ particleCount NOTIFY particleCountChanged)
public:
    explicit WeatherPreviewItem(QQuickItem *parent=nullptr);
    OtbmReader *map() const { return m_map; }
    void setMap(OtbmReader *map);
    QVariantMap configuration() const { return m_configuration; }
    void setConfiguration(const QVariantMap &options);
    double originX() const { return m_x; }
    double originY() const { return m_y; }
    double tileSize() const { return m_scale; }
    int floor() const { return m_floor; }
    int particleCount() const { return m_particles.particleCount(); }
    double simulationTime() const { return m_particles.elapsed(); }
    void setOriginX(double value);
    void setOriginY(double value);
    void setTileSize(double value);
    void setFloor(int value);
signals:
    void mapChanged();
    void configurationChanged();
    void cameraChanged();
    void particleCountChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *old,UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &now,const QRectF &before) override;
private:
    void cameraUpdated();
    void updateRunning();
    bool hasWeather() const;
    bool exposed(int x,int y) const;
    QPointer<OtbmReader> m_map;
    QVariantMap m_configuration;
    WeatherOptions m_options;
    WeatherParticles m_particles;
    QTimer m_timer;
    QElapsedTimer m_clock;
    double m_x=0,m_y=0,m_scale=32;
    int m_floor=7;
};

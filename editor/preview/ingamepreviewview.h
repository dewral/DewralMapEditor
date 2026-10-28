#pragma once
#include "ingamepreviewcontroller.h"
#include "ingamepreviewscene.h"
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QQuickRhiItem>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
class MapView;
class IngamePreviewView : public QQuickRhiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(IngamePreviewView)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(
        IngamePreviewController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool lighting READ lighting WRITE setLighting NOTIFY lightingChanged)
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged)
    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
    Q_PROPERTY(bool showCreatures MEMBER m_showCreatures NOTIFY displayOptionsChanged)
    Q_PROPERTY(bool showItems MEMBER m_showItems NOTIFY displayOptionsChanged)
    Q_PROPERTY(bool animateItems MEMBER m_animateItems NOTIFY displayOptionsChanged)
    Q_PROPERTY(bool showBorders MEMBER m_showBorders NOTIFY displayOptionsChanged)
    Q_PROPERTY(QVariantList labels READ labels NOTIFY labelsChanged)
public:
    explicit IngamePreviewView(QQuickItem *parent = nullptr);
    QQuickRhiItemRenderer *createRenderer() override;
    MapView *source() const;
    void setSource(MapView *);
    IngamePreviewController *controller() const
    {
        return m_controller;
    }
    void setController(IngamePreviewController *);
    bool lighting() const
    {
        return m_lighting;
    }
    void setLighting(bool value)
    {
        if (m_lighting == value)
            return;
        m_lighting = value;
        emit lightingChanged();
        refresh();
    }
    QString playerName() const
    {
        return m_name;
    }
    void setPlayerName(const QString &value)
    {
        if (m_name == value)
            return;
        m_name = value;
        emit playerNameChanged();
        refresh();
    }
    int maxFps() const
    {
        return m_maxFps;
    }
    void setMaxFps(int value)
    {
        value = qBound(1, value <= 0 ? 60 : value, 200);
        if (m_maxFps == value)
            return;
        m_maxFps = value;
        m_timer.setInterval(qMax(1, 1000 / value));
        emit maxFpsChanged();
    }
    QVariantList labels() const
    {
        return m_frame.labels;
    }
    const QImage &atlas() const
    {
        return m_atlas;
    }
    const IngamePreviewFrame &frame() const
    {
        return m_frame;
    }
    const QVector<float> &instances() const
    {
        return m_instances;
    }
    quint64 atlasRevision() const
    {
        return m_atlasRevision;
    }
    void refresh();
signals:
    void displayOptionsChanged();
    void sourceChanged();
    void controllerChanged();
    void lightingChanged();
    void playerNameChanged();
    void maxFpsChanged();
    void labelsChanged();

protected:
    void geometryChange(const QRectF &next, const QRectF &previous) override;

private:
    void resetAssets();
    QPointer<MapView> m_source;
    QPointer<IngamePreviewController> m_controller;
    QTimer m_timer;
    QElapsedTimer m_clock;
    IngamePreviewScene m_scene;
    IngamePreviewFrame m_frame;
    QImage m_atlas;
    struct Patch
    {
        QPoint offset, atlas;
    };
    QHash<quint64, QVector<Patch>> m_slots;
    QVector<float> m_instances;
    int m_cells = 0, m_maxFps = 60;
    quint64 m_atlasRevision = 0;
    bool m_lighting = true;
    bool m_showCreatures = true, m_showItems = true, m_animateItems = true, m_showBorders = true;
    QString m_name;
};

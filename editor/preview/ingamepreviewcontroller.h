#ifndef INGAMEPREVIEWCONTROLLER_H
#define INGAMEPREVIEWCONTROLLER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QQueue>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class MapView;

class IngamePreviewController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(IngamePreviewController)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int x READ x NOTIFY positionChanged)
    Q_PROPERTY(int y READ y NOTIFY positionChanged)
    Q_PROPERTY(int z READ z NOTIFY positionChanged)
    Q_PROPERTY(qreal visualX READ visualX NOTIFY visualPositionChanged)
    Q_PROPERTY(qreal visualY READ visualY NOTIFY visualPositionChanged)
    Q_PROPERTY(bool positioned READ positioned NOTIFY positionChanged)
    Q_PROPERTY(bool walking READ walking NOTIFY walkingChanged)
    Q_PROPERTY(int direction READ direction NOTIFY directionChanged)
    Q_PROPERTY(qreal walkProgress READ walkProgress NOTIFY visualPositionChanged)
    Q_PROPERTY(int walkAnimationTick READ walkAnimationTick NOTIFY visualPositionChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool noClip READ noClip WRITE setNoClip NOTIFY noClipChanged)
    Q_PROPERTY(int lookType READ lookType WRITE setLookType NOTIFY lookTypeChanged)
    Q_PROPERTY(QString lastBlockReason READ lastBlockReason NOTIFY lastBlockReasonChanged)

public:
    explicit IngamePreviewController(QObject *parent = nullptr);

    MapView *source() const { return m_source; }
    void setSource(MapView *source);
    int x() const { return m_x; }
    int y() const { return m_y; }
    int z() const { return m_z; }
    qreal visualX() const { return m_visualX; }
    qreal visualY() const { return m_visualY; }
    bool positioned() const { return m_x >= 0 && m_y >= 0; }
    bool walking() const { return m_walking; }
    int direction() const { return m_direction; }
    qreal walkProgress() const { return m_progress; }
    int walkAnimationTick() const { return m_walkAnimationTick; }
    int speed() const { return m_speed; }
    void setSpeed(int speed);
    bool noClip() const { return m_noClip; }
    void setNoClip(bool enabled);
    int lookType() const { return m_lookType; }
    QString lastBlockReason() const { return m_lastBlockReason; }
    void setLookType(int lookType);

    Q_INVOKABLE void setPosition(int x, int y, int z);
    Q_INVOKABLE bool walk(int dx, int dy);
    Q_INVOKABLE void changeFloor(int delta);
    Q_INVOKABLE void stop();

signals:
    void sourceChanged();
    void positionChanged();
    void visualPositionChanged();
    void walkingChanged();
    void directionChanged();
    void speedChanged();
    void noClipChanged();
    void lookTypeChanged();
    void movementBlocked(int x, int y, int z);
    void lastBlockReasonChanged();

private:
    bool beginStep(const QPoint &direction);
    void animationTick();
    int stepDurationMs() const;

    MapView *m_source = nullptr;
    QTimer m_animationTimer;
    QElapsedTimer m_stepClock;
    QQueue<QPoint> m_directionQueue;
    int m_x = -1;
    int m_y = -1;
    int m_z = 7;
    qreal m_visualX = -1;
    qreal m_visualY = -1;
    qreal m_fromX = -1;
    qreal m_fromY = -1;
    qreal m_progress = 0;
    int m_walkAnimationTick = 0;
    int m_direction = 2;
    int m_speed = 200;
    int m_lookType = 128;
    bool m_walking = false;
    bool m_noClip = false;
    QString m_lastBlockReason;
};

#endif

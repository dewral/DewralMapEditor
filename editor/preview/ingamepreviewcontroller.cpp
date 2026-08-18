#include "ingamepreviewcontroller.h"

#include "mapview.h"

#include <QtGlobal>

IngamePreviewController::IngamePreviewController(QObject *parent)
    : QObject(parent)
{
    m_animationTimer.setInterval(16);
    m_animationTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_animationTimer, &QTimer::timeout,
            this, &IngamePreviewController::animationTick);
}

void IngamePreviewController::setSource(MapView *source)
{
    if (m_source == source) return;
    m_source = source;
    stop();
    emit sourceChanged();
}

void IngamePreviewController::setSpeed(int speed)
{
    speed = qBound(100, speed, 2000);
    if (m_speed == speed) return;
    m_speed = speed;
    emit speedChanged();
}

void IngamePreviewController::setNoClip(bool enabled)
{
    if (m_noClip == enabled) return;
    m_noClip = enabled;
    emit noClipChanged();
    if (!enabled && positioned()) setPosition(m_x, m_y, m_z);
}

void IngamePreviewController::setLookType(int lookType)
{
    lookType = qMax(1, lookType);
    if (m_lookType == lookType) return;
    m_lookType = lookType;
    emit lookTypeChanged();
}

void IngamePreviewController::setLookHead(int color)
{
    color = qBound(0, color, 132);
    if (m_lookHead == color) return;
    m_lookHead = color;
    emit outfitColorsChanged();
}

void IngamePreviewController::setLookBody(int color)
{
    color = qBound(0, color, 132);
    if (m_lookBody == color) return;
    m_lookBody = color;
    emit outfitColorsChanged();
}

void IngamePreviewController::setLookLegs(int color)
{
    color = qBound(0, color, 132);
    if (m_lookLegs == color) return;
    m_lookLegs = color;
    emit outfitColorsChanged();
}

void IngamePreviewController::setLookFeet(int color)
{
    color = qBound(0, color, 132);
    if (m_lookFeet == color) return;
    m_lookFeet = color;
    emit outfitColorsChanged();
}

void IngamePreviewController::setPosition(int x, int y, int z)
{
    stop();
    m_z = qBound(0, z, 15);
    if (!m_noClip && m_source) {
        const QVector3D resolved = m_source->previewWalkablePositionAt(x, y, m_z);
        if (resolved.z() >= 0) {
            x = qRound(resolved.x());
            y = qRound(resolved.y());
            m_z = qRound(resolved.z());
        }
    }
    m_x = x;
    m_y = y;
    m_visualX = x;
    m_visualY = y;
    emit positionChanged();
    emit visualPositionChanged();
}

void IngamePreviewController::changeFloor(int delta)
{
    if (!positioned()) return;
    const int nextFloor = qBound(0, m_z + delta, 15);
    if (nextFloor == m_z) return;
    stop();
    m_z = nextFloor;
    emit positionChanged();
}

bool IngamePreviewController::walk(int dx, int dy)
{
    if (!positioned() || (qAbs(dx) + qAbs(dy) != 1)) return false;
    const QPoint direction(dx, dy);
    if (m_walking) {
        if (m_directionQueue.size() < 2
            && (m_directionQueue.isEmpty() || m_directionQueue.back() != direction))
            m_directionQueue.enqueue(direction);
        return true;
    }
    return beginStep(direction);
}

bool IngamePreviewController::beginStep(const QPoint &direction)
{
    const int targetX = m_x + direction.x();
    const int targetY = m_y + direction.y();
    if (!m_noClip && (!m_source || !m_source->isPreviewWalkable(targetX, targetY, m_z))) {
        const QString reason = m_source
            ? m_source->previewBlockReasonAt(targetX, targetY, m_z)
            : QStringLiteral("Map view unavailable");
        if (m_lastBlockReason != reason) {
            m_lastBlockReason = reason;
            emit lastBlockReasonChanged();
        }
        emit movementBlocked(targetX, targetY, m_z);
        return false;
    }

    if (!m_lastBlockReason.isEmpty()) {
        m_lastBlockReason.clear();
        emit lastBlockReasonChanged();
    }

    const int newDirection = direction.y() < 0 ? 0 : direction.x() > 0 ? 1
                           : direction.y() > 0 ? 2 : 3;
    if (m_direction != newDirection) {
        m_direction = newDirection;
        emit directionChanged();
    }

    m_fromX = m_visualX;
    m_fromY = m_visualY;
    m_x = targetX;
    m_y = targetY;
    m_progress = 0;
    m_walkAnimationTick = 1;
    m_walking = true;
    m_stepClock.restart();
    m_animationTimer.start();
    emit positionChanged();
    emit walkingChanged();
    return true;
}

void IngamePreviewController::animationTick()
{
    if (!m_walking) return;
    m_progress = qBound<qreal>(0, qreal(m_stepClock.elapsed()) / stepDurationMs(), 1);
    // OTClient advances outfit phases while pixels are being walked. Four
    // phase slots per tile keep old 3-frame and newer outfits deterministic;
    // DatReader wraps this tick to the actual number of DAT phases.
    m_walkAnimationTick = 1 + static_cast<int>(m_progress * 4.0);
    // Smoothstep avoids a mechanical camera snap while preserving exact tile
    // alignment at both ends of the movement.
    const qreal t = m_progress * m_progress * (3.0 - 2.0 * m_progress);
    m_visualX = m_fromX + (m_x - m_fromX) * t;
    m_visualY = m_fromY + (m_y - m_fromY) * t;
    emit visualPositionChanged();

    if (m_progress < 1) return;
    m_visualX = m_x;
    m_visualY = m_y;
    m_progress = 1;
    m_walkAnimationTick = 0;
    m_walking = false;
    m_animationTimer.stop();
    emit visualPositionChanged();
    emit walkingChanged();

    while (!m_directionQueue.isEmpty()) {
        const QPoint next = m_directionQueue.dequeue();
        if (beginStep(next)) break;
    }
}

void IngamePreviewController::stop()
{
    m_animationTimer.stop();
    m_directionQueue.clear();
    if (m_walking) {
        m_visualX = m_x;
        m_visualY = m_y;
        m_progress = 1;
        m_walkAnimationTick = 0;
        m_walking = false;
        emit visualPositionChanged();
        emit walkingChanged();
    }
}

int IngamePreviewController::stepDurationMs() const
{
    return qBound(80, 40000 / qMax(1, m_speed), 400);
}

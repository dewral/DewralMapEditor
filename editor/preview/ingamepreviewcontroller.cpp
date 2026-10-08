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
    if (!positioned() || (qAbs(dx) > 1 || qAbs(dy) > 1 || (dx == 0 && dy == 0))) return false;
    const QPoint direction(dx, dy);
    if (m_walking) {
        m_directionQueue.clear();
        m_directionQueue.enqueue(direction);
        return true;
    }
    return beginStep(direction);
}

bool IngamePreviewController::beginStep(const QPoint &direction)
{
    const int newDirection = direction.y() < 0 ? 0 : direction.x() > 0 ? 1
                           : direction.y() > 0 ? 2 : 3;
    if (m_direction != newDirection) {
        m_direction = newDirection;
        emit directionChanged();
    }

    int targetX = m_x + direction.x();
    int targetY = m_y + direction.y();
    int targetZ = m_z;
    if (!m_noClip && m_source) {
        const QVector3D transition = m_source->previewStepAt(m_x, m_y, m_z, direction.x(), direction.y());
        if (transition.z() >= 0) {
            targetX = qRound(transition.x()); targetY = qRound(transition.y()); targetZ = qRound(transition.z());
        }
    }
    if (!m_noClip && (!m_source || !m_source->isPreviewWalkable(targetX, targetY, targetZ))) {
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

    m_fromX = m_visualX;
    m_fromY = m_visualY;
    m_x = targetX;
    m_y = targetY;
    if (targetZ != m_z) {
        m_z = targetZ; m_visualX = targetX; m_visualY = targetY;
        m_fromX = targetX; m_fromY = targetY;
    }
    m_straightDuration = m_source ? m_source->previewStepDurationAt(m_x, m_y, m_z, m_speed) : 750;
    m_stepDuration = m_source ? m_source->previewStepDurationAt(m_x, m_y, m_z, m_speed,
                        direction.x() != 0 && direction.y() != 0) : m_straightDuration;
    m_walkPhases = m_source ? m_source->previewOutfitWalkPhases(m_lookType) : 0;
    m_progress = 0;
    m_walkAnimationTick = m_walkPhases > 0 ? 1 : 0;
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
    // Creature::updateWalk advances whole pixels at a constant rate.
    const int elapsed = int(m_stepClock.elapsed());
    const int pixels = qBound(0, elapsed * 32 / std::max(1, m_straightDuration + 10), 32);
    const int footDelay = m_walkPhases > 0 ? std::max(20, (m_straightDuration + 20) / m_walkPhases + 10) : 20;
    m_walkAnimationTick = m_walkPhases > 0 ? 1 + elapsed / footDelay : 0;
    const qreal t = pixels / 32.0;
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
    return m_stepDuration;
}

bool IngamePreviewController::useNearbyTransition()
{
    const QPoint directions[] = {QPoint(0, -1), QPoint(1, 0), QPoint(0, 1), QPoint(-1, 0)};
    const QPoint facing = directions[qBound(0, m_direction, 3)];
    return useTransitionAt(m_x + facing.x(), m_y + facing.y());
}

bool IngamePreviewController::useTransitionAt(int x, int y)
{
    if (!m_source || !positioned() || m_walking) return false;
    if (std::abs(x - m_x) > 1 || std::abs(y - m_y) > 1) {
        m_lastBlockReason = QStringLiteral("Move next to the ladder to use it");
        emit lastBlockReasonChanged();
        return false;
    }
    const QVector3D destination = m_source->previewTransitionAt(x, y, m_z, true);
    if (destination.z() >= 0) {
        stop();
        m_x = qRound(destination.x()); m_y = qRound(destination.y()); m_z = qRound(destination.z());
        m_visualX = m_x; m_visualY = m_y;
        m_lastBlockReason.clear();
        emit lastBlockReasonChanged(); emit positionChanged(); emit visualPositionChanged();
        return true;
    }
    m_lastBlockReason = QStringLiteral("No usable stairs or ladder here, or the landing is blocked");
    emit lastBlockReasonChanged();
    return false;
}

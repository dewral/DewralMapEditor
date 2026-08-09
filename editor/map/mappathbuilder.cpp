#include "mappathbuilder.h"

#include <QtMath>
#include <algorithm>

bool MapPathBuilder::configure(const Configuration &configuration, QString *error)
{
    if (configuration.straightPrefab.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Select a straight prefab.");
        return false;
    }
    if (configuration.cornerPrefab.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Select a corner prefab.");
        return false;
    }

    m_configuration = configuration;
    m_configuration.spacing = std::clamp(configuration.spacing, 1, 64);
    m_active = true;
    m_drawing = false;
    m_blocks.clear();
    m_placements.clear();
    return true;
}

void MapPathBuilder::begin(const QPoint &tile)
{
    if (!m_active) return;
    m_origin = tile;
    m_blocks.clear();
    m_blocks.append(QPoint(0, 0));
    m_drawing = true;
    rebuildPlacements();
}

bool MapPathBuilder::append(const QPoint &tile)
{
    if (!m_active || !m_drawing) return false;
    if (!appendBlock(blockForTile(tile))) return false;
    rebuildPlacements();
    return true;
}

void MapPathBuilder::finish()
{
    if (!m_active) return;
    m_drawing = false;
    rebuildPlacements();
}

void MapPathBuilder::clearPath()
{
    m_drawing = false;
    m_blocks.clear();
    m_placements.clear();
}

void MapPathBuilder::cancel()
{
    clearPath();
    m_active = false;
}

QPoint MapPathBuilder::blockForTile(const QPoint &tile) const
{
    const qreal spacing = static_cast<qreal>(m_configuration.spacing);
    return QPoint(qRound((tile.x() - m_origin.x()) / spacing),
                  qRound((tile.y() - m_origin.y()) / spacing));
}

QPoint MapPathBuilder::tileForBlock(const QPoint &block) const
{
    return QPoint(m_origin.x() + block.x() * m_configuration.spacing,
                  m_origin.y() + block.y() * m_configuration.spacing);
}

bool MapPathBuilder::appendBlock(const QPoint &target)
{
    if (m_blocks.isEmpty()) {
        m_blocks.append(target);
        return true;
    }

    QPoint current = m_blocks.constLast();
    bool changed = false;
    while (current != target) {
        const int dx = target.x() - current.x();
        const int dy = target.y() - current.y();
        QPoint next = current;
        if (qAbs(dx) >= qAbs(dy) && dx != 0)
            next.rx() += dx > 0 ? 1 : -1;
        else if (dy != 0)
            next.ry() += dy > 0 ? 1 : -1;

        if (m_blocks.size() >= 2 && next == m_blocks.at(m_blocks.size() - 2)) {
            m_blocks.removeLast();
            changed = true;
        } else if (next != m_blocks.constLast()) {
            m_blocks.append(next);
            changed = true;
        }
        current = next;
    }
    return changed;
}

int MapPathBuilder::directionTo(const QPoint &from, const QPoint &to)
{
    if (to.x() > from.x()) return East;
    if (to.x() < from.x()) return West;
    if (to.y() > from.y()) return South;
    if (to.y() < from.y()) return North;
    return 0;
}

int MapPathBuilder::rotateMaskClockwise(int mask)
{
    int rotated = 0;
    if (mask & North) rotated |= East;
    if (mask & East) rotated |= South;
    if (mask & South) rotated |= West;
    if (mask & West) rotated |= North;
    return rotated;
}

int MapPathBuilder::rotationForMask(int canonicalMask, int targetMask)
{
    int mask = canonicalMask;
    for (int turns = 0; turns < 4; ++turns) {
        if (mask == targetMask) return turns;
        mask = rotateMaskClockwise(mask);
    }
    return 0;
}

void MapPathBuilder::rebuildPlacements()
{
    m_placements.clear();
    if (m_blocks.isEmpty()) return;

    m_placements.reserve(m_blocks.size());
    for (int index = 0; index < m_blocks.size(); ++index) {
        const QPoint block = m_blocks.at(index);
        int mask = 0;
        if (index > 0) mask |= directionTo(block, m_blocks.at(index - 1));
        if (index + 1 < m_blocks.size()) mask |= directionTo(block, m_blocks.at(index + 1));

        Placement placement;
        const QPoint tile = tileForBlock(block);
        placement.x = tile.x();
        placement.y = tile.y();

        const bool endpoint = index == 0 || index + 1 == m_blocks.size();
        if (endpoint && !m_configuration.endPrefab.isEmpty()) {
            placement.prefab = m_configuration.endPrefab;
            placement.quarterTurns = rotationForMask(West, mask == 0 ? West : mask);
        } else if (mask == (North | East) || mask == (East | South)
                   || mask == (South | West) || mask == (West | North)) {
            placement.prefab = m_configuration.cornerPrefab;
            placement.quarterTurns = rotationForMask(West | North, mask);
        } else {
            placement.prefab = m_configuration.straightPrefab;
            const int straightMask = (mask & (North | South)) ? (North | South) : (East | West);
            placement.quarterTurns = rotationForMask(East | West, straightMask);
        }
        m_placements.append(std::move(placement));
    }
}

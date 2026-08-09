#ifndef MAPPATHBUILDER_H
#define MAPPATHBUILDER_H

#include <QPoint>
#include <QString>
#include <QVector>

class MapPathBuilder
{
public:
    struct Configuration {
        QString straightPrefab;
        QString cornerPrefab;
        QString endPrefab;
        int spacing = 1;
    };

    struct Placement {
        int x = 0;
        int y = 0;
        QString prefab;
        int quarterTurns = 0;
    };

    bool configure(const Configuration &configuration, QString *error = nullptr);
    void begin(const QPoint &tile);
    bool append(const QPoint &tile);
    void finish();
    void clearPath();
    void cancel();

    bool active() const { return m_active; }
    bool drawing() const { return m_drawing; }
    int spacing() const { return m_configuration.spacing; }
    const Configuration &configuration() const { return m_configuration; }
    const QVector<Placement> &placements() const { return m_placements; }

private:
    enum Direction : int { North = 1, East = 2, South = 4, West = 8 };

    static int directionTo(const QPoint &from, const QPoint &to);
    static int rotateMaskClockwise(int mask);
    static int rotationForMask(int canonicalMask, int targetMask);
    QPoint blockForTile(const QPoint &tile) const;
    QPoint tileForBlock(const QPoint &block) const;
    bool appendBlock(const QPoint &block);
    void rebuildPlacements();

    Configuration m_configuration;
    bool m_active = false;
    bool m_drawing = false;
    QPoint m_origin;
    QVector<QPoint> m_blocks;
    QVector<Placement> m_placements;
};

#endif

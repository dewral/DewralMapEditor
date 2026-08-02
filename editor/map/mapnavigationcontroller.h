#ifndef MAPNAVIGATIONCONTROLLER_H
#define MAPNAVIGATIONCONTROLLER_H

#include <QElapsedTimer>
#include <QPointF>
#include <QSet>
#include <QtGlobal>

class QTimer;

class MapNavigationController
{
public:
    int &floor() { return m_floor; }
    int floor() const { return m_floor; }
    int &tileSize() { return m_tileSize; }
    int tileSize() const { return m_tileSize; }
    qreal &originX() { return m_originX; }
    qreal originX() const { return m_originX; }
    qreal &originY() { return m_originY; }
    qreal originY() const { return m_originY; }
    QPointF &lastMouse() { return m_lastMouse; }
    const QPointF &lastMouse() const { return m_lastMouse; }
    bool &panning() { return m_panning; }
    bool panning() const { return m_panning; }
    QSet<int> &heldArrows() { return m_heldArrows; }
    const QSet<int> &heldArrows() const { return m_heldArrows; }
    QTimer *&arrowTimer() { return m_arrowTimer; }
    QElapsedTimer &arrowClock() { return m_arrowClock; }

    int &previousCenterX() { return m_previousCenterX; }
    int &previousCenterY() { return m_previousCenterY; }
    int &previousCenterZ() { return m_previousCenterZ; }
    bool &previousCenterValid() { return m_previousCenterValid; }
    bool previousCenterValid() const { return m_previousCenterValid; }

private:
    int m_floor = 7;
    int m_tileSize = 32;
    qreal m_originX = 0;
    qreal m_originY = 0;
    QPointF m_lastMouse;
    bool m_panning = false;
    QSet<int> m_heldArrows;
    QTimer *m_arrowTimer = nullptr;
    QElapsedTimer m_arrowClock;
    int m_previousCenterX = 0;
    int m_previousCenterY = 0;
    int m_previousCenterZ = 0;
    bool m_previousCenterValid = false;
};

#endif

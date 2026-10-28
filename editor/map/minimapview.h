#ifndef MINIMAPVIEW_H
#define MINIMAPVIEW_H

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>
#include "mapview.h"

class MinimapView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MinimapView)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)

    Q_PROPERTY(QPointF centerTile READ centerTile WRITE setCenterTile NOTIFY centerTileChanged)
    Q_PROPERTY(int focusFloor MEMBER m_focusFloor NOTIFY centerTileChanged)
    Q_PROPERTY(int pxPerTile READ pxPerTile WRITE setPxPerTile NOTIFY pxPerTileChanged)

public:
    explicit MinimapView(QQuickItem *parent = nullptr);

    MapView *source() const { return m_source; }
    void setSource(MapView *s);

    int pxPerTile() const { return m_pxPerTile; }
    void setPxPerTile(int v);

    QPointF centerTile() const { return m_centerTile; }
    void setCenterTile(QPointF value) { if(m_centerTile==value)return; m_centerTile=value; emit centerTileChanged(); update(); }
    void paint(QPainter *p) override;

signals:
    void centerTileChanged();
    void tileActivated(int x,int y);
    void sourceChanged();
    void pxPerTileChanged();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void centerMapAt(const QPointF &pos);
    // Repaint only when the minimap bitmap or viewport changes.
    // contentUpdated is also emitted for mouse hover and brush cursor updates;
    // those events do not require CPU rasterization and a texture upload.
    void maybeRepaint();

    MapView *m_source = nullptr;
    QPointF m_centerTile{-1,-1};
    int m_focusFloor=-1;
    MapMinimapService m_previewMinimap;
    quint32 m_previewSourceVersion=0;
    int m_pxPerTile = 1;
    quint32 m_paintedVer = 0;
    // View state captured for the repaint gate.
    double m_lastOx = -1e18, m_lastOy = -1e18;
    int m_lastTs = -1, m_lastFloor = -1;
    double m_lastW = -1, m_lastH = -1;
};

#endif


#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QGuiApplication>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

QPoint MapView::tileAtScreen(const QPointF &p) const
{
    const qreal ts = std::max(1, m_navigationController.tileSize());
    return QPoint(static_cast<int>(std::floor(m_navigationController.originX() + p.x() / ts)),
                  static_cast<int>(std::floor(m_navigationController.originY() + p.y() / ts)));
}

double MapView::glPointerVisualOffsetX() const
{
    if (m_navigationController.heldArrows().isEmpty() || m_hoverX < 0)
        return 0.0;
    const qreal tileSize = std::max(1, m_navigationController.tileSize());
    const qreal exactX = m_navigationController.originX()
                       + m_navigationController.lastMouse().x() / tileSize;
    return (exactX - (static_cast<qreal>(m_hoverX) + 0.5)) * kSprite;
}

double MapView::glPointerVisualOffsetY() const
{
    if (m_navigationController.heldArrows().isEmpty() || m_hoverY < 0)
        return 0.0;
    const qreal tileSize = std::max(1, m_navigationController.tileSize());
    const qreal exactY = m_navigationController.originY()
                       + m_navigationController.lastMouse().y() / tileSize;
    return (exactY - (static_cast<qreal>(m_hoverY) + 0.5)) * kSprite;
}

const OtbmTile *MapView::currentFloorTileAt(int x, int y) const
{

    return m_otbm ? m_otbm->tileAt(x, y, m_navigationController.floor()) : nullptr;
}

void MapView::clearSelection()
{
    if (m_selectionController.selected().isEmpty()) return;
    m_selectionController.selected().clear();
    notifySelectionChanged();
    emit contentUpdated(); update();
}

void MapView::applyRubberBand()
{
    m_selectionController.selected() = m_selectionController.rubberBase();
    const int x0 = std::min(m_selectionController.anchorX(), m_selectionController.rubberX()), x1 = std::max(m_selectionController.anchorX(), m_selectionController.rubberX());
    const int y0 = std::min(m_selectionController.anchorY(), m_selectionController.rubberY()), y1 = std::max(m_selectionController.anchorY(), m_selectionController.rubberY());

    int zBottom = m_navigationController.floor();
    if (m_selectionController.floorMode() == 1)      zBottom = 15;
    else if (m_selectionController.floorMode() == 2) zBottom = renderBottomFloor();

    for (int z = m_navigationController.floor(); z <= zBottom; ++z) {

        const int comp = m_selectionController.compensated() ? (z - m_navigationController.floor()) : 0;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int tx = x - comp, ty = y - comp;
                if (m_otbm && m_otbm->tileAt(tx, ty, z))
                    m_selectionController.selected().insert(selKey(tx, ty, z));
            }
    }
    notifySelectionChanged();
}

void MapView::updateHoverText()
{
    QString t;
    if (m_hoverX >= 0) {
        t = QStringLiteral("%1, %2, %3").arg(m_hoverX).arg(m_hoverY).arg(m_navigationController.floor());
        const OtbmTile *tile = currentFloorTileAt(m_hoverX, m_hoverY);
        if (tile && !tile->items.empty()) {
            const OtbmMapItem &top = tile->items.back();
            const QString name = m_otb ? m_otb->nameForServerId(top.server_id) : QString();
            t += QStringLiteral("  -  %1 item(s)").arg(static_cast<int>(tile->items.size()));
            t += name.isEmpty() ? QStringLiteral(" [id %1]").arg(top.server_id)
                                : QStringLiteral(": %1").arg(name);
        }
    }
    if (t != m_hoverText) {
        m_hoverText = t;
        // Throttle QML binding and layout work while keeping the property current.
        if (!m_hoverEmitTimer->isActive()) m_hoverEmitTimer->start();
    }
}

QVariantList MapView::selectionDetails() const
{
    QVariantList out;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key);
        const int y = selY(key);
        const int z = selZ(key);
        const OtbmTile *tile = m_otbm ? m_otbm->tileAt(x, y, z) : nullptr;

        QVariantMap m;
        m.insert(QStringLiteral("x"), x);
        m.insert(QStringLiteral("y"), y);
        m.insert(QStringLiteral("z"), z);

        QVariantList items;
        if (tile) {
            for (const OtbmMapItem &it : tile->items) {
                QVariantMap im;
                im.insert(QStringLiteral("serverId"), it.server_id);
                im.insert(QStringLiteral("clientId"),
                          m_otb ? m_otb->clientIdForServerId(it.server_id) : 0);
                im.insert(QStringLiteral("name"),
                          m_otb ? m_otb->nameForServerId(it.server_id) : QString());
                im.insert(QStringLiteral("isGround"), it.is_ground);
                items.append(im);
            }
        }
        m.insert(QStringLiteral("items"), items);
        out.append(m);
    }
    return out;
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus();
    m_navigationController.lastMouse() = event->position();

    if (m_selectionController.pasting()) {
        if (event->button() == Qt::LeftButton) {
            const QPoint t = tileAtScreen(event->position());
            commitPasteAt(t.x(), t.y());
            cancelPasting();
        } else if (event->button() == Qt::RightButton) {
            cancelPasting();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && !m_editController.selectionMode()
        && (m_brushController.serverId() > 0 || m_editController.activeZone() != 0 || m_editController.eraseMode()
            || m_brushController.spawnBrush() || !m_brushController.creatureBrush().isEmpty() || m_brushController.houseBrush() > 0)) {

        m_brushController.beginStroke(
            m_editController.eraseMode() || (event->modifiers() & Qt::ControlModifier) != 0);
        const QPoint t = tileAtScreen(event->position());

        m_dragDraw = (event->modifiers() & Qt::ShiftModifier) != 0 && brushCanDrag();
        if (m_dragDraw) {
            m_dragStartX = t.x();
            m_dragStartY = t.y();
            m_hoverX = t.x(); m_hoverY = t.y();
            emit contentUpdated(); update();
            event->accept();
            return;
        }

        if (m_otbm) m_otbm->beginUndoGroup();
        paintAt(t.x(), t.y());
    } else if (event->button() == Qt::LeftButton) {

        const QPoint t = tileAtScreen(event->position());
        const int tx = t.x(), ty = t.y();
        const quint64 k = selKey(tx, ty, m_navigationController.floor());
        const OtbmTile *tile = currentFloorTileAt(tx, ty);

        const bool onItem = tile && (!tile->items.empty() || !tile->creature_name.isEmpty()
                                     || tile->spawn_radius > 0);
        const bool shift = event->modifiers() & Qt::ShiftModifier;
        const bool ctrl = event->modifiers() & Qt::ControlModifier;

        if (shift) {

            m_selectionController.wholeStack() = true;
            m_selectionController.anchorX() = m_selectionController.rubberX() = tx;
            m_selectionController.anchorY() = m_selectionController.rubberY() = ty;
            m_selectionController.selecting() = true;
            if (!ctrl) m_selectionController.selected().clear();
            m_selectionController.rubberBase() = m_selectionController.selected();
            applyRubberBand();
            emit contentUpdated(); update();
        } else if (ctrl) {

            if (onItem) {
                m_selectionController.wholeStack() = false;
                if (m_selectionController.selected().contains(k)) m_selectionController.selected().remove(k);
                else m_selectionController.selected().insert(k);
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else if (!onItem) {

            if (!m_selectionController.selected().isEmpty()) {
                m_selectionController.selected().clear();
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else {

            if (!m_selectionController.selected().contains(k)) {
                m_selectionController.selected().clear();
                m_selectionController.selected().insert(k);
                m_selectionController.wholeStack() = false;
                notifySelectionChanged();
            }
            m_selectionController.moving() = true;
            m_selectionController.moveChanged() = false;
            m_selectionController.moveSourceX() = tx;
            m_selectionController.moveSourceY() = ty;
            m_selectionController.moveSourceZ() = m_navigationController.floor();

            m_selectionController.moveServerId() = tile->items.empty() ? 0 : tile->items.back().server_id;
            emit contentUpdated(); update();
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_navigationController.panning() = true;

        if (m_hoverX != -1) { m_hoverX = m_hoverY = -1; updateHoverText(); }
    } else if (event->button() == Qt::RightButton && !m_editController.selectionMode()
               && (m_brushController.serverId() > 0 || m_editController.activeZone() != 0 || m_editController.eraseMode()
                   || m_brushController.spawnBrush() || !m_brushController.creatureBrush().isEmpty() || m_brushController.houseBrush() > 0)) {

        if (m_editController.activeZone() != 0) setActiveZone(0);
        else if (m_brushController.spawnBrush()) setSpawnBrush(false);
        else if (!m_brushController.creatureBrush().isEmpty()) setCreatureBrush(QString());
        else if (m_brushController.houseBrush() > 0) { setHouseExitMode(false); setHouseBrush(0); }
        else setBrushServerId(0);
    } else if (event->button() == Qt::RightButton) {

        const QPoint t = tileAtScreen(event->position());
        m_itemController.contextX() = t.x();
        m_itemController.contextY() = t.y();
        const OtbmTile *contextTile = currentFloorTileAt(t.x(), t.y());
        m_itemController.contextItemIndex() = contextTile && !contextTile->items.empty()
                                 ? static_cast<int>(contextTile->items.size()) - 1
                                 : -1;
        const quint64 k = selKey(t.x(), t.y(), m_navigationController.floor());
        if (contextTile) {
            if (!m_selectionController.selected().contains(k)) {
                m_selectionController.selected().clear();
                m_selectionController.selected().insert(k);
                notifySelectionChanged();
                emit contentUpdated(); update();
            }
        } else if (!m_selectionController.selected().isEmpty()) {
            m_selectionController.selected().clear();
            notifySelectionChanged();
            emit contentUpdated(); update();
        }
        emit contextMenuRequested(event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();

    if (m_navigationController.panning()) {

        const QPointF delta = pos - m_navigationController.lastMouse();
        const qreal ts = std::max(1, m_navigationController.tileSize());
        m_navigationController.originX() -= delta.x() / ts;
        m_navigationController.originY() -= delta.y() / ts;
        m_navigationController.lastMouse() = pos;
        emit contentUpdated(); update();
        event->accept();
        return;
    }

    m_navigationController.lastMouse() = pos;

    if (m_selectionController.moving()) {

        const QPoint t = tileAtScreen(pos);
        if (t.x() != m_selectionController.moveSourceX() || t.y() != m_selectionController.moveSourceY()) m_selectionController.moveChanged() = true;
        if (t.x() != m_hoverX || t.y() != m_hoverY) {
            m_hoverX = t.x(); m_hoverY = t.y(); updateHoverText(); emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    if (m_brushController.painting()) {
        const QPoint t = tileAtScreen(pos);

        if (!m_dragDraw) paintAt(t.x(), t.y());

        if (t.x() != m_hoverX || t.y() != m_hoverY) {
            m_hoverX = t.x(); m_hoverY = t.y(); updateHoverText();
            if (m_dragDraw) { emit contentUpdated(); update(); }
        }
        event->accept();
        return;
    }

    if (m_selectionController.selecting()) {
        const QPoint t = tileAtScreen(pos);
        if (t.x() != m_selectionController.rubberX() || t.y() != m_selectionController.rubberY()) {
            m_selectionController.rubberX() = t.x();
            m_selectionController.rubberY() = t.y();
            applyRubberBand();
            emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    const QPoint h = tileAtScreen(pos);
    if (h.x() != m_hoverX || h.y() != m_hoverY) {
        m_hoverX = h.x();
        m_hoverY = h.y();
        updateHoverText();
        if (!m_editController.selectionMode() || m_selectionController.pasting()) {
            emit contentUpdated();
            update();
        }
    }
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_brushController.painting()) {
        m_brushController.setPainting(false);
        if (m_dragDraw) {

            const QPoint t = tileAtScreen(event->position());
            drawDragRect(m_dragStartX, m_dragStartY, t.x(), t.y());
            m_dragDraw = false;
            emit contentUpdated(); update();
        } else if (m_otbm) {
            m_otbm->endUndoGroup();
            emit contentUpdated();
            update();
        }
    } else if (event->button() == Qt::LeftButton && m_selectionController.moving()) {
        m_selectionController.moving() = false;
        const QPoint t = tileAtScreen(event->position());
        const int dx = t.x() - m_selectionController.moveSourceX();
        const int dy = t.y() - m_selectionController.moveSourceY();
        const int dz = m_navigationController.floor() - m_selectionController.moveSourceZ();
        if (m_selectionController.moveChanged() && (dx != 0 || dy != 0 || dz != 0))
            moveSelection(dx, dy, dz);

        emit contentUpdated(); update();
    } else if (event->button() == Qt::LeftButton && m_selectionController.selecting()) {
        m_selectionController.selecting() = false;
        emit contentUpdated(); update();
    } else if (event->button() == Qt::MiddleButton && m_navigationController.panning()) {
        m_navigationController.panning() = false;
    }
    event->accept();
}

void MapView::mouseUngrabEvent()
{
    // Always close an active stroke so focus loss cannot leave an undo group open.
    if (m_brushController.painting() && !m_dragDraw && m_otbm)
        m_otbm->endUndoGroup();

    m_brushController.setPainting(false);
    m_dragDraw = false;
    m_selectionController.moving() = false;
    m_selectionController.moveChanged() = false;
    m_selectionController.selecting() = false;
    m_navigationController.panning() = false;
    m_brushController.finishStroke();

    emit contentUpdated();
    update();
    QQuickItem::mouseUngrabEvent();
}

void MapView::hoverMoveEvent(QHoverEvent *event)
{
    if (m_navigationController.panning() || m_selectionController.selecting()) return;
    m_navigationController.lastMouse() = event->position();
    const QPoint h = tileAtScreen(event->position());
    if (h.x() != m_hoverX || h.y() != m_hoverY) {
        m_hoverX = h.x();
        m_hoverY = h.y();
        updateHoverText();
        // Redraw only when hover has a visual representation on the map.
        const bool cursorVisual = m_brushController.serverId() > 0 || m_editController.activeZone() != 0
            || m_editController.eraseMode() || m_brushController.spawnBrush() || !m_brushController.creatureBrush().isEmpty()
            || m_brushController.houseBrush() > 0 || m_brushController.houseExitMode() || !m_brushController.doodadBrush().isEmpty();
        if (m_selectionController.pasting() || (!m_editController.selectionMode() && cursorVisual)) {
            emit contentUpdated();
        }
    }
    event->accept();
}

void MapView::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        event->ignore();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        setFloor(m_navigationController.floor() - steps);
    } else if (event->modifiers() & Qt::AltModifier) {

        static constexpr int kSizes[] = {0, 1, 2, 4, 6, 8, 11};
        int idx = 0;
        for (int i = 0; i < 7; ++i)
            if (kSizes[i] == m_brushController.size()) { idx = i; break; }
        idx = std::clamp(idx + (steps > 0 ? 1 : -1), 0, 6);
        setBrushSize(kSizes[idx]);
    } else {
        zoomAt(steps, event->position().x(), event->position().y());
    }
    event->accept();
}

void MapView::zoomAt(int steps, qreal px, qreal py)
{
    if (steps == 0) return;
    const qreal ts = std::max(1, m_navigationController.tileSize());
    const qreal worldX = m_navigationController.originX() + px / ts;
    const qreal worldY = m_navigationController.originY() + py / ts;

    int newSize = static_cast<int>(std::lround(m_navigationController.tileSize() * std::pow(1.2, steps)));
    if (newSize == m_navigationController.tileSize()) newSize += (steps > 0 ? 1 : -1);
    newSize = std::clamp(newSize, 1, 256);
    if (newSize != m_navigationController.tileSize()) {
        m_navigationController.tileSize() = newSize;
        emit tileSizeChanged();
        m_navigationController.originX() = worldX - px / newSize;
        m_navigationController.originY() = worldY - py / newSize;
        emit contentUpdated(); update();
    }
}

void MapView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        setFloor(m_navigationController.floor() - 1);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        setFloor(m_navigationController.floor() + 1);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Q) {
        setShowShade(!m_showShade);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
        setShowLowerFloors(!m_showLowerFloors);
        event->accept();
        return;
    }

    if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_F) {
            setShowCreatures(!m_showCreatures);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_S) {
            setShowSpawns(!m_showSpawns);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_E) {
            setShowZones(!m_showZones);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_O) {
            setShowPathing(!m_showPathing);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_L) {
            setShowAnimations(!m_showAnimations);
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_M) {
            setMinimapOn(!m_minimapOn);
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Space && !(event->modifiers() & Qt::ControlModifier)) {
        toggleSelectionMode();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_R && !m_brushController.doodadBrush().isEmpty() && m_brushController.store()) {
        const int cnt = m_brushController.store()->doodadVariantCount(m_brushController.doodadBrush());
        if (cnt > 1) {
            m_brushController.doodadVariant() = (m_brushController.doodadVariant() + 1) % cnt;
            emit contentUpdated(); update();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete) {
        if (!m_selectionController.selected().isEmpty()) deleteSelectedTop();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_selectionController.pasting()) {
        cancelPasting();
        event->accept();
        return;
    }

    const int k = event->key();
    if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
        if (!event->isAutoRepeat() && !m_navigationController.heldArrows().contains(k)) {
            m_navigationController.heldArrows().insert(k);
            if (!m_navigationController.arrowTimer()) {
                m_navigationController.arrowTimer() = new QTimer(this);
                m_navigationController.arrowTimer()->setTimerType(Qt::CoarseTimer);
                m_navigationController.arrowTimer()->setInterval(16);
                connect(m_navigationController.arrowTimer(), &QTimer::timeout, this, [this] {
                    const double dt = m_navigationController.arrowClock().nsecsElapsed() / 1e9;
                    m_navigationController.arrowClock().restart();

                    const bool fast = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
                    const double speed = (fast ? 60.0 : 25.0) * dt;
                    const int dx = static_cast<int>(m_navigationController.heldArrows().contains(Qt::Key_Right))
                                 - static_cast<int>(m_navigationController.heldArrows().contains(Qt::Key_Left));
                    const int dy = static_cast<int>(m_navigationController.heldArrows().contains(Qt::Key_Down))
                                 - static_cast<int>(m_navigationController.heldArrows().contains(Qt::Key_Up));
                    if (dx == 0 && dy == 0) return;
                    m_navigationController.originX() += dx * speed;
                    m_navigationController.originY() += dy * speed;

                    // Moving the camera changes the world tile below a stationary
                    // mouse pointer. Keep brush, paste and selection previews bound
                    // to the cursor while keyboard navigation is active.
                    const QPointF mouse = m_navigationController.lastMouse();
                    if (mouse.x() >= 0 && mouse.y() >= 0
                        && mouse.x() < width() && mouse.y() < height()) {
                        const QPoint hover = tileAtScreen(mouse);
                        if (hover.x() != m_hoverX || hover.y() != m_hoverY) {
                            m_hoverX = hover.x();
                            m_hoverY = hover.y();
                            updateHoverText();
                        }
                    }

                    emit contentUpdated();
                });
            }
            if (!m_navigationController.arrowTimer()->isActive()) {
                m_navigationController.arrowClock().restart();
                m_navigationController.arrowTimer()->start();
            }
        }
        event->accept();
        return;
    }
    QQuickItem::keyPressEvent(event);
}

void MapView::keyReleaseEvent(QKeyEvent *event)
{
    const int k = event->key();
    if (!event->isAutoRepeat() && m_navigationController.heldArrows().remove(k)) {
        if (m_navigationController.heldArrows().isEmpty() && m_navigationController.arrowTimer()) {
            m_navigationController.arrowTimer()->stop();

            const QPointF mouse = m_navigationController.lastMouse();
            if (mouse.x() >= 0 && mouse.y() >= 0
                && mouse.x() < width() && mouse.y() < height()) {
                const QPoint hover = tileAtScreen(mouse);
                if (hover.x() != m_hoverX || hover.y() != m_hoverY) {
                    m_hoverX = hover.x();
                    m_hoverY = hover.y();
                    updateHoverText();
                    emit contentUpdated();
                }
            }
        }
        event->accept();
        return;
    }
    QQuickItem::keyReleaseEvent(event);
}

void MapView::focusOutEvent(QFocusEvent *event)
{

    m_navigationController.heldArrows().clear();
    if (m_navigationController.arrowTimer()) m_navigationController.arrowTimer()->stop();
    QQuickItem::focusOutEvent(event);
}

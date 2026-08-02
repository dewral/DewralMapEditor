#ifndef MAPSELECTIONCONTROLLER_H
#define MAPSELECTIONCONTROLLER_H

#include "otbmreader.h"

#include <QSet>
#include <QString>
#include <QtGlobal>
#include <vector>

struct MapClipboardTile {
    int dx = 0;
    int dy = 0;
    int dz = 0;
    std::vector<OtbmMapItem> items;
    QString creature;
    int spawntime = 60;
    bool npc = false;
    int spawnRadius = 0;
};

class MapSelectionController
{
public:
    QSet<quint64> &selected() { return m_selected; }
    const QSet<quint64> &selected() const { return m_selected; }
    QSet<quint64> &selectedChunks() { return m_selectedChunks; }
    const QSet<quint64> &selectedChunks() const { return m_selectedChunks; }
    QSet<quint64> &rubberBase() { return m_rubberBase; }
    const QSet<quint64> &rubberBase() const { return m_rubberBase; }
    std::vector<MapClipboardTile> &clipboard() { return m_clipboard; }
    const std::vector<MapClipboardTile> &clipboard() const { return m_clipboard; }

    int &floorMode() { return m_floorMode; }
    int floorMode() const { return m_floorMode; }
    bool &compensated() { return m_compensated; }
    bool compensated() const { return m_compensated; }
    bool &wholeStack() { return m_wholeStack; }
    bool wholeStack() const { return m_wholeStack; }
    bool &selecting() { return m_selecting; }
    bool selecting() const { return m_selecting; }
    bool &pasting() { return m_pasting; }
    bool pasting() const { return m_pasting; }
    int &anchorX() { return m_anchorX; }
    int anchorX() const { return m_anchorX; }
    int &anchorY() { return m_anchorY; }
    int anchorY() const { return m_anchorY; }
    int &rubberX() { return m_rubberX; }
    int rubberX() const { return m_rubberX; }
    int &rubberY() { return m_rubberY; }
    int rubberY() const { return m_rubberY; }
    bool &moving() { return m_moving; }
    bool moving() const { return m_moving; }
    bool &moveChanged() { return m_moveChanged; }
    bool moveChanged() const { return m_moveChanged; }
    int &moveSourceX() { return m_moveSourceX; }
    int moveSourceX() const { return m_moveSourceX; }
    int &moveSourceY() { return m_moveSourceY; }
    int moveSourceY() const { return m_moveSourceY; }
    int &moveSourceZ() { return m_moveSourceZ; }
    int moveSourceZ() const { return m_moveSourceZ; }
    int &moveServerId() { return m_moveServerId; }
    int moveServerId() const { return m_moveServerId; }

private:
    QSet<quint64> m_selected;
    QSet<quint64> m_selectedChunks;
    QSet<quint64> m_rubberBase;
    std::vector<MapClipboardTile> m_clipboard;
    int m_floorMode = 0;
    bool m_compensated = true;
    bool m_wholeStack = false;
    bool m_selecting = false;
    bool m_pasting = false;
    int m_anchorX = 0;
    int m_anchorY = 0;
    int m_rubberX = 0;
    int m_rubberY = 0;
    bool m_moving = false;
    bool m_moveChanged = false;
    int m_moveSourceX = 0;
    int m_moveSourceY = 0;
    int m_moveSourceZ = 0;
    int m_moveServerId = 0;
};

#endif

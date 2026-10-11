#ifndef MAPITEMCONTROLLER_H
#define MAPITEMCONTROLLER_H

class MapItemController
{
public:
    int &contextX() { return m_contextX; }
    int contextX() const { return m_contextX; }
    int &contextY() { return m_contextY; }
    int contextY() const { return m_contextY; }
    int &contextItemIndex() { return m_contextItemIndex; }
    int contextItemIndex() const { return m_contextItemIndex; }
    bool &menuPending() { return m_menuPending; }
    bool &contextTargetsItem() { return m_contextTargetsItem; }
    bool contextTargetsItem() const { return m_contextTargetsItem; }
    void setContext(int x, int y, int itemIndex)
    {
        m_contextX = x;
        m_contextY = y;
        m_contextItemIndex = itemIndex;
        m_contextTargetsItem = true;
    }

private:
    int m_contextX = 0;
    int m_contextY = 0;
    int m_contextItemIndex = -1;
    bool m_menuPending = false;
    bool m_contextTargetsItem = true;
};

#endif

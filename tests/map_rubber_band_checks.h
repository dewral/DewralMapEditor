#pragma once
#include <QMouseEvent>

static bool testRubberBandRegions()
{
    const auto key=[](int x,int y,int z) { return quint64(x)|(quint64(y)<<24)|(quint64(z)<<48); };
    OtbmReader map;
    if (!map.newMap(65535,65535,1098,3,57)) return false;
    map.setUndoLimit(0);
    int id=100;
    for (int z=7;z<=15;++z) {
        const int side=z==7?65:8;
        const int sparse[]={0,1,30,31,32,33,63,64};
        for (int iy=0;iy<side;++iy) for (int ix=0;ix<side;++ix) {
            const int x=z==7?ix:sparse[ix],y=z==7?iy:sparse[iy];
            if (!map.addItem(x,y,z,static_cast<uint16_t>(id++))) return false;
        }
        if (!map.addItem(65535,65535,z,static_cast<uint16_t>(id++))) return false;
    }
    map.setUndoLimit(500);
    if (!map.placeItem(513,513,7,60000,0,false,true)) return false;
    class View : public MapView {
    public:
        using MapView::mousePressEvent;
        using MapView::mouseMoveEvent;
        using MapView::mouseReleaseEvent;
    } view;
    view.setOtbm(&map); view.setSize({320,320}); view.setTileSize(1); view.setSelectionMode(true);
    const auto drag=[&](QPoint a,QPoint b,bool add) {
        const auto modifiers=Qt::ShiftModifier|(add?Qt::ControlModifier:Qt::NoModifier);
        const QPointF first(a.x()+0.25,a.y()+0.25),last(b.x()+0.25,b.y()+0.25);
        QMouseEvent press(QEvent::MouseButtonPress,first,first,Qt::LeftButton,Qt::LeftButton,modifiers);
        QMouseEvent move(QEvent::MouseMove,last,last,Qt::NoButton,Qt::LeftButton,modifiers);
        QMouseEvent release(QEvent::MouseButtonRelease,last,last,Qt::LeftButton,Qt::NoButton,modifiers);
        view.mousePressEvent(&press);view.mouseMoveEvent(&move);view.advancePointerFrame();view.mouseReleaseEvent(&release);
    };
    const auto reference=[&](QPoint a,QPoint b,QSet<quint64> base) {
        const int bottom=view.selectionFloors()==1?15:view.selectionFloors()==2?view.renderBottomFloor():view.floor();
        for (const auto &tile:map.tiles()) {
            if (tile.z<view.floor()||tile.z>bottom) continue;
            const int comp=view.compensatedSelect()?int(tile.z)-view.floor():0;
            // Independent projected-position oracle, without chunk lookups.
            const int x=int(tile.x)+comp,y=int(tile.y)+comp;
            if (x>=std::min(a.x(),b.x())&&x<=std::max(a.x(),b.x())
                &&y>=std::min(a.y(),b.y())&&y<=std::max(a.y(),b.y()))
                base.insert(key(tile.x,tile.y,tile.z));
        }
        return base;
    };
    const auto equal=[&](const QSet<quint64> &positions) {
        QSet<int> expected;
        for (const auto &tile:map.tiles()) if (positions.contains(key(tile.x,tile.y,tile.z)))
            for (const auto &item:tile.items) expected.insert(item.server_id);
        QSet<int> actual;for (const auto &item:view.selectionItemIds()) actual.insert(item.toInt());
        return view.selectionCount()==positions.size()&&actual==expected;
    };
    const std::vector<std::pair<QPoint,QPoint>> regions={
        {{0,0},{3,3}},{{30,30},{33,33}},{{64,64},{31,31}},{{-5,-5},{2,2}},
        {{-20,-20},{-10,-10}},{{65534,65534},{65538,65538}},{{0,0},{65535,65535}},
        {{500,500},{520,520}}};
    for (int floor:{7,9,15}) for (int mode:{0,1,2}) for (bool compensate:{false,true}) {
        view.centerOnPosition(160,160,floor);view.setSelectionFloors(mode);view.setCompensatedSelect(compensate);
        for (const auto &region:regions) {
            view.clearSelection();drag(region.first,region.second,false);
            auto expected=reference(region.first,region.second,{});
            if (!require(equal(expected),"Spatial rubber-band result differs from projected-position oracle")) return false;
            const QPoint a(31,31),b(35,35);
            drag(a,b,true);expected=reference(a,b,expected);
            if (!require(equal(expected),"Ctrl+rubber-band lost existing selection or compensation")) return false;
        }
    }
    view.centerOnPosition(160,160,7);view.setSelectionFloors(0);view.setCompensatedSelect(false);
    view.undo();view.clearSelection();drag({500,500},{520,520},false);
    if (!require(equal(reference({500,500},{520,520},{})),"Rubber-band retained a tile removed by undo")) return false;
    view.redo();view.clearSelection();drag({500,500},{520,520},false);
    if (!require(equal(reference({500,500},{520,520},{})),"Rubber-band missed a tile restored by redo")) return false;
    // Direct reader edits do not update MapView's render index. Selection must
    // use the authoritative index, even when structural edits preserve the count.
    if (!map.placeItem(514,514,8,60001,0,false,true) || !map.undo()
        || !map.placeItem(515,515,8,60002,0,false,true)) return false;
    view.setSelectionFloors(1);view.clearSelection();drag({500,500},{520,520},false);
    return require(equal(reference({500,500},{520,520},{})),"Rubber-band missed direct reader edits");
}

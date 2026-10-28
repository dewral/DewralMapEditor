// Exercise the production MapView pointer path without a visible window/GPU.
#include "mapview.h"
#include <QGuiApplication>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <iostream>
class AuditView : public MapView {
public:
    using MapView::mousePressEvent;
    using MapView::mouseMoveEvent;
    using MapView::mouseReleaseEvent;
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    OtbmReader map;
    if (!map.newMap(4096,4096,1098,3,57)) return 1;
    for (int z=7;z<=15;++z)
        for (int i=0;i<2;++i)
            if (!map.addItem(1050+i*10,1470+i*10,z,100)) return 2;
    AuditView view;
    view.setOtbm(&map); view.setSize({1920,1080}); view.setTileSize(1);
    view.centerOnPosition(2000,2000,7); view.setSelectionMode(true);
    view.setCompensatedSelect(false);
    std::cout<<"width,height,floors,mapTiles,selected,run,moveMs\n";
    for (int floors : {1,9}) {
        view.setSelectionFloors(floors==1?0:1);
        for (const auto &size : {QSize(64,64),QSize(256,256),QSize(1024,512),QSize(1900,1000)}) {
            for (int run=0;run<=3;++run) {
                view.clearSelection();
                const QPointF first(1.1,1.1),last(size.width()+0.1,size.height()+0.1);
                QMouseEvent press(QEvent::MouseButtonPress,first,first,Qt::LeftButton,Qt::LeftButton,Qt::ShiftModifier);
                view.mousePressEvent(&press);
                QMouseEvent move(QEvent::MouseMove,last,last,Qt::NoButton,Qt::LeftButton,Qt::ShiftModifier);
                QElapsedTimer timer; timer.start();
                view.mouseMoveEvent(&move);
                if (!view.advancePointerFrame()) return 3;
                const double ms=timer.nsecsElapsed()/1000000.0;
                const int selected=view.selectionCount();
                QMouseEvent release(QEvent::MouseButtonRelease,last,last,Qt::LeftButton,Qt::NoButton,Qt::ShiftModifier);
                view.mouseReleaseEvent(&release);
                if (selected!=2*floors) { std::cerr<<"Wrong selection: "<<selected<<'\n'; return 4; }
                if (run) std::cout<<size.width()<<','<<size.height()<<','<<floors<<','<<map.tileCount()
                                 <<','<<selected<<','<<run<<','<<ms<<'\n';
            }
        }
    }
    return 0;
}

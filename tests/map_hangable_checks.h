#pragma once

#include <QMouseEvent>

bool testHangableRendering(const QTemporaryDir &directory)
{
    QByteArray dat, otb("OTBI"), spr;
    appendU32(dat, 0x12345678); appendU16(dat, 104); dat.append(QByteArray(6, '\0'));
    otb.append(char(0xfe)); otb.append(QByteArray(5, '\0'));
    for (int i = 0; i < 5; ++i) {
        const int flag = i == 0 ? 18 : i == 1 ? 19 : i == 2 ? 17 : i == 3 ? 0 : 10;
        dat.append(char(flag));
        if (flag == 0) appendU16(dat, 100);
        dat.append(char(0xff));
        const int patterns = i == 2 || i == 4 ? 3 : 1;
        dat.append(QByteArray(3, '\1')); dat.append(char(patterns)); dat.append(QByteArray(3, '\1'));
        for (int sprite = 1; sprite <= patterns; ++sprite) appendU16(dat, sprite);

        QByteArray item(1, char(i == 3 ? 1 : i == 4 ? 12 : 0));
        appendU32(item, 0);
        item.append(char(0x10)); appendU16(item, 2); appendU16(item, i == 4 ? 2006 : 1000 + i);
        item.append(char(0x11)); appendU16(item, 2); appendU16(item, 100 + i);
        otb.append(char(0xfe));
        for (char byte : item) {
            if (quint8(byte) >= 0xfd) otb.append(char(0xfd));
            otb.append(byte);
        }
        otb.append(char(0xff));
    }
    otb.append(char(0xff));
    appendU32(spr, 0x12345678); appendU16(spr, 3);
    for (quint32 offset : {18u, 30u, 42u}) appendU32(spr, offset);
    for (const QColor &color : {red, green, blue}) {
        spr.append(QByteArray(3, '\0')); appendU16(spr, 7); appendU16(spr, 0); appendU16(spr, 1);
        spr.append(char(color.red())); spr.append(char(color.green())); spr.append(char(color.blue()));
    }
    const QString base = directory.filePath("hangable");
    DatReader client; client.setClientVersion(860);
    OtbReader items;
    SprReader sprites;
    if (!require(writeFile(base + ".dat", dat) && writeFile(base + ".otb", otb)
                 && writeFile(base + ".spr", spr) && client.loadFile(base + ".dat")
                 && items.loadFile(base + ".otb") && sprites.loadFile(base + ".spr"),
                 "Could not create hook fixtures")) return false;

    class HookView : public MapView {
    public:
        using MapView::mousePressEvent;
        using MapView::mouseReleaseEvent;
        using MapView::mouseMoveEvent;
    };
    for (int wall : {1000, 1001, 1003}) {
        OtbmReader document;
        document.newMap(512, 512, 860, 2, 22);
        for (int x : {99, 100, 101}) {
            document.placeItem(x, 100, 7, wall, 0, false, false);
            document.placeItem(x, 100, 7, 1002, 1, false, false);
        }
        HookView view;
        view.setOtb(&items); view.setDat(&client); view.setSpr(&sprites); view.setOtbm(&document);
        const QColor expected = wall == 1000 ? green : wall == 1001 ? blue : red;
        const quint64 key = (quint64(3) << 32) | 3;
        if (!require(waitUntil([&] {
            std::vector<float> instances;
            if (view.renderChunkVersion(7, key) == MapView::kChunkPending) view.renderRequestChunk(7, key);
            if (view.atlasBuilding() || view.renderCollectChunkInstances(7, key, false, instances)
                == MapView::kChunkPending || instances.size() != 36) return false;
            const QImage image = view.renderAtlasUpload(0, 0).image;
            if (image.isNull()) return false;
            for (int index : {1, 3, 5})
                if (image.pixelColor(int(instances[index * 6 + 2]), int(instances[index * 6 + 3])) != expected)
                    return false;
            return true;
        }), "Hangables used tile coordinates instead of the wall hook")) return false;

        // Paint through the real MapView path: a new vial must start empty.
        view.placeItemAt(110, 100, 2006);
        const auto *fluid = document.tileAt(110, 100, 7);
        if (!require(fluid && fluid->items.back().count == 0
                     && fluid->items.back().has_subtype_attribute,
                     "MapView painted a vial with inconsistent defaults")) return false;

        document.setCreatureAt(101, 100, 7, "Guide", 60, true, 2);
        view.setSize({320, 320}); view.setTileSize(32);
        const auto click = [&](Qt::MouseButton button, int x) {
            view.centerOnPosition(x, 100, 7);
            const QPointF point((x + .5 - view.renderOriginX()) * view.tileSize(),
                                (100.5 - view.renderOriginY()) * view.tileSize());
            QMouseEvent press(QEvent::MouseButtonPress, point, point, button, button, Qt::NoModifier);
            view.mousePressEvent(&press);
            QMouseEvent release(QEvent::MouseButtonRelease, point, point, button, Qt::NoButton, Qt::NoModifier);
            view.mouseReleaseEvent(&release);
        };
        click(Qt::RightButton, 101);
        view.copySelection(); view.startPasting(); click(Qt::LeftButton, 120);
        const auto *pasted = document.tileAt(120, 100, 7);
        if (!require(pasted && pasted->creature_name == QStringLiteral("Guide")
                     && pasted->creature_direction == 2,
                     "Copy/paste lost creature direction")) return false;
        view.cancelPasting();

        // Drag only the decoration from this wall onto an East-hook wall.
        document.placeItem(104, 100, 7, 1001, 0, false, false);
        click(Qt::RightButton, 100);
        const auto pointAt = [&](int x) {
            return QPointF((x + .5 - view.renderOriginX()) * view.tileSize(),
                           (100.5 - view.renderOriginY()) * view.tileSize());
        };
        QMouseEvent press(QEvent::MouseButtonPress, pointAt(100), pointAt(100),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        view.mousePressEvent(&press);
        QMouseEvent move(QEvent::MouseMove, pointAt(104), pointAt(104),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        view.mouseMoveEvent(&move);
        view.advancePointerFrame();
        std::vector<float> ghost;
        view.renderCollectGhostInstances(ghost);
        const QImage atlas = view.renderAtlasUpload(0, 0).image;
        if (!require(ghost.size() == 4
                     && atlas.pixelColor(int(ghost[2]), int(ghost[3])) == blue,
                     "Drag preview used the source wall hook")) return false;
        QMouseEvent release(QEvent::MouseButtonRelease, pointAt(104), pointAt(104),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        view.mouseReleaseEvent(&release);
    }
    return true;
}

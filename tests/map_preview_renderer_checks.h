#pragma once
#include "../editor/preview/ingamepreviewscene.h"
#include "../editor/preview/ingamepreviewview.h"
#include <QQuickWindow>

static bool previewItems(const QTemporaryDir &directory, OtbReader &otb, const QVector<QPair<int, int>> &ids)
{
    QByteArray bytes("OTBI");
    bytes.append(char(0xfe));
    bytes.append(QByteArray(5, '\0'));
    for (const auto &id : ids)
    {
        QByteArray node(5, '\0');
        node.append(char(0x10));
        appendU16(node, 2);
        appendU16(node, id.first);
        node.append(char(0x11));
        appendU16(node, 2);
        appendU16(node, id.second);
        bytes.append(char(0xfe));
        for (char byte : node)
        {
            if (uchar(byte) >= 0xfd)
                bytes.append(char(0xfd));
            bytes.append(byte);
        }
        bytes.append(char(0xff));
    }
    bytes.append(char(0xff));
    return writeFile(directory.filePath("preview.otb"), bytes) &&
           otb.loadFile(directory.filePath("preview.otb"));
}
static bool testClientPreviewComposition(const QTemporaryDir &directory, DatReader &dat, SprReader &spr)
{
    auto *ground = const_cast<ClientItem *>(dat.itemByClientId(100));
    auto *common = const_cast<ClientItem *>(dat.itemByClientId(101));
    auto *top = const_cast<ClientItem *>(dat.itemByClientId(102));
    const auto savedGround = *ground, savedCommon = *common, savedTop = *top;
    struct Restore
    {
        ClientItem *a, *b, *c;
        ClientItem aa, bb, cc;
        ~Restore()
        {
            *a = aa;
            *b = bb;
            *c = cc;
        }
    } restore{ground, common, top, savedGround, savedCommon, savedTop};
    ground->properties = 0;
    ground->enable(ClientProperty::Ground);
    ground->frames = 1;
    common->properties = 0;
    common->enable(ClientProperty::Elevation);
    common->elevation = 40;
    top->properties = 0;
    top->enable(ClientProperty::Top);
    top->elevation = 0;
    OtbReader otb;
    if (!previewItems(directory, otb, {{1000, 100}, {1001, 101}, {1002, 102}}))
        return false;
    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    map.placeItem(100, 100, 7, 1000, 0, false, true);
    map.placeItem(100, 100, 7, 1001, 1, false, false);
    map.placeItem(100, 100, 7, 1002, 2, false, false);
    MapView view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setOtbm(&map);
    IngamePreviewState state;
    state.x = 100;
    state.y = 100;
    state.visualX = 100;
    state.visualY = 100;
    state.lookType = 1;
    state.lighting = false;
    state.name = "Dewral";
    IngamePreviewScene scene;
    auto frame = scene.build(&view, state);
    if (!require(frame.draws.size() == 4, "Client preview did not stage ground/common/actor/top passes"))
        return false;
    if (!require(frame.draws[2].position == QPointF(200, 136) && frame.draws[3].position == QPointF(224, 160),
                 "Elevation cap or always-on-top displacement differs from the client"))
        return false;
    if (!require(frame.labels.size() == 1 && frame.labels[0].toMap()["x"].toDouble() == 216 &&
                     frame.labels[0].toMap()["y"].toDouble() == 136,
                 "Information anchor did not follow the raised actor"))
        return false;
    if (!require(frame.referenceImage().pixelColor(224, 160) == blue,
                 "Always-on-top item was not drawn after the actor"))
        return false;
    state.showItems=false;
    const auto groundOnly=scene.build(&view,state);
    if (!require(groundOnly.draws.size()==2 && groundOnly.labels.size()==1,
                 "Preview item visibility must retain ground and the local player")) return false;
    state.showItems=true;
    if (!require(scene.build(&view,state).draws.size()==4,
                 "Preview items must return when their visibility is restored")) return false;
    common->enable(ClientProperty::Light);
    common->light_level = 7;
    common->light_color = 180;
    state.lighting = true;
    state.ambient = 0;
    scene.reset();
    frame = scene.build(&view, state);
    if (!require(frame.light.size() == QSize(17, 13) && frame.light.pixelColor(8, 6).red() > 0 &&
                     frame.light.pixelColor(8, 6).green() == 0,
                 "Client light-map colour or lattice coordinates differ"))
        return false;
    // Entering a house raises the first visible floor, revealing the local player.
    map.placeItem(101, 101, 6, 1000, 0, false, true);
    frame = scene.build(&view, state);
    if (!require(view.previewFirstVisibleFloor(100, 100, 7) == 7 && !frame.labels.isEmpty(),
                 "Entering a house did not hide the roof above the player"))
        return false;
    return true;
}
static bool testPreviewWalkAnimation(const QTemporaryDir &directory, DatReader &dat, SprReader &spr)
{
    auto *outfit = const_cast<ClientItem *>(dat.outfitByLookType(1));
    const ClientItem original = *outfit;
    struct Restore
    {
        ClientItem *item;
        ClientItem value;
        ~Restore()
        {
            *item = value;
        }
    } restore{outfit, original};
    OtbReader otb;
    if (!previewItems(directory, otb, {{1000, 100}}))
        return false;
    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    map.placeItem(100, 100, 7, 1000, 0, false, true);
    MapView source;
    source.setOtb(&otb);
    source.setDat(&dat);
    source.setSpr(&spr);
    source.setOtbm(&map);
    IngamePreviewController controller;
    controller.setSource(&source);
    controller.setNoClip(true);
    controller.setLookType(1);
    controller.setSpeed(220);
    IngamePreviewView view;
    view.setSize({480, 352});
    view.setSource(&source);
    view.setController(&controller);
    view.setLighting(false);
    for (bool grouped : {false, true})
    {
        *outfit = original;
        outfit->width = outfit->height = outfit->layers = outfit->pattern_x = outfit->pattern_y =
            outfit->pattern_z = 1;
        outfit->frames = grouped ? 1 : 3;
        outfit->sprite_ids = {1, 2, 3};
        outfit->sprite_groups.clear();
        if (grouped)
        {
            ClientSpriteGroup idle;
            idle.type = 0;
            idle.frames = 1;
            idle.sprite_ids = {1};
            ClientSpriteGroup walk;
            walk.type = 1;
            walk.frames = 2;
            walk.sprite_ids = {2, 3};
            outfit->sprite_groups = {idle, walk};
        }
        view.setSource(nullptr);
        view.setSource(&source);
        controller.setPosition(100, 100, 7);
        QSet<QRgb> phases;
        if (!controller.walk(1, 0))
            return false;
        QElapsedTimer timer;
        timer.start();
        while (controller.walking() && timer.elapsed() < 2000)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            view.refresh();
            if (!view.frame().draws.isEmpty())
                phases.insert(view.frame().draws.last().image.pixel(0, 0));
            QThread::msleep(5);
        }
        qInfo() << "Walking outfit frame-group=" << grouped << " distinct images=" << phases.size();
        if (!require(phases.contains(green.rgba()) && phases.contains(blue.rgba()),
                     "Walking preview did not render both foot animation phases"))
            return false;
        view.refresh();
        if (!require(view.frame().draws.last().image.pixelColor(0, 0) == red,
                     "Walking preview did not return to its idle sprite"))
            return false;
    }
    return true;
}

static bool testPreviewClientReference(const QStringList &args)
{
    const int at = args.indexOf("--preview-reference");
    if (at < 0 || args.size() < at + 4)
        return false;
    const QString folder = args[at + 1], expectedPath = args[at + 2], out = args[at + 3];
    QDir().mkpath(out);
    DatReader dat;
    dat.setClientVersion(772);
    SprReader spr;
    OtbReader otb;
    QTemporaryDir directory;
    if (!dat.loadFile(QDir(folder).filePath("Tibia.dat")) ||
        !spr.loadFile(QDir(folder).filePath("Tibia.spr")))
        return false;
    int groundId = 0;
    for (int id = 100; id < 65536; ++id)
    {
        const auto *t = dat.itemByClientId(id);
        if (t && t->has(ClientProperty::Ground) && t->width == 1 && t->height == 1 && t->pattern_x == 1 &&
            t->pattern_y == 1 && t->pattern_z == 1 && !t->offset_x && !t->offset_y && !t->elevation &&
            !t->has(ClientProperty::Solid) && !t->has(ClientProperty::FloorChange) && t->ground_speed > 0 &&
            t->ground_speed <= 150)
        {
            groundId = id;
            break;
        }
    }
    if (!groundId || !previewItems(directory, otb, {{1000, groundId}}))
        return false;
    OtbmReader map;
    map.newMap(1024, 1024, 772, 2, 3);
    map.placeItem(100, 200, 7, 1000, 0, false, true);
    MapView source;
    source.setOtb(&otb);
    source.setDat(&dat);
    source.setSpr(&spr);
    source.setOtbm(&map);
    IngamePreviewState s;
    s.x = 100;
    s.y = 200;
    s.visualX = 100;
    s.visualY = 200;
    s.lookType = 128;
    s.name = "Dewral";
    s.lighting = false;
    IngamePreviewScene composer;
    auto frame = composer.build(&source, s);
    const auto actual = frame.referenceImage();
    const QImage expected(expectedPath);
    actual.save(out + "/dme-reference.png");
    if (!require(!expected.isNull() && actual.size() == expected.size(),
                 "Reference-client screenshot is missing or has different dimensions"))
        return false;
    int different = 0;
    for (int y = 0; y < actual.height(); ++y)
        for (int x = 0; x < actual.width(); ++x)
            different += actual.pixel(x, y) != expected.pixel(x, y);
    qInfo() << "DME/client reference differing pixels:" << different << "/"
            << actual.width() * actual.height();
    if (!require(different == 0, "DME preview pixels differ from reconstruction WorldScene"))
        return false;
    // Real QRhi draw path, compared against the same CPU reference.
    IngamePreviewController controller;
    controller.setSource(&source);
    controller.setPosition(100, 200, 7);
    QQuickWindow window;
    window.resize(480, 352);
    IngamePreviewView view(window.contentItem());
    view.setSize({480, 352});
    view.setSource(&source);
    view.setController(&controller);
    view.setLighting(false);
    window.show();
    if (!require(waitUntil([&] { return window.isExposed(); }), "Preview GPU window was not exposed"))
        return false;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 600)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    const auto gpu = window.grabWindow();
    gpu.save(out + "/dme-gpu.png");
    if (gpu.size() != actual.size())
        return false;
    different = 0;
    for (int y = 0; y < actual.height(); ++y)
        for (int x = 0; x < actual.width(); ++x)
        {
            const auto a = gpu.pixelColor(x, y), b = actual.pixelColor(x, y);
            different += qAbs(a.red() - b.red()) > 1 || qAbs(a.green() - b.green()) > 1 ||
                         qAbs(a.blue() - b.blue()) > 1;
        }
    qInfo() << "GPU/client reference differing pixels:" << different;
    if (!require(different == 0, "QRhi preview differs from reference pixels")) return false;
    controller.setNoClip(true);
    if (!controller.walk(1,0)) return false;
    QSet<quint64> walkingFrames;
    timer.restart();
    while (controller.walking() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,10);
        view.refresh();
        if (!view.frame().draws.isEmpty()) walkingFrames.insert(view.frame().draws.last().image.cacheKey());
        QThread::msleep(5);
    }
    qInfo() << "Original 7.72 player walking images:" << walkingFrames.size();
    return require(walkingFrames.size() >= 2, "Original 7.72 player did not animate during walking");
}

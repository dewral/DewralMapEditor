#pragma once

bool testLegacySubtypeBackgroundLoad(const QTemporaryDir &directory)
{
    const auto node = [](int type, const QByteArray &properties, const QByteArray &children = QByteArray()) {
        QByteArray out(1, char(0xfe)), data(1, char(type));
        data += properties;
        for (char byte : data) {
            if (quint8(byte) >= 0xfd) out.append(char(0xfd));
            out.append(byte);
        }
        out += children;
        out.append(char(0xff));
        return out;
    };
    QByteArray itemType(4, '\0');
    itemType.append(char(0x10)); appendU16(itemType, 2); appendU16(itemType, 2006);
    const QString otbPath = directory.filePath("legacy.otb");
    OtbReader definitions;
    if (!require(writeFile(otbPath, QByteArrayLiteral("OTBI")
                         + node(0, QByteArray(4, '\0'), node(12, itemType)))
                 && definitions.loadFile(otbPath), "Cannot create legacy fluid definitions")) return false;

    QByteArray header, area, vial;
    appendU32(header, 0); appendU16(header, 512); appendU16(header, 512);
    appendU32(header, 1); appendU32(header, 3);
    appendU16(area, 100); appendU16(area, 100); area.append(char(7));
    appendU16(vial, 2006); vial.append(char(7)); // Inline mana subtype, then AID.
    vial.append(char(4)); appendU16(vial, 1234);
    const QString path = directory.filePath("legacy.otbm");
    if (!require(writeFile(path, QByteArray(4, '\0')
                         + node(0, header, node(2, {}, node(4, area,
                              node(5, QByteArray(2, '\0'), node(6, vial)))))),
                 "Cannot create legacy map")) return false;

    OtbmReader document;
    MapView view;
    view.setOtb(&definitions);
    view.setOtbm(&document);
    bool finished = false, success = false;
    QObject::connect(&view, &MapView::mapLoadFinished, &view,
                     [&](bool ok, const QString &, const QString &error) {
        finished = true;
        success = ok;
        if (!ok) qCritical() << error;
    });
    if (!require(view.loadMap(path) && waitUntil([&] { return finished; }) && success,
                 "Background decoder did not receive legacy item types")) return false;
    const auto *tile = document.tileAt(100, 100, 7);
    if (!require(tile && tile->items.size() == 1 && tile->items[0].count == 7
                 && tile->items[0].actionId() == 1234,
                 "Background load lost the fluid subtype or attributes")) return false;

    // Autosave uses the original map version, before the normal Save action
    // can upgrade it. Its writer must retain the adopted type snapshot.
    const QString recovery = directory.filePath("legacy-recovery.otbm");
    OtbmReader reloaded;
    reloaded.setItemTypes(definitions.otbmItemTypes());
    return require(document.saveRecoveryFile(recovery) && reloaded.loadFile(recovery)
                   && reloaded.tileAt(100, 100, 7)->items[0].count == 7,
                   "Recovery lost the legacy subtype after background adoption");
}

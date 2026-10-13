#pragma once

#include "otbmreader.h"
#include <QJsonObject>
#include <QSharedPointer>

// Version 1 SQLite/CBOR interchange. Loading never modifies a live document.
class TmgFormat {
public:
    struct Item { bool ground = false; bool blocked = false; quint32 floorChange = 0; };
    struct Catalog {
        QString fingerprint;
        QHash<int, Item> items;
        QSet<QString> monsters;
        int version = 860, major = 2, minor = 22;
    };
    struct Result { QSharedPointer<OtbmReader> document; QJsonObject manifest; QString error; };
    static Result load(const QString &path, const Catalog &catalog,
                       std::function<void(int, const QString &)> progress = {},
                       std::function<bool()> cancelled = {}, bool smallResult = false);
    static QString itemError(int id, int count, const Catalog &catalog);
    static QString itemError(int id, int count, bool knownItem);
    static QString placementError(const OtbmReader &target, const OtbmReader &source,
                                  int x, int y, int z, int baseZ, int width, int height, int floors);
    static QString placeEmpty(OtbmReader &target, const OtbmReader &source,
                              int x, int y, int z, int baseZ, int width, int height, int floors);
};

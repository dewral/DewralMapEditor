#include "tilemapgenassets.h"
#include "datreader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>

#define CHECK(c) do { if (!(c)) { qCritical() << "Failed at" << __LINE__ << #c; return 1; } } while (false)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(":memory:");
    CHECK(db.open());
    QSqlQuery query(db);
    CHECK(query.exec("CREATE TABLE assets(id INTEGER PRIMARY KEY,png BLOB NOT NULL)"));
    CHECK(query.exec("CREATE TABLE sprite_variants(id INTEGER,pattern INTEGER,png BLOB NOT NULL,PRIMARY KEY(id,pattern))"));
    ClientItem item;
    item.pattern_x = 2; item.pattern_y = 3; item.pattern_z = 2;
    item.elevation = 8; item.enable(ClientProperty::Elevation); item.enable(ClientProperty::Stackable);
    const auto geometry = TileMapGenAssets::geometry(item, 102);
    CHECK(geometry["pattern_x"].toInt() == 2 && geometry["pattern_y"].toInt() == 3 && geometry["pattern_z"].toInt() == 2);
    CHECK(geometry["elevation"].toInt() == 8 && geometry["stackable"].toBool() && geometry["rotate_to"].toInt() == 102);
    int requested = 0;
    const auto sprite = [&requested](int pattern) {
        ++requested;
        QImage image(64, 96, QImage::Format_ARGB32);
        image.fill(QColor(pattern * 10, 20, 30, 180));
        return image;
    };
    CHECK(db.transaction());
    CHECK(TileMapGenAssets::writeItem(db, 101, item, sprite).isEmpty());
    CHECK(db.commit() && requested == 12);
    // Every position pattern must retain its native size, alpha and pixel values.
    CHECK(query.exec("SELECT 0,png FROM assets WHERE id=101 UNION ALL SELECT pattern,png FROM sprite_variants WHERE id=101 ORDER BY 1"));
    int pattern = 0;
    while (query.next()) {
        CHECK(query.value(0).toInt() == pattern);
        const auto image = QImage::fromData(query.value(1).toByteArray(), "PNG");
        CHECK(image.size() == QSize(64, 96));
        CHECK(image.pixelColor(40, 70) == QColor(pattern * 10, 20, 30, 180));
        ++pattern;
    }
    CHECK(pattern == 12);
    query.finish();
    // Missing base art must not discard other available patterns.
    CHECK(TileMapGenAssets::writeItem(db, 102, item, [&sprite](int p) { return p == 0 ? QImage() : sprite(p); }).isEmpty());
    CHECK(query.exec("SELECT COUNT(*) FROM sprite_variants WHERE id=102") && query.next() && query.value(0).toInt() == 11);
    query.finish();
    // Metadata is meaningful even for items with no sprite (e.g. invisible blockers).
    item.properties = 0; item.pattern_x = 0; item.pattern_y = 0; item.pattern_z = 0;
    const auto plain = TileMapGenAssets::geometry(item, 0);
    CHECK(plain["pattern_x"].toInt() == 1 && plain["pattern_y"].toInt() == 1 && plain["pattern_z"].toInt() == 1);
    CHECK(plain["elevation"].toInt() == 0 && !plain["stackable"].toBool());
    CHECK(TileMapGenAssets::writeItem(db, 103, item, [](int) { return QImage(); }).isEmpty());
    // SQL failures must reach the caller, allowing its whole-pack transaction to roll back.
    CHECK(db.transaction());
    CHECK(TileMapGenAssets::writeItem(db, 104, item, sprite).isEmpty());
    CHECK(!TileMapGenAssets::writeItem(db, 101, item, sprite).isEmpty());
    CHECK(db.rollback());
    CHECK(query.exec("SELECT COUNT(*) FROM assets WHERE id=104") && query.next() && query.value(0).toInt() == 0);
    qInfo() << "Sprite variants, native pixels, geometry and rollback passed";
    return 0;
}

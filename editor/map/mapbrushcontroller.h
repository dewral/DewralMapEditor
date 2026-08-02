#ifndef MAPBRUSHCONTROLLER_H
#define MAPBRUSHCONTROLLER_H

#include <QSet>
#include <QString>
#include <QtGlobal>
#include <algorithm>

class BrushStore;

class MapBrushController
{
public:
    static constexpr int InvalidCoordinate = -2000000;

    int &size() { return m_size; }
    int size() const { return m_size; }
    bool setSize(int size)
    {
        size = std::clamp(size, 0, 11);
        if (m_size == size) return false;
        m_size = size;
        return true;
    }

    const QString &shape() const { return m_shape; }
    bool setShape(const QString &shape)
    {
        if (shape != QLatin1String("square") && shape != QLatin1String("circle"))
            return false;
        if (m_shape == shape) return false;
        m_shape = shape;
        return true;
    }

    bool covers(int dx, int dy) const
    {
        if (m_shape == QLatin1String("circle"))
            return dx * dx + dy * dy <= m_size * m_size;
        return std::abs(dx) <= m_size && std::abs(dy) <= m_size;
    }

    void beginStroke(bool erase)
    {
        m_painting = true;
        m_eraseStroke = erase;
        m_lastX = InvalidCoordinate;
        m_lastY = InvalidCoordinate;
        m_placed.clear();
        m_borderTiles.clear();
    }

    void finishStroke()
    {
        m_painting = false;
        m_eraseStroke = false;
        m_lastX = InvalidCoordinate;
        m_lastY = InvalidCoordinate;
        m_placed.clear();
        m_borderTiles.clear();
    }

    bool painting() const { return m_painting; }
    void setPainting(bool painting) { m_painting = painting; }
    bool &eraseStroke() { return m_eraseStroke; }
    bool eraseStroke() const { return m_eraseStroke; }
    void setEraseStroke(bool erase) { m_eraseStroke = erase; }
    bool &bulkEdit() { return m_bulkEdit; }
    bool bulkEdit() const { return m_bulkEdit; }
    void setBulkEdit(bool bulk) { m_bulkEdit = bulk; }
    int lastX() const { return m_lastX; }
    int lastY() const { return m_lastY; }
    void setLastPosition(int x, int y) { m_lastX = x; m_lastY = y; }

    bool markPlaced(quint64 position)
    {
        if (m_placed.contains(position)) return false;
        m_placed.insert(position);
        return true;
    }
    void reservePlaced(qsizetype capacity) { m_placed.reserve(capacity); }
    qsizetype placedCount() const { return m_placed.size(); }
    void clearPlaced() { m_placed.clear(); }
    QSet<quint64> &placed() { return m_placed; }
    const QSet<quint64> &placed() const { return m_placed; }

    QSet<quint64> &borderTiles() { return m_borderTiles; }
    const QSet<quint64> &borderTiles() const { return m_borderTiles; }
    void clearBorderTiles() { m_borderTiles.clear(); }

    int &serverId() { return m_serverId; }
    int serverId() const { return m_serverId; }
    BrushStore *&store() { return m_store; }
    BrushStore *store() const { return m_store; }
    QString &groundBrush() { return m_groundBrush; }
    const QString &groundBrush() const { return m_groundBrush; }
    QString &wallBrush() { return m_wallBrush; }
    const QString &wallBrush() const { return m_wallBrush; }
    QString &doodadBrush() { return m_doodadBrush; }
    const QString &doodadBrush() const { return m_doodadBrush; }
    QString &carpetBrush() { return m_carpetBrush; }
    const QString &carpetBrush() const { return m_carpetBrush; }
    QString &tableBrush() { return m_tableBrush; }
    const QString &tableBrush() const { return m_tableBrush; }
    int &doorBrushId() { return m_doorBrushId; }
    int doorBrushId() const { return m_doorBrushId; }
    int &doodadVariant() { return m_doodadVariant; }
    int doodadVariant() const { return m_doodadVariant; }
    QString &creatureBrush() { return m_creatureBrush; }
    const QString &creatureBrush() const { return m_creatureBrush; }
    bool &spawnBrush() { return m_spawnBrush; }
    bool spawnBrush() const { return m_spawnBrush; }
    int &creatureSpawntime() { return m_creatureSpawntime; }
    int creatureSpawntime() const { return m_creatureSpawntime; }
    int &spawnRadius() { return m_spawnRadius; }
    int spawnRadius() const { return m_spawnRadius; }
    int &houseBrush() { return m_houseBrush; }
    int houseBrush() const { return m_houseBrush; }
    bool &houseExitMode() { return m_houseExitMode; }
    bool houseExitMode() const { return m_houseExitMode; }
    bool &automagic() { return m_automagic; }
    bool automagic() const { return m_automagic; }

private:
    int m_size = 0;
    QString m_shape = QStringLiteral("square");
    QSet<quint64> m_placed;
    QSet<quint64> m_borderTiles;
    bool m_bulkEdit = false;
    bool m_painting = false;
    bool m_eraseStroke = false;
    int m_lastX = InvalidCoordinate;
    int m_lastY = InvalidCoordinate;
    int m_serverId = 0;
    BrushStore *m_store = nullptr;
    QString m_groundBrush;
    QString m_wallBrush;
    QString m_doodadBrush;
    QString m_carpetBrush;
    QString m_tableBrush;
    int m_doorBrushId = 0;
    int m_doodadVariant = -1;
    QString m_creatureBrush;
    bool m_spawnBrush = false;
    int m_creatureSpawntime = 60;
    int m_spawnRadius = 3;
    int m_houseBrush = 0;
    bool m_houseExitMode = false;
    bool m_automagic = true;
};

#endif

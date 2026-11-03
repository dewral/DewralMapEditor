#pragma once
#include "otbmreader.h"
#include <QObject>
#include <QPointer>
#include <QRegion>
#include <QJsonObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <QFutureWatcher>

class WeatherRegionDocument : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QVariantList regions READ regions NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    Q_PROPERTY(QStringList presets READ presets NOTIFY changed)
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY selectionChanged)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool canUndoGeometry READ canUndoGeometry NOTIFY changed)
    Q_PROPERTY(bool canRedoGeometry READ canRedoGeometry NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY operationChanged)
    Q_PROPERTY(int progress READ progress NOTIFY operationChanged)
    Q_PROPERTY(QString exportedPackage READ exportedPackage NOTIFY operationChanged)
public:
    explicit WeatherRegionDocument(OtbmReader *map);
    ~WeatherRegionDocument() override;
    bool busy() const { return m_operation!=nullptr; }
    int progress() const { return m_progress; }
    QString exportedPackage() const { return m_exportedPackage; }
    Q_INVOKABLE bool exportPackage(const QString &parent, bool migrateLegacy = false);
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void setTestTimeout(int seconds);
    QVariantList regions() const;
    QString selectedId() const { return m_selected; }
    void setSelectedId(const QString &id);
    QVariantMap selected() const;
    QStringList presets() const;
    QVariantMap preview() const;
    Q_INVOKABLE QVariantMap previewPreset(const QString &id) const;
    Q_INVOKABLE bool applyWeather(const QVariantMap &mix, int minimumSeconds, int maximumSeconds, bool stopAfter);
    QString path() const;
    QString error() const { return m_error; }
    bool dirty() const { return m_dirty; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    bool canUndoGeometry() const;
    bool canRedoGeometry() const;
    Q_INVOKABLE void undoGeometry();
    Q_INVOKABLE void redoGeometry();
    Q_INVOKABLE bool createRegion(const QString &id, const QString &name);
    Q_INVOKABLE bool applyRegion(const QVariantMap &options);
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void beginStroke();
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void cancelStroke();
    Q_INVOKABLE void paint(int x, int y, int floor, int radius, bool circle, bool erase);
    Q_INVOKABLE void paintLine(int x1, int y1, int x2, int y2, int floor, int radius, bool circle, bool erase);
    Q_INVOKABLE void paintRectangle(int x1, int y1, int x2, int y2, int floor, bool erase);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool importFile(const QString &file);
    Q_INVOKABLE bool exportFile(const QString &file);
    Q_INVOKABLE QVariantList visibleRects(int left, int top, int right, int bottom, int floor) const;
signals:
    void operationChanged();
    void testTimedOut();
    void changed();
    void selectionChanged();
private:
    struct Region { QJsonObject definition; QMap<int,QRegion> floors; };
    struct Snapshot { QJsonObject environment; QList<Region> regions; QString selected; };
    Snapshot snapshot() const;
    void restore(const Snapshot &value);
    void commit(const Snapshot &before);
    void applyShape(const QRegion &shape, int floor, bool erase);
    bool load(const QByteArray &bytes);
    QByteArray serialize(bool editor) const;
    bool write(const QString &file, bool editor);
    bool fail(const QString &message);
    void notify();
    void storeDefinition(const QJsonObject &definition);
    QRegion geometry(const Region &region,int floor) const;
    void importZoneWeather(const QString &source,const QVariantMap &mapping);
    Region *current();
    const Region *current() const;
    OtbmReader *m_map;
    QJsonObject m_environment;
    QList<Region> m_regions;
    QString m_selected, m_error, m_lastPath;
    QList<Snapshot> m_undo, m_redo;
    Snapshot m_before;
    bool m_stroke=false, m_strokeChanged=false, m_dirty=false;
    bool m_zoneStroke=false;
    QFutureWatcher<QString> *m_operation=nullptr;
    int m_progress=0;
    QString m_exportedPackage;
};

class WeatherRegions : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(WeatherRegions)
    Q_PROPERTY(OtbmReader *map READ map WRITE setMap NOTIFY documentChanged)
    Q_PROPERTY(WeatherRegionDocument *document READ document NOTIFY documentChanged)
public:
    explicit WeatherRegions(QObject *parent=nullptr) : QObject(parent) {}
    OtbmReader *map() const { return m_map; }
    WeatherRegionDocument *document() const { return m_document; }
    void setMap(OtbmReader *map);
signals:
    void documentChanged();
private:
    QPointer<OtbmReader> m_map;
    QPointer<WeatherRegionDocument> m_document;
};

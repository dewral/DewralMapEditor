#pragma once
#include "tmgformat.h"
#include "tilemapgenprocess.h"
#include <QObject>
#include <QPointer>
#include <QFutureWatcher>
#include <QUrl>
#include <QTemporaryDir>
#include <atomic>
#include <memory>

class Backend;
class MapView;
class TileMapGenBridge : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString errorDetails READ errorDetails NOTIFY changed)
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString generatorDirectory READ generatorDirectory CONSTANT)
public:
    explicit TileMapGenBridge(Backend *backend);
    ~TileMapGenBridge() override;
    bool busy() const { return m_busy; }
    int progress() const { return m_progress; }
    QString status() const { return m_status; }
    QString errorDetails() const { return m_errorDetails; }
    bool previewReady() const { return !m_preview.document.isNull(); }
    QString generatorDirectory() const { return QStringLiteral("C:/github2/TileMapGenerator"); }
    bool available() const { return !TileMapGenProcess::programForDirectory(generatorDirectory()).isEmpty(); }
    Q_INVOKABLE QVariantMap generationMaterials() const;
    Q_INVOKABLE void generateMap(const QVariantMap &options, QObject *mapView);
    Q_INVOKABLE QString exportAssets(const QUrl &path, int downId, int upId);
    Q_INVOKABLE void importMap(const QUrl &path, bool placement, QObject *mapView, int x, int y, int z);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QString applyPreview();
    Q_INVOKABLE void clearPreview();
signals:
    void changed();
    void finished(bool success);
private:
    QJsonObject catalogJson() const;
    TmgFormat::Catalog catalog() const;
    QString writeAssets(const QUrl &path, int downId, int upId, bool sprites,
                        const QVariantMap &palette = {});
    void failGeneration(const QString &error);
    Backend *m_backend;
    QFutureWatcher<TmgFormat::Result> m_watcher;
    TileMapGenProcess m_generator;
    std::unique_ptr<QTemporaryDir> m_workDirectory;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QPointer<MapView> m_view;
    QPointer<OtbmReader> m_target;
    TmgFormat::Result m_preview;
    bool m_busy = false, m_placement = false, m_generating = false;
    int m_progress = 0, m_x = 0, m_y = 0, m_z = 7;
    QString m_status;
    QString m_errorDetails;
    QString m_importFingerprint;
    mutable QHash<QString, QString> m_fileHashes;
};

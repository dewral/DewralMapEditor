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
    Q_PROPERTY(bool settlementAvailable READ settlementAvailable NOTIFY changed)
    Q_PROPERTY(bool aiBusy READ aiBusy NOTIFY changed)
    Q_PROPERTY(QVariantMap aiStatus READ aiStatus NOTIFY changed)
public:
    explicit TileMapGenBridge(Backend *backend);
    ~TileMapGenBridge() override;
    bool busy() const { return m_busy || m_helper.running(); }
    int progress() const { return m_progress; }
    QString status() const { return m_status; }
    QString errorDetails() const { return m_errorDetails; }
    bool previewReady() const { return !m_preview.document.isNull(); }
    QString generatorDirectory() const { const auto path = qEnvironmentVariable("TILEMAPGEN_DIRECTORY"); return path.isEmpty() ? QStringLiteral("C:/github2/TileMapGenerator") : path; }
    bool settlementAvailable() const { return m_settlementAvailable; }
    bool aiBusy() const { return m_helperMode == "prepare-ai" || m_helperMode == "plan-ai"; }
    QVariantMap aiStatus() const { return m_aiStatus; }
    bool available() const { return !TileMapGenProcess::programForDirectory(generatorDirectory()).isEmpty(); }
    Q_INVOKABLE QVariantMap generationMaterials() const;
    Q_INVOKABLE void generateMap(const QVariantMap &options, QObject *mapView);
    Q_INVOKABLE QString exportAssets(const QUrl &path, int downId, int upId);
    Q_INVOKABLE void importMap(const QUrl &path, bool placement, QObject *mapView, int x, int y, int z);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QString applyPreview();
    Q_INVOKABLE void clearPreview();
    Q_INVOKABLE void refreshCapabilities();
    Q_INVOKABLE void setAiCredentials(const QString &key, bool remember);
    Q_INVOKABLE void forgetAiCredentials();
    Q_INVOKABLE void requestAiPlan(const QString &prompt, const QVariantMap &options, const QString &model);
    Q_INVOKABLE void cancelAiPlan();
signals:
    void changed();
    void finished(bool success);
    void aiPlanReady(const QVariantMap &blueprint);
    void aiFailed(const QString &message);
private:
    QJsonObject catalogJson() const;
    TmgFormat::Catalog catalog() const;
    QString writeAssets(const QUrl &path, int downId, int upId, bool sprites,
                        const QVariantMap &palette = {}, const QVariantMap &settlementMaterials = {});
    void failGeneration(const QString &error);
    void helperFinished(bool success, const QString &error);
    bool writeJson(const QString &path, const QJsonObject &value);
    Backend *m_backend;
    QFutureWatcher<TmgFormat::Result> m_watcher;
    TileMapGenProcess m_generator;
    TileMapGenProcess m_helper;
    std::unique_ptr<QTemporaryDir> m_aiWorkDirectory;
    QString m_helperMode, m_aiFingerprint, m_aiPrompt, m_aiModel;
    QByteArray m_sessionApiKey;
    QVariantMap m_aiStatus;
    bool m_settlementAvailable = false;
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

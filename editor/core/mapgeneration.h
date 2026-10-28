#pragma once
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <memory>

namespace MapGeneration
{
constexpr int ModelVersion = 1;
constexpr int MaxCells = 100000;
struct MapStyle
{
    QString id, profile;
    QVariantList examples, prefabs;
    QVariantMap parameters;
};
struct MapBlueprint
{
    QString styleId;
    QVariantList mask, paths, pins;
    QVariantMap parameters;
    quint32 seed = 1;
};
struct GenerationRequest
{
    QVariantMap model, blueprint, context;
};
struct GenerationResult
{
    bool success = false;
    QVariantMap patch;
    QString error;
    QStringList diagnostics;
};
using Cancel = std::shared_ptr<std::atomic_bool>;
using Progress = std::function<void(int)>;
QVariantMap learn(const QVariantList &examples, const QVariantList &prefabs, const Cancel &cancel,
                  const Progress &progress = {});
GenerationResult finishLayout(const GenerationRequest &, const QVariantList &, const Cancel &,
                              const Progress &progress = {});
QVariantMap learnProcedural(const QVariantList &, const QVariantList &, const QVariantMap &, const Cancel &,
                            const Progress &progress = {});
GenerationResult generateProcedural(const GenerationRequest &, const Cancel &, const Progress &progress = {});
QVariantMap learnRaw(const QVariantList &, const QVariantList &, const Cancel &,
                     const Progress &progress = {});
GenerationResult generateRaw(const GenerationRequest &, const Cancel &, const Progress &progress = {});
GenerationResult generate(const GenerationRequest &, const Cancel &, const Progress &progress = {});
quint64 key(int x, int y, int z);
} // namespace MapGeneration

#pragma once
#include <QElapsedTimer>
#include <QString>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace LoadProfile {
inline std::atomic_bool enabled{false};
inline QElapsedTimer clock;
inline std::mutex outputMutex;
inline void start(bool on) { clock.start(); enabled.store(on); }
inline void record(const QString &stage, qint64 duration = -1, const QString &details = {}) {
    if (!enabled.load()) return;
    std::lock_guard<std::mutex> lock(outputMutex);
    quint64 peak = 0;
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS counters{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        peak = counters.PeakWorkingSetSize;
#endif
    std::fprintf(stderr, "DME_LOAD elapsed_ms=%lld duration_ms=%lld peak_ram_bytes=%llu stage=%s %s\n",
        static_cast<long long>(clock.elapsed()), static_cast<long long>(duration),
        static_cast<unsigned long long>(peak), stage.toUtf8().constData(), details.toUtf8().constData());
    std::fflush(stderr);
}
struct Scope {
    QString stage;
    QElapsedTimer timer;
    explicit Scope(QString name) : stage(std::move(name)) { timer.start(); }
    ~Scope() { record(stage, timer.elapsed()); }
};
}

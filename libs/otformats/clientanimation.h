#ifndef CLIENTANIMATION_H
#define CLIENTANIMATION_H
#include <QtGlobal>
#include <QRandomGenerator>
#include <algorithm>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

// DAT animation metadata and independently implemented playback state.
struct ClientAnimation {
    struct Duration { quint32 minimum, maximum; };
    bool asynchronous = false;
    qint32 loops = 0; // 0: repeat, negative: ping-pong, positive: finite.
    qint8 start = 0; // -1: random initial phase.
    std::vector<Duration> durations;
private:
    struct Playback {
        std::mutex mutex;
        quint32 seed = QRandomGenerator::global()->generate();
        std::minstd_rand random;
        bool initialized = false, complete = false;
        int phase = 0, direction = 1;
        quint32 completedLoops = 0;
        quint64 origin = 0, boundary = 0, lastTime = 0;
    };
    mutable std::shared_ptr<Playback> playback;
public:
    int frameAt(int frames, quint64 time) const {
        if (frames <= 1 || durations.size() != size_t(frames)) return 0;
        auto state = std::atomic_load(&playback);
        if (!state) {
            static std::mutex initializationMutex;
            std::lock_guard<std::mutex> initialize(initializationMutex);
            state = std::atomic_load(&playback);
            if (!state) {
                state = std::make_shared<Playback>();
                std::atomic_store(&playback, state);
            }
        }
        auto &p = *state;
        std::lock_guard<std::mutex> lock(p.mutex);
        const auto sample = [&]() -> quint64 {
            const auto &d = durations[size_t(p.phase)];
            const quint32 low = std::max<quint32>(1, d.minimum);
            const quint32 high = std::max(low, d.maximum);
            return std::uniform_int_distribution<quint32>(low, high)(p.random);
        };
        if (!p.initialized || time < p.lastTime) {
            if (!p.initialized) p.origin = asynchronous ? time : 0;
            p.random.seed(p.seed);
            p.phase = start == -1 ? std::uniform_int_distribution<int>(0, frames - 1)(p.random)
                                 : std::clamp<int>(start, 0, frames - 1);
            p.direction = 1; p.completedLoops = 0; p.complete = false;
            p.boundary = p.origin + sample();
            p.initialized = true;
        }
        while (!p.complete && time >= p.boundary) {
            if (loops < 0) {
                if (p.phase + p.direction >= frames || p.phase + p.direction < 0)
                    p.direction = -p.direction;
                p.phase += p.direction;
            } else if (p.phase + 1 < frames) {
                ++p.phase;
            } else {
                ++p.completedLoops;
                if (loops > 0 && p.completedLoops >= quint32(loops)) {
                    p.complete = true;
                    break;
                }
                p.phase = 0;
            }
            p.boundary += sample();
        }
        p.lastTime = time;
        return p.phase;
    }
};
#endif
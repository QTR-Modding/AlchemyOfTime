// https://github.com/ozooma10/OSLAroused-SKSE/blob/master/src/Utilities/Ticker.h
#pragma once

class Ticker {
public:
    virtual ~Ticker() = default;

    Ticker(const std::function<void()>& onTick, const std::chrono::milliseconds interval)
        : m_OnTick(onTick), m_Interval(interval), m_ThreadActive(false), m_Running(false) {
    }

    void Start();

    void Stop() { m_Running = false; }

    void UpdateInterval(std::chrono::milliseconds newInterval);

    bool isRunning() const { return m_Running; }

private:
    void RunLoop();

    std::function<void()> m_OnTick;
    std::chrono::milliseconds m_Interval;

    std::atomic<bool> m_ThreadActive;
    std::atomic<bool> m_Running;
    std::mutex m_IntervalMutex;
};
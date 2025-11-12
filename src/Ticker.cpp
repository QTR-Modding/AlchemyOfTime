#include "Ticker.h"

void Ticker::Start() {
    if (m_Running) {
        return;
    }
    m_Running = true;
    // logger::info("Start Called with thread active state of: {}", m_ThreadActive);
    if (!m_ThreadActive) {
        std::thread tickerThread(&Ticker::RunLoop, this);
        tickerThread.detach();
    }
}

void Ticker::UpdateInterval(const std::chrono::milliseconds newInterval) {
    m_IntervalMutex.lock();
    m_Interval = newInterval;
    m_IntervalMutex.unlock();
}

void Ticker::RunLoop() {
    m_ThreadActive = true;
    while (m_Running) {
        m_OnTick();
        {
            std::lock_guard lock(m_IntervalMutex);
            std::chrono::milliseconds interval = m_Interval;
            std::this_thread::sleep_for(interval);
        }
    }
    m_ThreadActive = false;
}
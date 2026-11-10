#pragma once
#include <REX/REX/Singleton.h>
#include <CLibUtil/rng.hpp>
#include "ClibUtilsQTR/Ticker.hpp"
#include "Presets.h"

class ActivationQueue : public REX::Singleton<ActivationQueue>, public Ticker {
public:
    using Key = std::tuple<FormID, FormID, FormID, TriggerKind>; // source, item, trigger, kind

    ActivationQueue();
    ~ActivationQueue();

    void Reset();
    std::vector<Key> GetQueuedTriggers(RefID refid);
    void RemoveTrigger(RefID refid, const Key& key);
    [[nodiscard]] bool CanActivate(RefID refid, const Key& key, float activation_delay);

private:
    void UpdateLoop();
    void RequestUpdates(const std::vector<RefID>& refids);

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };

    struct Entry {
        float activation_delay;
        bool ready = false;
    };

    float last_update_ = 0.0f;
    clib_util::RNG<> random_;
    std::mutex mutex_;
    std::unordered_map<RefID, std::unordered_map<Key, Entry, KeyHash>> pending_;
};

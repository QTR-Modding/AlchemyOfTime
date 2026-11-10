#pragma once
#include <REX/REX/Singleton.h>
#include <CLibUtil/rng.hpp>
#include "ClibUtilsQTR/Ticker.hpp"

class ActivationQueue : public REX::Singleton<ActivationQueue>, public Ticker {
public:
    ActivationQueue();
    ~ActivationQueue();

    void Reset();
    [[nodiscard]] bool CanActivate(RefID refid, FormID base_id, FormID trigger_id, float activation_delay);

private:
    void UpdateLoop();
    void RequestUpdates(const std::vector<RefID>& refids);

    struct Entry {
        float activation_delay;
        bool ready = false;
    };

    float last_update_ = 0.0f;
    clib_util::RNG<> random_;
    std::mutex mutex_;
    std::unordered_map<RefID, std::unordered_map<FormID, std::unordered_map<FormID, Entry>>> pending_;
};

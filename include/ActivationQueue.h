#pragma once
#include <REX/REX/Singleton.h>

class ActivationQueue : public REX::Singleton<ActivationQueue> {
public:
    [[nodiscard]] bool CanActivate(RefID refid, FormID base_id, FormID trigger_id, float activation_delay);

private:
    struct Entry {
        float activation_delay;
        bool ready = false;
    };

    std::mutex mutex_;
    std::unordered_map<RefID, std::unordered_map<FormID, std::unordered_map<FormID, Entry>>> pending_;
};

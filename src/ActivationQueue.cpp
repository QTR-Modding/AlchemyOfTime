#include "ActivationQueue.h"

bool ActivationQueue::CanActivate(const RefID refid, const FormID base_id, const FormID trigger_id,
                                  const float activation_delay) {
    if (activation_delay == 0.0f) {
        return true;
    }

    std::lock_guard lock(mutex_);
    const auto [it, inserted] = pending_[refid][base_id].try_emplace(trigger_id, Entry{activation_delay});
    return it->second.ready;
}

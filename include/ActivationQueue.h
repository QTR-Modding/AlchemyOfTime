#pragma once
#include <REX/REX/Singleton.h>

class ActivationQueue : public REX::Singleton<ActivationQueue> {
public:
    [[nodiscard]] bool CanActivate(RefID refid, FormID base_id, FormID trigger_id, float activation_delay);
};

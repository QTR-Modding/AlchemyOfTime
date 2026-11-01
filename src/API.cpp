#include "API.h"
#include "Manager.h"
#include "NumericKeywords.h"

namespace {
    RE::FormID GetValueKeyword(const float value) {
        if (!std::isfinite(value)) return 0;
        return NumericKeywords::Create(value)->GetFormID();
    }

    bool VisitInstances(const RE::FormID owner, const RE::FormID current_form, void* context,
                        const AoTAPI::Visitor visitor) {
        if (!M || M->isUninstalled.load() || M->isLoading.load() || !owner || !current_form || !visitor) return false;
        const auto instances = M->GetInstanceInfo(current_form, owner);
        for (const auto& instance : instances) visitor(&instance, context);
        return true;
    }
}

extern "C" __declspec(dllexport) const AoTAPI::Interface* AoT_RequestAPI(const std::uint32_t version) {
    if (version != AoTAPI::interface_version) return nullptr;
    static const AoTAPI::Interface api{VisitInstances, GetValueKeyword};
    return &api;
}

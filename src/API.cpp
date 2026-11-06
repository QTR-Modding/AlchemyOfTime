#include "API.h"
#include "Manager.h"

namespace {
    bool GetRemainingTime(RE::FormID location, RE::FormID item, float& hours) {
        if (!M || M->isUninstalled.load()) return false;
        const auto remaining = M->GetRemainingTime(location, item);
        if (!remaining) return false;
        hours = *remaining;
        return true;
    }
}

extern "C" __declspec(dllexport) const AoTAPI::Interface* AoT_RequestAPI(std::uint32_t version) {
    if (version != AoTAPI::interface_version) return nullptr;
    static const AoTAPI::Interface api{GetRemainingTime};
    return &api;
}

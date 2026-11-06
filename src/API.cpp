#include "API.h"
#include "Manager.h"

namespace {
    std::uint32_t GetProgress(RE::FormID owner, RE::FormID item, std::span<AoTAPI::Progress> output) {
        return M && !M->isUninstalled.load() ? M->GetProgress(owner, item, output) : 0;
    }
}

extern "C" __declspec(dllexport) const AoTAPI::Interface* AoT_RequestAPI(std::uint32_t version) {
    if (version != AoTAPI::interface_version) return nullptr;
    static const AoTAPI::Interface api{GetProgress};
    return &api;
}

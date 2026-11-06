#include "API.h"
#include "Manager.h"

namespace {
    std::uint32_t GetProgress(RE::FormID owner, RE::FormID item, std::span<AoT_API::Progress> output) {
        return M && !M->isUninstalled.load() ? M->GetProgress(owner, item, output) : 0;
    }
}

extern "C" __declspec(dllexport) const AoT_API::Interface* AoT_RequestAPI(std::uint32_t version) {
    if (version != AoT_API::interface_version) return nullptr;
    static const AoT_API::Interface api{GetProgress};
    return &api;
}

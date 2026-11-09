#pragma once

#include <cstdint>
#include <span>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace AoT_API {
    constexpr std::uint32_t interface_version = 1;

    // SKSE mod event after stage or trigger changes.
    // sender is the updated world reference or inventory owner.
    constexpr auto kReferenceUpdated = "AoT_ReferenceUpdated";

    struct Progress {
        float startTime;      // Game hour when the current slope started.
        float elapsed;        // Progress accumulated before startTime, in game hours.
        float slope;
        float totalDuration;  // Active stage or transformation duration, in game hours.
        RE::FormID triggerID;
    };

    struct Interface {
        // Game thread, after kDataLoaded. Reads recorded state without updating it.
        // Returns the total matches; writes up to output.size(). Empty output queries the count.
        std::uint32_t (*GetProgress)(RE::FormID owner, RE::FormID item, std::span<Progress> output);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        const auto module = GetModuleHandleW(L"AlchemyOfTime.dll");
        if (!module) return nullptr;
        using Request = const Interface* (*)(std::uint32_t);
        const auto request = reinterpret_cast<Request>(GetProcAddress(module, "AoT_RequestAPI"));
        return request ? request(interface_version) : nullptr;
    }
}

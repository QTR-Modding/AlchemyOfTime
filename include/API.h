#pragma once

#include <cstdint>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace AoTAPI {
    constexpr std::uint32_t interface_version = 1;

    struct Interface {
        // Game thread, after kDataLoaded. Reads recorded state without updating it.
        // Returns false if untracked/unavailable; hours is unchanged on failure.
        // Infinity means no active countdown. Multiple stacks return the soonest update.
        bool (*GetRemainingTime)(RE::FormID location, RE::FormID item, float& hours);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        using Request = const Interface* (*)(std::uint32_t);
        const auto module = GetModuleHandleW(L"AlchemyOfTime.dll");
        const auto request = module ? reinterpret_cast<Request>(GetProcAddress(module, "AoT_RequestAPI")) : nullptr;
        return request ? request(interface_version) : nullptr;
    }
}

#pragma once

#include <cstdint>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace AoTAPI {
    constexpr std::uint32_t interface_version = 1;
    enum State : std::uint32_t {
        Transforming = 1u << 0,
        Paused = 1u << 1,
        Reversing = 1u << 2
    };

    struct InstanceInfo {
        RE::FormID source;
        RE::FormID current;
        RE::FormID next;
        std::int32_t count;
        std::uint32_t stage;
        float remainingHours;   // Next transition; -1 means no scheduled transition.
        float progressPercent;  // -1 means unavailable.
        float multiplier;
        std::uint32_t state;
    };

    using Visitor = void (*)(const InstanceInfo* instance, void* context);
    struct Interface {
        bool (*VisitInstances)(RE::FormID owner, RE::FormID currentForm, void* context, Visitor visitor);
        RE::FormID (*GetValueKeyword)(float value);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        const auto module = GetModuleHandleW(L"AlchemyOfTime.dll");
        if (!module) return nullptr;
        using Request = const Interface* (*)(std::uint32_t);
        const auto request = reinterpret_cast<Request>(GetProcAddress(module, "AoT_RequestAPI"));
        return request ? request(interface_version) : nullptr;
    }
}

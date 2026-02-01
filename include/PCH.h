#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

constexpr float EPSILON = 1e-10f;

namespace logger = SKSE::log;
using namespace std::literals;

constexpr uint32_t player_refid = 20;

using FormID = RE::FormID;
using RefID = RE::FormID;
using Count = RE::TESObjectREFR::Count;
using InvMap = RE::TESObjectREFR::InventoryItemMap;
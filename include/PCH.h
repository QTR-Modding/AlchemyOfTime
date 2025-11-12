#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <spdlog/sinks/basic_file_sink.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800

#undef GetObject

constexpr float EPSILON = 1e-10f;

#include <wrl/client.h>

#include <DirectXMath.h>

namespace logger = SKSE::log;
using namespace std::literals;

constexpr uint32_t player_refid = 20;

using FormID = RE::FormID;
using RefID = RE::FormID;
using Count = RE::TESObjectREFR::Count;
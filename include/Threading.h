#pragma once
#include "CLibUtilsQTR/SpeedProfiler.hpp"
#include "CLibUtilsQTR/ThreadPool.hpp"

inline size_t numThreads = std::max(1u, std::thread::hardware_concurrency());

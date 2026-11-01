#include "Events.h"
#include "Logger.h"
#include "Serialization.h"
#include "Threading.h"

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse, false);
    InitializeSerialization();
    if (!EventSink::GetSingleton()->Register(skse)) {
        SKSE::stl::report_and_fail("Failed to register message listener");
        // ReSharper disable once CppUnreachableCode
        return false;
    }
    logger::info("Number of threads: {}", numThreads);
    return true;
}
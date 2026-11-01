#include "Startup.h"
#include "Serialization.h"
#include "Logger.h"
#include "Threading.h"



SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse, false);
    InitializeSerialization();
    Startup::Install(skse);
    logger::info("Number of threads: {}", numThreads);
    return true;
}
#pragma once
#include <windows.h>
#include <TraceLoggingProvider.h>
#include <chrono>

// Provider name + GUID must match profiler/Contract.h and EtwSession.cpp exactly (shell's only profiler coupling)
TRACELOGGING_DECLARE_PROVIDER(g_traceProvider);

namespace trace
{
    void Register();
    void Unregister();

    inline long long NowUs()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }
}

#define TRACE_EVENT(name, ...) TraceLoggingWrite(g_traceProvider, name, __VA_ARGS__)

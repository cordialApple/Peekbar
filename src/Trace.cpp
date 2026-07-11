#include "Trace.h"

#pragma comment(lib, "advapi32.lib")

// GUID must match profiler/EtwSession.cpp; do not hand-edit without re-deriving both sides
TRACELOGGING_DEFINE_PROVIDER(
    g_traceProvider,
    "Peekbar.Perf",
    (0x2d97e6b8, 0x5943, 0x5fec, 0x37, 0xa8, 0xa9, 0x22, 0x7d, 0xc5, 0x25, 0xa3));

namespace trace
{
    void Register()   { TraceLoggingRegister(g_traceProvider); }
    void Unregister()  { TraceLoggingUnregister(g_traceProvider); }
}

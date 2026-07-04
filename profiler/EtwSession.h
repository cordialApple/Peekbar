#pragma once

#include <windows.h>
#include <evntrace.h>

#include <functional>
#include <string>
#include <thread>
#include <vector>

// A decoded TraceLogging event handed to the sink.
struct DecodedEvent {
    std::wstring name;
    long long durationUs = -1;  // value of the duration_us field, or -1 if absent
    bool hasDuration = false;
    // All top-level fields rendered "name=value", for raw stdout printing.
    std::vector<std::pair<std::wstring, std::wstring>> fields;
};

using EventSink = std::function<void(const DecodedEvent&)>;

// Derive the TraceLogging provider GUID from its name (EventSource/TraceLogging
// convention: SHA-1 over a fixed namespace + the upper-cased UTF-16BE name).
GUID ProviderGuidFromName(const std::wstring& name);

// Real-time ETW consumer for one TraceLogging provider, subscribed by
// name-derived GUID. Owns a private real-time session for its lifetime and
// guarantees the session is stopped on Stop()/destruction — never leaked.
class EtwSession {
public:
    EtwSession(std::wstring sessionName, std::wstring providerName);
    ~EtwSession();

    EtwSession(const EtwSession&) = delete;
    EtwSession& operator=(const EtwSession&) = delete;

    // Start the session + enable the provider + open the trace. Returns a
    // Win32 error (ERROR_SUCCESS on success). Transparently recovers from a
    // stale same-named session (ERROR_ALREADY_EXISTS → stop → retry once).
    DWORD Start();

    // Run ProcessTrace on a background thread, delivering decoded events to
    // sink until Stop() is called. Non-blocking.
    void Consume(EventSink sink);

    // Stop consumption and tear down the session. Idempotent; also invoked by
    // the destructor so no exit path leaks the ETW session.
    void Stop();

    const GUID& ProviderGuid() const { return m_providerGuid; }

private:
    static void WINAPI EventRecordThunk(PEVENT_RECORD record);
    void OnEvent(PEVENT_RECORD record);
    DWORD StartSessionOnce();

    std::wstring m_sessionName;
    std::wstring m_providerName;
    GUID m_providerGuid{};
    TRACEHANDLE m_sessionHandle = 0;  // controller handle from StartTraceW
    TRACEHANDLE m_traceHandle = INVALID_PROCESSTRACE_HANDLE;
    std::vector<BYTE> m_props;  // backing store for EVENT_TRACE_PROPERTIES
    EventSink m_sink;
    std::thread m_consumer;
    bool m_stopped = false;
};

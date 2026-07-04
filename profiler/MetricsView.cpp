#include "MetricsView.h"

#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace {

ULONGLONG ToU64(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

double PercentileSorted(const std::vector<double>& v, double pct) {
    if (v.empty())
        return 0.0;
    size_t idx = static_cast<size_t>(pct * (v.size() - 1) + 0.5);
    if (idx >= v.size())
        idx = v.size() - 1;
    return v[idx];
}

DWORD FindPidByImage(const std::wstring& image) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, image.c_str()) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

}  // namespace

MetricsView::MetricsView(std::wstring shellImageName, std::wstring csvPath,
                         std::wstring providerName)
    : m_imageName(std::move(shellImageName)),
      m_csvPath(std::move(csvPath)),
      m_providerName(std::move(providerName)) {}

MetricsView::~MetricsView() {
    if (m_proc)
        CloseHandle(m_proc);
    if (m_csv)
        fclose(m_csv);
}

void MetricsView::Ingest(const DecodedEvent& ev) {
    std::lock_guard<std::mutex> lk(m_mu);
    Agg& a = m_aggs[ev.name];
    ++a.total;
    ++a.interval;
    if (ev.hasDuration && ev.durationUs >= 0)
        a.durations.push_back(static_cast<double>(ev.durationUs));
}

MetricsView::ProcSample MetricsView::SampleProcess(double intervalSec) {
    ProcSample s;
    if (!m_proc || WaitForSingleObject(m_proc, 0) == WAIT_OBJECT_0) {
        if (m_proc) {
            CloseHandle(m_proc);
            m_proc = nullptr;
            m_havePrev = false;
        }
        m_pid = FindPidByImage(m_imageName);
        if (m_pid)
            m_proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, m_pid);
    }
    if (!m_proc)
        return s;
    s.found = true;

    FILETIME c, e, k, u;
    if (GetProcessTimes(m_proc, &c, &e, &k, &u)) {
        ULONGLONG now = ToU64(k) + ToU64(u);
        if (m_havePrev && intervalSec > 0) {
            double busy100ns = static_cast<double>(now - m_prevProcTime100ns);
            double cpus = static_cast<double>(std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
            s.cpuPct = (busy100ns / 1.0e7) / (intervalSec * cpus) * 100.0;
        }
        m_prevProcTime100ns = now;
        m_havePrev = true;
    }
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(m_proc, &pmc, sizeof(pmc)))
        s.workingSetKB = pmc.WorkingSetSize / 1024;
    GetProcessHandleCount(m_proc, &s.handles);
    return s;
}

void MetricsView::OpenCsvIfNeeded() {
    if (m_csvPath.empty() || m_csv)
        return;
    if (_wfopen_s(&m_csv, m_csvPath.c_str(), L"w") != 0 || !m_csv) {
        m_csv = nullptr;
        return;
    }
    fwprintf(m_csv, L"time,event,count,rate_s,p50_us,p95_us,max_us,cpu_pct,ws_kb,handles\n");
}

void MetricsView::Tick(double intervalSec) {
    std::map<std::wstring, Agg> snapshot;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        snapshot = m_aggs;
        for (auto& kv : m_aggs) {
            kv.second.interval = 0;
            kv.second.durations.clear();
        }
    }

    ProcSample ps = SampleProcess(intervalSec);
    OpenCsvIfNeeded();

    SYSTEMTIME st;
    GetLocalTime(&st);

    system("cls");
    wprintf(L"shell_profiler — provider %s   %02d:%02d:%02d\n",
            m_providerName.c_str(), st.wHour, st.wMinute, st.wSecond);
    if (ps.found)
        wprintf(L"shell (%s pid %lu): CPU %.1f%%  WS %llu KB  handles %lu\n\n",
                m_imageName.c_str(), m_pid, ps.cpuPct,
                static_cast<unsigned long long>(ps.workingSetKB), ps.handles);
    else
        wprintf(L"shell (%s): not running\n\n", m_imageName.c_str());

    wprintf(L"%-18s %8s %8s %10s %10s %10s\n", L"event", L"count", L"rate/s",
            L"p50_us", L"p95_us", L"max_us");
    wprintf(L"%s\n", L"------------------------------------------------------------------------");

    if (snapshot.empty())
        wprintf(L"(no events yet — is the shell emitting %s?)\n", m_providerName.c_str());

    for (auto& kv : snapshot) {
        Agg& a = kv.second;
        double rate = a.interval / (intervalSec > 0 ? intervalSec : 1.0);
        std::sort(a.durations.begin(), a.durations.end());
        double p50 = PercentileSorted(a.durations, 0.50);
        double p95 = PercentileSorted(a.durations, 0.95);
        double mx = a.durations.empty() ? 0.0 : a.durations.back();
        wprintf(L"%-18s %8llu %8.1f %10.1f %10.1f %10.1f\n", kv.first.c_str(),
                a.total, rate, p50, p95, mx);
        if (m_csv) {
            fwprintf(m_csv, L"%02d:%02d:%02d,%s,%llu,%.1f,%.1f,%.1f,%.1f,%.1f,%llu,%lu\n",
                     st.wHour, st.wMinute, st.wSecond, kv.first.c_str(), a.total, rate,
                     p50, p95, mx, ps.cpuPct,
                     static_cast<unsigned long long>(ps.workingSetKB), ps.handles);
        }
    }
    if (m_csv)
        fflush(m_csv);
}

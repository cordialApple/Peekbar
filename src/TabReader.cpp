#include "TabReader.h"
#include "TabHop.h"
#include "Trace.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <chrono>

using Microsoft::WRL::ComPtr;

namespace
{

// 15ms: worker thread has no message pump for WinEventHook; tighter poll ceiling caps latency at 15ms vs 50ms.
constexpr int kPollIntervalMs  = 15;
constexpr int kReadyTimeoutMs  = 3000;   // window visible && !iconic

static long long NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Sleep that observes the worker's stop flag so teardown (join on the UI thread)
// is never blocked for a full retry budget. Returns true if a stop was requested.
static bool CancelableSleep(const std::atomic<bool>& stop, int ms)
{
    if (stop.load()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return stop.load();
}

static const wchar_t* OutcomeName(ActivateOutcome o)
{
    switch (o)
    {
    case ActivateOutcome::Selected: return L"Selected";
    case ActivateOutcome::Failed:   return L"Failed";
    }
    return L"Unknown";
}

static bool EndsWith(const std::wstring& s, const wchar_t* suffix)
{
    const size_t n = wcslen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// Edge appends status/telemetry suffixes to tab TabItem names, e.g.
// "<title> - Pinned - Sleeping - Memory usage - 103 MB". Strip them.
static std::wstring CleanTabTitle(std::wstring s)
{
    const size_t mu = s.rfind(L" - Memory usage");
    if (mu != std::wstring::npos)
        s.erase(mu);
    for (bool changed = true; changed; )
    {
        changed = false;
        for (const wchar_t* suf : { L" - Sleeping", L" - Pinned" })
            if (EndsWith(s, suf)) { s.erase(s.size() - wcslen(suf)); changed = true; }
    }
    return s;
}

// Walk up the parent chain looking for a Document control type.
// Browser chrome elements are never inside a document; web content always is.
static bool IsInsideDocument(IUIAutomation* automation, IUIAutomationElement* startElem)
{
    ComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(automation->get_RawViewWalker(&walker)) || !walker) return false;

    ComPtr<IUIAutomationElement> current;
    startElem->QueryInterface(IID_PPV_ARGS(&current));

    for (int depth = 0; depth < 30 && current; ++depth)
    {
        ComPtr<IUIAutomationElement> parent;
        if (FAILED(walker->GetParentElement(current.Get(), &parent)) || !parent) break;

        CONTROLTYPEID ct = 0;
        if (SUCCEEDED(parent->get_CurrentControlType(&ct)) &&
            ct == UIA_DocumentControlTypeId)
            return true;

        current = std::move(parent);
    }
    return false;
}

std::vector<Tab> SnapshotTabs(IUIAutomation* automation, HWND hwnd)
{
    ComPtr<IUIAutomationElement> elem;
    if (FAILED(automation->ElementFromHandle(hwnd, &elem)) || !elem) return {};

    ComPtr<IUIAutomationCacheRequest> cacheReq;
    if (FAILED(automation->CreateCacheRequest(&cacheReq))) return {};
    cacheReq->AddProperty(UIA_NamePropertyId);
    cacheReq->AddProperty(UIA_SelectionItemIsSelectedPropertyId);

    VARIANT vt = {};
    vt.vt   = VT_I4;
    vt.lVal = UIA_TabControlTypeId;
    ComPtr<IUIAutomationCondition> tabCtrlCond;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vt, &tabCtrlCond)))
        return {};

    ComPtr<IUIAutomationElementArray> tabCtrls;
    if (FAILED(elem->FindAll(TreeScope_Descendants, tabCtrlCond.Get(), &tabCtrls)) || !tabCtrls)
        return {};

    int ctrlCount = 0;
    tabCtrls->get_Length(&ctrlCount);

    vt.lVal = UIA_TabItemControlTypeId;
    ComPtr<IUIAutomationCondition> tabItemCond;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vt, &tabItemCond)))
        return {};

    for (int ci = 0; ci < ctrlCount; ++ci)
    {
        ComPtr<IUIAutomationElement> tabCtrl;
        if (FAILED(tabCtrls->GetElement(ci, &tabCtrl)) || !tabCtrl) continue;

        if (IsInsideDocument(automation, tabCtrl.Get())) continue;

        ComPtr<IUIAutomationElementArray> items;
        if (FAILED(tabCtrl->FindAllBuildCache(TreeScope_Descendants, tabItemCond.Get(),
                                               cacheReq.Get(), &items)) || !items)
            continue;

        int count = 0;
        items->get_Length(&count);
        if (count == 0) continue;

        std::vector<Tab> tabs;
        tabs.reserve(count);
        bool sawActive = false;
        for (int i = 0; i < count; ++i)
        {
            ComPtr<IUIAutomationElement> item;
            if (FAILED(items->GetElement(i, &item)) || !item) continue;
            BSTR name = nullptr;
            if (SUCCEEDED(item->get_CachedName(&name)) && name)
            {
                std::wstring title = CleanTabTitle(name);
                if (!title.empty())
                {
                    bool active = false;
                    VARIANT sel = {};
                    if (SUCCEEDED(item->GetCachedPropertyValue(
                            UIA_SelectionItemIsSelectedPropertyId, &sel)))
                        active = (sel.vt == VT_BOOL && sel.boolVal != VARIANT_FALSE);
                    VariantClear(&sel);
                    if (active && sawActive) active = false;  // one active tab max
                    sawActive = sawActive || active;
                    tabs.push_back({ std::move(title), active });
                }
                SysFreeString(name);
            }
        }
        if (!tabs.empty())
            return tabs;
    }
    return {};
}

// Tab activation via keystroke ring-hop. PlanTabHops returns the minimal ring-hop sequence
// (direct Ctrl+digit, relative Ctrl+PgUp/PgDn walk, or jump+walk); this maps each Hop to its
// VK, and KeystrokeHop fires the whole sequence as one batched SendInput.
static WORD HopVk(const Hop& h)
{
    switch (h.kind)
    {
    case HopKind::Next:      return VK_NEXT;
    case HopKind::Prev:      return VK_PRIOR;
    case HopKind::JumpDigit: return static_cast<WORD>('0' + h.digit);
    case HopKind::JumpLast:  return '9';
    }
    return 0;
}

static TabActivateResult KeystrokeHop(HWND hwnd, int activeIndex, int targetIndex, int tabCount,
                                      const std::atomic<bool>& stop, HANDLE fgReadyEvent,
                                      const std::atomic<uint64_t>& latestGen, uint64_t myGen,
                                      long long tClickUs, long long tRestoreUs)
{
    TabActivateResult r{ hwnd, ActivateOutcome::Failed, -1 };

    long long tReadyUs = 0, tDoneUs = 0;
    int hopCount = 0, usedJump = 0;
    auto Finish = [&]() -> TabActivateResult {
        const long long tEndUs = tDoneUs ? tDoneUs : trace::NowUs();
        TRACE_EVENT("KeystrokeHopLatency",
            TraceLoggingWideString(OutcomeName(r.outcome), "outcome"),
            TraceLoggingInt32(activeIndex, "active_index"),
            TraceLoggingInt32(targetIndex, "target_index"),
            TraceLoggingInt32(tabCount, "tab_count"),
            TraceLoggingInt32(hopCount, "hop_count"),
            TraceLoggingInt32(usedJump, "used_jump"),
            TraceLoggingInt64(tRestoreUs - tClickUs, "us_click_to_restore"),
            TraceLoggingInt64(tReadyUs ? tReadyUs - tRestoreUs : -1, "us_restore_to_ready"),
            TraceLoggingInt64((tDoneUs && tReadyUs) ? tDoneUs - tReadyUs : -1, "us_ready_to_done"),
            TraceLoggingInt64(tEndUs - tClickUs, "duration_us"));
        return std::move(r);
    };

    // Injected input lands on the foreground window: gate until the target actually IS foreground.
    // Event-driven: WinEventProc SetEvents fgReadyEvent on EVENT_SYSTEM_FOREGROUND; the wait wakes
    // then, or after kPollIntervalMs as a safety ceiling (missed/in-place events), or on teardown.
    const long long t0 = NowMs();
    bool ready = false;
    while (IsWindow(hwnd) && NowMs() - t0 < kReadyTimeoutMs)
    {
        // A newer click superseded this hop: abandon now instead of stalling the queue on a window
        // the user already navigated away from (outcome stays Failed => no optimistic cache write).
        if (latestGen.load(std::memory_order_relaxed) != myGen) return Finish();
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetForegroundWindow() == hwnd) { ready = true; break; }
        if (stop.load()) return Finish();
        if (fgReadyEvent) WaitForSingleObject(fgReadyEvent, kPollIntervalMs);
        else if (CancelableSleep(stop, kPollIntervalMs)) return Finish();   // event-create failed: real sleep, no busy-spin
    }
    if (!ready) return Finish();
    tReadyUs = trace::NowUs();

    const bool activeKnown = activeIndex >= 0;
    const std::vector<Hop> hops = PlanTabHops(
        activeKnown ? activeIndex + 1 : 1, targetIndex + 1, tabCount, activeKnown);
    hopCount = static_cast<int>(hops.size());

    // Final gate before committing the optimistic Selected: re-verify the target is still foreground
    // and this is still the newest hop. Narrows (cannot fully close) the check->SendInput TOCTOU vs
    // OS-global foreground, and also guards the no-key case (already on target) from a stale write.
    if (GetForegroundWindow() != hwnd || latestGen.load(std::memory_order_relaxed) != myGen)
        return Finish();   // outcome stays Failed -> no optimistic cache write

    r.matchedIndex = targetIndex;
    r.outcome = ActivateOutcome::Selected;   // optimistic: this path never confirms via UIA

    if (!hops.empty())
    {
        std::vector<INPUT> seq;
        seq.reserve(hops.size() * 2 + 2);
        auto key = [&](WORD vk, DWORD flags) {
            INPUT e{};
            e.type = INPUT_KEYBOARD;
            e.ki.wVk = vk;
            e.ki.dwFlags = flags;
            seq.push_back(e);
        };

        key(VK_CONTROL, 0);
        for (const Hop& h : hops)
        {
            if (h.kind == HopKind::JumpDigit || h.kind == HopKind::JumpLast) usedJump = 1;
            const WORD vk = HopVk(h);
            key(vk, 0);
            key(vk, KEYEVENTF_KEYUP);
        }
        key(VK_CONTROL, KEYEVENTF_KEYUP);

        SendInput(static_cast<UINT>(seq.size()), seq.data(), sizeof(INPUT));
    }

    tDoneUs = trace::NowUs();
    return Finish();
}

} // namespace

TabReader::TabReader(HWND dockHwnd, UINT snapshotMsg, UINT activateMsg)
    : m_dockHwnd(dockHwnd), m_snapshotMsg(snapshotMsg), m_activateMsg(activateMsg),
      m_exit(std::make_shared<ExitSignal>())
{
    m_fgReadyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_thread = std::thread([this, exit = m_exit] {
        WorkerLoop();
        {
            std::lock_guard<std::mutex> lk(exit->m);
            exit->exited = true;
        }
        exit->cv.notify_all();
    });
}

TabReader::~TabReader()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stop = true;
    }
    m_cv.notify_one();
    if (m_fgReadyEvent) SetEvent(m_fgReadyEvent);   // wake a worker parked in the readiness gate

    // m_stop only bounds sleeps; cannot interrupt wedged UIA/COM calls. Bounded-join: if call hangs after timeout, detach (shutdown-only UB).
    bool exited = false;
    {
        std::unique_lock<std::mutex> lk(m_exit->m);
        exited = m_exit->cv.wait_for(lk, std::chrono::seconds(2),
                                     [this] { return m_exit->exited; });
    }
    if (exited)
    {
        m_thread.join();
        if (m_fgReadyEvent) { CloseHandle(m_fgReadyEvent); m_fgReadyEvent = nullptr; }
    }
    else if (m_thread.joinable())
        m_thread.detach();   // detached worker may still touch m_fgReadyEvent; leak it (process exiting)
}

void TabReader::NotifyForeground()
{
    if (m_fgReadyEvent) SetEvent(m_fgReadyEvent);
}

void TabReader::RequestSnapshot(HWND hwnd)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stop) return;
        for (const Request& req : m_queue)
            if (req.kind == ReqKind::Snapshot && req.hwnd == hwnd) return;
        m_queue.push_back({ ReqKind::Snapshot, hwnd });
    }
    m_cv.notify_one();
}

void TabReader::RequestKeystrokeHop(HWND hwnd, int activeIndex, int targetIndex, int tabCount,
                                    long long tClickUs, long long tRestoreUs)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stop) return;
        // Single-flight: a hop is only ever worth the newest click. Drop any queued-but-unstarted
        // hops and bump the generation so an in-flight one abandons itself (see the readiness gate).
        m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                          [](const Request& r) { return r.kind == ReqKind::KeystrokeHop; }),
                      m_queue.end());
        Request req{ ReqKind::KeystrokeHop, hwnd, tClickUs, tRestoreUs,
                     targetIndex, tabCount, activeIndex };
        req.gen = m_hopGen.fetch_add(1, std::memory_order_relaxed) + 1;
        m_queue.push_back(std::move(req));
    }
    m_cv.notify_one();
    if (m_fgReadyEvent) SetEvent(m_fgReadyEvent);   // wake a parked worker so it re-checks generation now
}

void TabReader::WorkerLoop()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IUIAutomation> automation;
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&automation));

    while (true)
    {
        Request req;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this] { return m_stop.load() || !m_queue.empty(); });
            if (m_stop) break;
            req = std::move(m_queue.front());
            m_queue.pop_front();
        }

        try
        {
            if (req.kind == ReqKind::Snapshot)
            {
                const long long tStartUs = trace::NowUs();
                std::vector<Tab> tabs;
                if (automation)
                    tabs = SnapshotTabs(automation.Get(), req.hwnd);

                const bool failed = tabs.empty();
                TRACE_EVENT("UiaSnapshot",
                    TraceLoggingInt64(trace::NowUs() - tStartUs, "duration_us"),
                    TraceLoggingPointer(req.hwnd, "hwnd"),
                    TraceLoggingInt32(static_cast<int32_t>(tabs.size()), "tab_count"),
                    TraceLoggingInt32(automation ? (failed ? E_FAIL : S_OK) : E_HANDLE, "hr"));
                auto* payload = new TabSnapshot{ req.hwnd, std::move(tabs), failed };
                if (!PostMessageW(m_dockHwnd, m_snapshotMsg,
                                  reinterpret_cast<WPARAM>(req.hwnd),
                                  reinterpret_cast<LPARAM>(payload)))
                    delete payload;
            }
            else  // ReqKind::KeystrokeHop
            {
                TabActivateResult result = KeystrokeHop(req.hwnd, req.activeIndex, req.targetIndex,
                                                        req.tabCount, m_stop, m_fgReadyEvent,
                                                        m_hopGen, req.gen, req.tClickUs, req.tRestoreUs);
                auto* payload = new TabActivateResult(std::move(result));
                if (!PostMessageW(m_dockHwnd, m_activateMsg,
                                  reinterpret_cast<WPARAM>(req.hwnd),
                                  reinterpret_cast<LPARAM>(payload)))
                    delete payload;
            }
        }
        catch (...) {}
    }

    CoUninitialize();
}

#include "TabReader.h"
#include "Trace.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <cwchar>
#include <string>
#include <chrono>

using Microsoft::WRL::ComPtr;

namespace
{

// Activation retry budget (R2). Restore is async; Select on a still-iconic or
// mid-animation window returns S_OK but SILENTLY no-ops. Poll readiness, then the
// (async-rebuilt) tab tree, then confirm the selection actually took.
// 15ms (not event-driven — see ActivateTab comment): the worker thread has no
// message pump, so a WinEventHook here can't fire; that fix needs a UI-thread
// hook + a new signal path, deferred. Cheap interim win: tighter poll ceiling
// caps Gate 1/2 worst-case added latency at 15ms instead of 50ms.
constexpr int kPollIntervalMs  = 15;
constexpr int kReadyTimeoutMs  = 3000;   // window visible && !iconic
constexpr int kTreeTimeoutMs   = 3000;   // TabControl with TabItems present
constexpr int kConfirmSettleMs = 60;     // settle before re-reading IsSelected

// Backstop against a runaway/cyclic OR pathologically wide tree in
// FindTabControlsGuided — browser chrome (toolbar, tab strip) is a handful of
// levels deep and a few dozen nodes wide in practice; this is generous headroom,
// not a tuned limit. Depth alone doesn't bound cost: a wide, shallow, non-Document
// sibling fan-out isn't pruned by a depth cap and would otherwise turn one
// FindLiveTabItems call into unboundedly many cross-process FindAll calls.
constexpr int kGuidedDescentMaxDepth = 25;
constexpr int kGuidedDescentMaxNodes = 500;

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
    case ActivateOutcome::Selected:           return L"Selected";
    case ActivateOutcome::NoMatch:            return L"NoMatch";
    case ActivateOutcome::PatternUnavailable: return L"PatternUnavailable";
    case ActivateOutcome::Failed:             return L"Failed";
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

// Depth + total-nodes-visited bound, and whether any per-node COM call failed or a
// bound was hit — see FindTabControlsGuided. A non-empty candidate list from a
// truncated walk cannot be trusted as complete (the real tab strip could be in the
// part that was cut off), so the caller must fall back to the blanket search rather
// than silently proceeding on a partial result.
struct GuidedDescentState
{
    int  visited   = 0;
    bool truncated = false;
};

// Pruned search for TabControl elements via TreeScope_Children recursion instead of
// one TreeScope_Descendants FindAll — never descends into a Document subtree (see
// activatetab-restore-to-tabfound-bottleneck debt: live capture showed the blanket
// Descendants search dominates FindLiveTabItems's cost and scales with how much is
// open, almost certainly because it also walks the current tab's DOM-backed
// web-content accessibility tree just to discard it afterward via IsInsideDocument).
// Browser chrome is never inside a Document and web content always is, so pruning at
// the Document boundary while descending is the mirror image of IsInsideDocument's
// existing ascending check — in the common case this means fewer candidates to
// re-check, not a replacement for the check: every candidate still goes through the
// same IsInsideDocument recheck the blanket path always used (see FindLiveTabItems),
// which is the actual safety backstop, not this function's pruning alone.
static void FindTabControlsGuided(IUIAutomationElement* node, IUIAutomationCondition* trueCond,
                                   int depth, std::vector<ComPtr<IUIAutomationElement>>& outCandidates,
                                   GuidedDescentState& state)
{
    if (depth > kGuidedDescentMaxDepth) { state.truncated = true; return; }

    ComPtr<IUIAutomationElementArray> children;
    if (FAILED(node->FindAll(TreeScope_Children, trueCond, &children)) || !children)
    {
        state.truncated = true;
        return;
    }

    int count = 0;
    children->get_Length(&count);
    for (int i = 0; i < count; ++i)
    {
        if (state.visited >= kGuidedDescentMaxNodes) { state.truncated = true; return; }
        ++state.visited;

        ComPtr<IUIAutomationElement> child;
        if (FAILED(children->GetElement(i, &child)) || !child) { state.truncated = true; continue; }

        CONTROLTYPEID ct = 0;
        if (FAILED(child->get_CurrentControlType(&ct))) { state.truncated = true; continue; }
        if (ct == UIA_DocumentControlTypeId) continue;   // prune: web content, never the tab strip

        if (ct == UIA_TabControlTypeId) outCandidates.push_back(child);

        FindTabControlsGuided(child.Get(), trueCond, depth + 1, outCandidates, state);
    }
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

        // Skip web-content tab controls — they live inside a Document node.
        if (IsInsideDocument(automation, tabCtrl.Get())) continue;

        // TabItems are nested inside layout panes, not direct children — use Descendants.
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

// Confirm a tab is selected by re-reading IsSelected (Select() returns S_OK even
// on the silent-no-op failure mode). Worker thread, UIA only.
static bool IsItemSelected(IUIAutomationElement* item)
{
    VARIANT v = {};
    bool sel = false;
    if (SUCCEEDED(item->GetCurrentPropertyValue(UIA_SelectionItemIsSelectedPropertyId, &v)))
        sel = (v.vt == VT_BOOL && v.boolVal != VARIANT_FALSE);
    VariantClear(&v);
    return sel;
}

// Diagnostic breakdown of one FindLiveTabItems call's internal UIA cost (see
// activatetab-restore-to-tabfound-bottleneck debt: settles whether the ~283ms
// gate-2 wait is the TabControl FindAll(Descendants) walk, the per-candidate
// document-exclusion parent-walk, or the TabItem FindAllBuildCache). The
// per-candidate fields accumulate across the inner ci loop into one scalar
// each (1-2 candidates typical) rather than a per-candidate vector.
struct WalkTiming
{
    long long usElementFromHandle = 0;
    long long usFindAllTabCtrls   = 0;   // guided descent, or the blanket-search fallback if it found nothing
    long long usIsInsideDocument  = 0;   // accrues every candidate, incl. continue'd ones
    long long usFindAllTabItems   = 0;   // accrues only when FindAllBuildCache actually runs
    int       tabCtrlCandidates   = 0;   // ctrlCount
    bool      guidedDescentUsed   = false;
};

// Like SnapshotTabs but KEEPS the live TabItem element array (elements are not
// durable across restore, so activation must re-walk fresh and act immediately).
// Names/IsSelected are read from a single FindAllBuildCache round trip (mirrors
// SnapshotTabs) instead of one GetCurrentName+GetCurrentPropertyValue pair per
// item — was up to 2N+1 cross-process COM calls on the click-to-activate path,
// now one. BuildCache only prefetches properties; the returned elements are
// still live and safe to Select()/SetFocus() afterward.
// Returns S_OK with a non-empty array + parallel tabs, or E_FAIL if no tab tree yet.
static HRESULT FindLiveTabItems(IUIAutomation* automation, HWND hwnd,
                                ComPtr<IUIAutomationElementArray>& outItems,
                                std::vector<Tab>& outTabs,
                                WalkTiming& timing)
{
    outItems.Reset();
    outTabs.clear();
    timing = WalkTiming{};

    long long tStartUs = trace::NowUs();
    ComPtr<IUIAutomationElement> elem;
    if (FAILED(automation->ElementFromHandle(hwnd, &elem)) || !elem) return E_FAIL;
    timing.usElementFromHandle = trace::NowUs() - tStartUs;

    ComPtr<IUIAutomationCacheRequest> cacheReq;
    if (FAILED(automation->CreateCacheRequest(&cacheReq))) return E_FAIL;
    cacheReq->AddProperty(UIA_NamePropertyId);
    cacheReq->AddProperty(UIA_SelectionItemIsSelectedPropertyId);

    VARIANT vt = {};
    vt.vt   = VT_I4;
    vt.lVal = UIA_TabControlTypeId;
    ComPtr<IUIAutomationCondition> tabCtrlCond;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vt, &tabCtrlCond)))
        return E_FAIL;

    long long tFindCtrlsUs = trace::NowUs();
    std::vector<ComPtr<IUIAutomationElement>> guidedCandidates;
    ComPtr<IUIAutomationCondition> trueCond;
    GuidedDescentState guidedState;
    if (SUCCEEDED(automation->CreateTrueCondition(&trueCond)) && trueCond)
        FindTabControlsGuided(elem.Get(), trueCond.Get(), 0, guidedCandidates, guidedState);

    // Fallback: guided descent found nothing, OR its walk was truncated (depth/node
    // cap hit, or any per-node COM call failed) — a truncated walk's candidate list
    // cannot be trusted as complete even if non-empty, since the real tab strip could
    // be in the part that got cut off. Falls back to the original blanket search in
    // this same call, so correctness never regresses versus the pre-guided-descent
    // code (never silently miss the real tab strip).
    ComPtr<IUIAutomationElementArray> tabCtrls;
    const bool usedGuided = !guidedCandidates.empty() && !guidedState.truncated;
    int ctrlCount = 0;
    if (usedGuided)
    {
        ctrlCount = static_cast<int>(guidedCandidates.size());
    }
    else
    {
        if (FAILED(elem->FindAll(TreeScope_Descendants, tabCtrlCond.Get(), &tabCtrls)) || !tabCtrls)
            return E_FAIL;
        tabCtrls->get_Length(&ctrlCount);
    }
    timing.usFindAllTabCtrls = trace::NowUs() - tFindCtrlsUs;
    timing.tabCtrlCandidates = ctrlCount;
    timing.guidedDescentUsed = usedGuided;

    vt.lVal = UIA_TabItemControlTypeId;
    ComPtr<IUIAutomationCondition> tabItemCond;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, vt, &tabItemCond)))
        return E_FAIL;

    for (int ci = 0; ci < ctrlCount; ++ci)
    {
        ComPtr<IUIAutomationElement> tabCtrl;
        if (usedGuided) tabCtrl = guidedCandidates[ci];
        else if (FAILED(tabCtrls->GetElement(ci, &tabCtrl)) || !tabCtrl) continue;

        long long tInsideDocUs = trace::NowUs();
        const bool insideDoc = IsInsideDocument(automation, tabCtrl.Get());
        timing.usIsInsideDocument += trace::NowUs() - tInsideDocUs;
        if (insideDoc) continue;

        ComPtr<IUIAutomationElementArray> items;
        long long tFindItemsUs = trace::NowUs();
        const HRESULT hrItems = tabCtrl->FindAllBuildCache(TreeScope_Descendants, tabItemCond.Get(),
                                                            cacheReq.Get(), &items);
        timing.usFindAllTabItems += trace::NowUs() - tFindItemsUs;
        if (FAILED(hrItems) || !items) continue;

        int count = 0;
        items->get_Length(&count);
        if (count == 0) continue;

        std::vector<Tab> tabs;
        tabs.reserve(count);
        bool sawActive = false;
        for (int i = 0; i < count; ++i)
        {
            ComPtr<IUIAutomationElement> item;
            std::wstring title;
            bool active = false;
            if (SUCCEEDED(items->GetElement(i, &item)) && item)
            {
                BSTR name = nullptr;
                if (SUCCEEDED(item->get_CachedName(&name)) && name)
                {
                    title = CleanTabTitle(name);
                    SysFreeString(name);
                }
                VARIANT sel = {};
                if (SUCCEEDED(item->GetCachedPropertyValue(
                        UIA_SelectionItemIsSelectedPropertyId, &sel)))
                    active = (sel.vt == VT_BOOL && sel.boolVal != VARIANT_FALSE);
                VariantClear(&sel);
            }
            if (active && sawActive) active = false;
            sawActive = sawActive || active;
            tabs.push_back({ std::move(title), active });   // keep index parity with items
        }

        outItems = std::move(items);
        outTabs  = std::move(tabs);
        return S_OK;
    }
    return E_FAIL;
}

// Activate a specific tab in an already-restoring window. Never selects a wrong
// tab: title-first match, fallbackIndex only breaks ties among title matches, a
// bare index never selects alone. Restore is driven by the UI thread; here we only
// gate on readiness, act, and confirm. matchedIndex.active is set on success.
//
// tClickUs/tRestoreUs are the A/C timestamps handed down from the UI thread. Finish()
// emits ONE combined FanActivateLatency event on every exit path (success or bail),
// reusing timestamps captured at points this function already visits — no extra
// sleeps, polls, or COM calls added for telemetry alone. F ("first visible frame")
// is approximated as the moment IsItemSelected() re-confirms true after the existing
// settle sleep, not a true paint signal (see session discussion).
TabActivateResult ActivateTab(IUIAutomation* automation, HWND hwnd,
                              const std::wstring& wantedTitle, int fallbackIndex,
                              const std::atomic<bool>& stop,
                              long long tClickUs, long long tRestoreUs)
{
    TabActivateResult r{ hwnd, ActivateOutcome::Failed, -1, {} };

    long long tTabFoundUs = 0, tSelectAttemptUs = 0, tConfirmUs = 0;
    // Diagnostic split of us_restore_to_tabfound (see activatetab-restore-to-tabfound-
    // bottleneck debt): which gate the wait is in, how many polls it took, and whether
    // the UIA walk itself is slow (vs. many fast-failing polls while genuinely not
    // ready yet). tFirstWalkUs/tLastWalkUs bracket the SAME call each iteration —
    // first vs. last tells apart "walk is slow throughout" from "walk is slow only
    // once the tree is mid-rebuild".
    long long tReadyUs = 0;
    int gate1Attempts = 0, gate2Attempts = 0;
    long long tFirstWalkUs = -1, tLastWalkUs = -1;
    // Reported from the winning call only (overwritten every gate-2 iteration,
    // same pattern as tLastWalkUs — the last write before a break IS the winner's).
    WalkTiming lastWalkTiming;
    auto Finish = [&]() -> TabActivateResult
    {
        const long long tEndUs = tConfirmUs ? tConfirmUs : trace::NowUs();
        TRACE_EVENT("FanActivateLatency",
            TraceLoggingWideString(OutcomeName(r.outcome), "outcome"),
            TraceLoggingInt64(tRestoreUs - tClickUs, "us_click_to_restore"),
            TraceLoggingInt64(tTabFoundUs ? tTabFoundUs - tRestoreUs : -1, "us_restore_to_tabfound"),
            TraceLoggingInt64(tSelectAttemptUs ? tSelectAttemptUs - tTabFoundUs : -1, "us_tabfound_to_select"),
            TraceLoggingInt64(tConfirmUs ? tConfirmUs - tSelectAttemptUs : -1, "us_select_to_confirm"),
            TraceLoggingInt64(tEndUs - tClickUs, "duration_us"),
            TraceLoggingInt64(tReadyUs ? tReadyUs - tRestoreUs : -1, "us_gate1_wait"),
            TraceLoggingInt32(gate1Attempts, "gate1_attempts"),
            TraceLoggingInt64((tTabFoundUs && tReadyUs) ? tTabFoundUs - tReadyUs : -1, "us_gate2_wait"),
            TraceLoggingInt32(gate2Attempts, "gate2_attempts"),
            TraceLoggingInt64(tFirstWalkUs, "us_first_walk"),
            TraceLoggingInt64(tLastWalkUs, "us_last_walk"),
            TraceLoggingInt64(lastWalkTiming.usElementFromHandle, "us_element_from_handle"),
            TraceLoggingInt64(lastWalkTiming.usFindAllTabCtrls, "us_findall_tabctrls"),
            TraceLoggingInt64(lastWalkTiming.usIsInsideDocument, "us_is_inside_document"),
            TraceLoggingInt64(lastWalkTiming.usFindAllTabItems, "us_findall_tabitems"),
            TraceLoggingInt32(lastWalkTiming.tabCtrlCandidates, "tabctrl_candidates"),
            TraceLoggingInt32(lastWalkTiming.guidedDescentUsed ? 1 : 0, "guided_descent_used"));
        return std::move(r);
    };

    if (!automation) return Finish();

    const long long t0 = NowMs();

    // Gate 1: window readiness (restore is async). Bail fast if the window died
    // between the fan click and here (closed while the request was queued).
    bool ready = false;
    while (IsWindow(hwnd) && NowMs() - t0 < kReadyTimeoutMs)
    {
        ++gate1Attempts;
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd)) { ready = true; break; }
        if (CancelableSleep(stop, kPollIntervalMs)) return Finish();
    }
    if (!ready) return Finish();
    tReadyUs = trace::NowUs();

    // Gate 2: tab tree rebuilt (async after restore). Each iteration calls
    // FindLiveTabItems exactly once, timed, before the success check — so the walk
    // is never called twice and the sleep-on-failure below is untouched.
    ComPtr<IUIAutomationElementArray> items;
    std::vector<Tab> tabs;
    bool haveTree = false;
    while (NowMs() - t0 < kTreeTimeoutMs)
    {
        ++gate2Attempts;
        const long long tWalkStartUs = trace::NowUs();
        WalkTiming walkTiming;
        const bool walkOk = SUCCEEDED(FindLiveTabItems(automation, hwnd, items, tabs, walkTiming)) && items && !tabs.empty();
        const long long walkUs = trace::NowUs() - tWalkStartUs;
        if (tFirstWalkUs < 0) tFirstWalkUs = walkUs;
        tLastWalkUs = walkUs;
        lastWalkTiming = walkTiming;
        if (walkOk) { haveTree = true; break; }
        if (CancelableSleep(stop, kPollIntervalMs)) return Finish();
    }
    if (!haveTree) return Finish();
    tTabFoundUs = trace::NowUs();   // D: tab tree available to match against

    r.freshTabs = tabs;   // carry the re-snapshot back regardless of match outcome

    // Match: title-first, fallbackIndex tiebreak among matches only.
    int idx = -1;
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
        if (tabs[i].title == wantedTitle)
        {
            if (idx < 0) idx = i;
            if (i == fallbackIndex) idx = i;
        }
    if (idx < 0) { r.outcome = ActivateOutcome::NoMatch; return Finish(); }

    auto markSelected = [&] {
        r.matchedIndex = idx;
        r.outcome = ActivateOutcome::Selected;
        for (auto& t : r.freshTabs) t.active = false;
        if (idx < static_cast<int>(r.freshTabs.size())) r.freshTabs[idx].active = true;
    };

    ComPtr<IUIAutomationElement> item;
    if (FAILED(items->GetElement(idx, &item)) || !item) return Finish();

    // Select via SelectionItemPattern (live pattern — the action runs on the provider).
    ComPtr<IUIAutomationSelectionItemPattern> selPat;
    if (SUCCEEDED(item->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(&selPat)))
        && selPat)
    {
        selPat->Select();
    }
    else
    {
        r.outcome = ActivateOutcome::PatternUnavailable;
        // fall through to the SetFocus/Legacy fallback below
    }
    tSelectAttemptUs = trace::NowUs();   // E: activation attempted

    if (CancelableSleep(stop, kConfirmSettleMs)) return Finish();
    if (IsItemSelected(item.Get()))
    {
        tConfirmUs = trace::NowUs();   // F: activation-confirmed proxy
        markSelected();
        return Finish();
    }

    // Fallback chain (R2): SetFocus → LegacyIAccessible.DoDefaultAction. NOT Invoke.
    item->SetFocus();
    if (CancelableSleep(stop, kConfirmSettleMs)) return Finish();
    if (!IsItemSelected(item.Get()))
    {
        ComPtr<IUIAutomationLegacyIAccessiblePattern> legacy;
        if (SUCCEEDED(item->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId,
                                                IID_PPV_ARGS(&legacy))) && legacy)
        {
            legacy->DoDefaultAction();
            if (CancelableSleep(stop, kConfirmSettleMs)) return Finish();
        }
    }

    if (IsItemSelected(item.Get()))
    {
        tConfirmUs = trace::NowUs();   // F: activation-confirmed proxy (fallback path)
        markSelected();
    }
    else if (r.outcome != ActivateOutcome::PatternUnavailable)
    {
        r.outcome = ActivateOutcome::Failed;
    }
    return Finish();
}

} // namespace

TabReader::TabReader(HWND dockHwnd, UINT snapshotMsg, UINT activateMsg)
    : m_dockHwnd(dockHwnd), m_snapshotMsg(snapshotMsg), m_activateMsg(activateMsg),
      m_exit(std::make_shared<ExitSignal>())
{
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

    // m_stop only bounds the sleeps; it cannot interrupt an in-flight cross-process
    // UIA/COM call into a wedged browser provider — ActivateTab's Select/SetFocus/
    // DoDefaultAction OR SnapshotTabs/FindLiveTabItems' FindAll (F-02). A plain join()
    // would then stall WM_DESTROY forever, wedging process teardown. Bounded-join
    // instead: give an in-flight call a
    // moment to finish, else detach so teardown proceeds. Detach is shutdown-only: if
    // the call unhangs while the process is still tearing down (WM_DESTROY → message
    // loop exit, tens–hundreds of ms), the leaked worker touches freed members — UB,
    // but unobservable since the process is exiting.
    bool exited = false;
    {
        std::unique_lock<std::mutex> lk(m_exit->m);
        exited = m_exit->cv.wait_for(lk, std::chrono::seconds(2),
                                     [this] { return m_exit->exited; });
    }
    if (exited)
        m_thread.join();
    else if (m_thread.joinable())
        m_thread.detach();
}

void TabReader::RequestSnapshot(HWND hwnd)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stop) return;
        for (const Request& req : m_queue)
            if (req.kind == ReqKind::Snapshot && req.hwnd == hwnd) return;  // de-dupe snapshots only
        m_queue.push_back({ ReqKind::Snapshot, hwnd, {}, 0 });
    }
    m_cv.notify_one();
}

void TabReader::RequestActivate(HWND hwnd, std::wstring wantedTitle, int fallbackIndex,
                                long long tClickUs, long long tRestoreUs)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stop) return;
        m_queue.push_back({ ReqKind::Activate, hwnd, std::move(wantedTitle), fallbackIndex,
                            tClickUs, tRestoreUs });
    }
    m_cv.notify_one();
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
            else  // ReqKind::Activate
            {
                TabActivateResult result{ req.hwnd, ActivateOutcome::Failed, -1, {} };
                if (automation)
                    result = ActivateTab(automation.Get(), req.hwnd,
                                         req.wantedTitle, req.fallbackIndex, m_stop,
                                         req.tClickUs, req.tRestoreUs);

                auto* payload = new TabActivateResult(std::move(result));
                if (!PostMessageW(m_dockHwnd, m_activateMsg,
                                  reinterpret_cast<WPARAM>(req.hwnd),
                                  reinterpret_cast<LPARAM>(payload)))
                    delete payload;
            }
        }
        catch (...) {}  // bad_alloc etc. → skip iteration, continue loop
    }

    CoUninitialize();
}

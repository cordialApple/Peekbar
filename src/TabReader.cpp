#include "TabReader.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <cwchar>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{

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
        for (int i = 0; i < count; ++i)
        {
            ComPtr<IUIAutomationElement> item;
            if (FAILED(items->GetElement(i, &item)) || !item) continue;
            BSTR name = nullptr;
            if (SUCCEEDED(item->get_CachedName(&name)) && name)
            {
                std::wstring title = CleanTabTitle(name);
                if (!title.empty())
                    tabs.push_back({ std::move(title) });
                SysFreeString(name);
            }
        }
        if (!tabs.empty())
            return tabs;
    }
    return {};
}

} // namespace

TabReader::TabReader(HWND dockHwnd, UINT resultMsg)
    : m_dockHwnd(dockHwnd), m_resultMsg(resultMsg)
{
    m_thread = std::thread([this] { WorkerLoop(); });
}

TabReader::~TabReader()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stop = true;
    }
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
}

void TabReader::RequestSnapshot(HWND hwnd)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_stop) return;
        for (HWND h : m_queue)
            if (h == hwnd) return;
        m_queue.push_back(hwnd);
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
        HWND target;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this] { return m_stop.load() || !m_queue.empty(); });
            if (m_stop) break;
            target = m_queue.front();
            m_queue.pop_front();
        }

        try
        {
            std::vector<Tab> tabs;
            if (automation)
                tabs = SnapshotTabs(automation.Get(), target);

            const bool failed = tabs.empty();
            auto* payload = new TabSnapshot{ target, std::move(tabs), failed };
            if (!PostMessageW(m_dockHwnd, m_resultMsg,
                              reinterpret_cast<WPARAM>(target),
                              reinterpret_cast<LPARAM>(payload)))
                delete payload;
        }
        catch (...) {}  // bad_alloc etc. → skip iteration, continue loop
    }

    CoUninitialize();
}

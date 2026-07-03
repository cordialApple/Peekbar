#include "TabReader.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{

static bool IsWebContentTabName(const wchar_t* name)
{
    if (!name) return false;
    const wchar_t* p = name;
    if (p[0] != L't' || p[1] != L'a' || p[2] != L'b' || p[3] != L'-') return false;
    for (p += 4; *p; ++p)
        if (*p < L'0' || *p > L'9') return false;
    return p > name + 4;
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

    // FindAll so we can skip web-content tab panels (e.g. YouTube's <div role="tablist">
    // whose items get UIA names like "tab-0", "tab-1"). Browser chrome tabs come first
    // in tree order on most pages but not all.
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

        ComPtr<IUIAutomationElementArray> items;
        if (FAILED(tabCtrl->FindAllBuildCache(TreeScope_Children, tabItemCond.Get(),
                                               cacheReq.Get(), &items)) || !items)
            continue;

        int count = 0;
        items->get_Length(&count);
        if (count == 0) continue;

        // Check first item: if it looks like "tab-<digits>", this is web content.
        ComPtr<IUIAutomationElement> first;
        if (SUCCEEDED(items->GetElement(0, &first)) && first)
        {
            BSTR probe = nullptr;
            if (SUCCEEDED(first->get_CachedName(&probe)))
            {
                bool isWeb = IsWebContentTabName(probe);
                SysFreeString(probe);
                if (isWeb) continue;
            }
        }

        std::vector<Tab> tabs;
        tabs.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            ComPtr<IUIAutomationElement> item;
            if (FAILED(items->GetElement(i, &item)) || !item) continue;
            BSTR name = nullptr;
            if (SUCCEEDED(item->get_CachedName(&name)) && name)
            {
                tabs.push_back({ std::wstring(name) });
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

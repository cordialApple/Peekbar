#include "Store.h"
#include "Trace.h"
#include <algorithm>
#include <numeric>

void Store::TraceUpdate() const
{
    const long long totalTabs = std::accumulate(m_windows.begin(), m_windows.end(), 0LL,
        [](long long sum, const auto& kv) { return sum + static_cast<long long>(kv.second.tabs.size()); });
    TRACE_EVENT("StoreUpdate",
        TraceLoggingInt64(static_cast<long long>(m_windows.size()), "tracked_windows"),
        TraceLoggingInt64(totalTabs, "total_tabs"));
}

void Store::Set(HWND hwnd, std::wstring title)
{
    const bool isNew = m_windows.find(hwnd) == m_windows.end();
    auto& w   = m_windows[hwnd];
    w.hwnd    = hwnd;
    w.title   = std::move(title);
    if (isNew)
    {
        m_order.push_back(hwnd);
        TraceUpdate();
    }
}

void Store::SetMinimized(HWND hwnd, bool minimized)
{
    auto it = m_windows.find(hwnd);
    if (it != m_windows.end())
        it->second.minimized = minimized;
}

void Store::SetTabs(HWND hwnd, std::vector<Tab> tabs)
{
    auto it = m_windows.find(hwnd);
    if (it != m_windows.end())
    {
        it->second.tabs      = std::move(tabs);
        it->second.tabsStale = false;
        TraceUpdate();
    }
}

void Store::SetActiveTab(HWND hwnd, int index)
{
    auto it = m_windows.find(hwnd);
    if (it == m_windows.end()) return;
    auto& tabs = it->second.tabs;
    if (index < 0 || index >= static_cast<int>(tabs.size())) return;
    for (auto& t : tabs) t.active = false;
    tabs[index].active = true;
}

void Store::MarkTabsStale(HWND hwnd)
{
    auto it = m_windows.find(hwnd);
    if (it != m_windows.end())
        it->second.tabsStale = true;
}

void Store::Remove(HWND hwnd)
{
    m_windows.erase(hwnd);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), hwnd), m_order.end());
    TraceUpdate();
}

bool Store::Has(HWND hwnd) const
{
    return m_windows.count(hwnd) > 0;
}

#include "Store.h"
#include <algorithm>

void Store::Set(HWND hwnd, std::wstring title)
{
    const bool isNew = m_windows.find(hwnd) == m_windows.end();
    auto& w   = m_windows[hwnd];
    w.hwnd    = hwnd;
    w.title   = std::move(title);
    if (isNew) m_order.push_back(hwnd);
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
    }
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
}

bool Store::Has(HWND hwnd) const
{
    return m_windows.count(hwnd) > 0;
}

#include "DockWindow.h"

namespace
{
    constexpr wchar_t kClassName[] = L"BrowserShellOsDockWindow";

    // Debug rect for 1.2. Real size come from AppBar negotiation in 1.5.
    constexpr int kDebugWidth = 800;
    constexpr int kDebugHeight = 64;
}

bool DockWindow::Create(HINSTANCE instance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = StaticWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    // Center on primary monitor. Process per-monitor-v2 aware, so these
    // metrics = physical pixels of primary.
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screenW - kDebugWidth) / 2;
    const int y = (screenH - kDebugHeight) / 2;

    // WS_EX_TOOLWINDOW: no taskbar button, no Alt-Tab. WS_EX_TOPMOST: float.
    // WS_EX_NOACTIVATE: dock never take foreground on click.
    const HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClassName,
        L"browser_shell_os dock",
        WS_POPUP,
        x, y, kDebugWidth, kDebugHeight,
        nullptr, nullptr, instance,
        this);
    if (!hwnd)
    {
        return false;
    }

    // m_hwnd already set by WM_NCCREATE, but belt + suspenders.
    m_hwnd = hwnd;
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return true;
}

LRESULT CALLBACK DockWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // Messages before WM_NCCREATE (WM_GETMINMAXINFO, WM_NCCALCSIZE) come with
    // GWLP_USERDATA unset -> self==nullptr -> fall to DefWindowProcW. Keep guard.
    DockWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<DockWindow*>(cs->lpCreateParams);
        self->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<DockWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    // WndProc has no WM_NCCREATE arm -> DefWindowProcW return TRUE, creation
    // proceed. Never short-circuit WM_NCCREATE to 0 -> CreateWindow fail.
    if (self)
    {
        return self->WndProc(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT DockWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_RBUTTONUP:
        // Debug quit for Stage 1 testing. DestroyWindow send WM_DESTROY
        // synchronously (re-enter this WndProc) before returning here.
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        m_hwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

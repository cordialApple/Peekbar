#include "TaskbarOverlayWindow.h"
#include "PaintUtil.h"
#include <shellapi.h>
#include <cwchar>

using namespace Paint;

namespace
{
    constexpr wchar_t kClassName[] = L"BrowserShellOsTaskbarOverlay";

    // Debug outline: color-keyed transparent interior + a bright frame. LWA_COLORKEY
    // makes kColorKey fully see-through so only the frame shows over the taskbar.
    constexpr COLORREF kColorKey = RGB(1, 1, 1);
    constexpr COLORREF kOutline  = RGB(0, 230, 90);

    constexpr int kMinGapDip = 40;   // narrower than this → not worth an overlay (→ hide)

    // A Windows update or a third-party shell (YASB/Zebar/Managed Shell) can register
    // Shell_TrayWnd for a fake taskbar. Only the real one is owned by explorer.exe.
    bool IsExplorer(HWND hwnd)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid) return false;
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!proc) return false;
        wchar_t path[MAX_PATH] = {};
        DWORD len = MAX_PATH;
        const bool ok = QueryFullProcessImageNameW(proc, 0, path, &len);
        CloseHandle(proc);
        if (!ok) return false;
        const wchar_t* file = wcsrchr(path, L'\\');
        file = file ? file + 1 : path;
        return _wcsicmp(file, L"explorer.exe") == 0;
    }
}

TaskbarOverlayWindow::~TaskbarOverlayWindow()
{
    Destroy();
}

bool TaskbarOverlayWindow::Create(HINSTANCE instance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = StaticWndProc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    // WS_EX_TRANSPARENT + no hit-test work → clicks fall through to the taskbar, so
    // even the debug outline never blocks normal taskbar behavior. LAYERED enables
    // the color-key transparency. TOOLWINDOW/TOPMOST/NOACTIVATE match the dock.
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kClassName,
        L"",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, instance,
        this);
    if (!m_hwnd) return false;

    SetLayeredWindowAttributes(m_hwnd, kColorKey, 0, LWA_COLORKEY);
    return true;
}

void TaskbarOverlayWindow::Destroy()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_shown = false;
}

// static
HWND TaskbarOverlayWindow::FindTaskbar()
{
    HWND tray = nullptr;
    while ((tray = FindWindowExW(nullptr, tray, L"Shell_TrayWnd", nullptr)) != nullptr)
        if (IsExplorer(tray)) return tray;
    return nullptr;
}

// static
bool TaskbarOverlayWindow::IsAutoHide()
{
    APPBARDATA abd = { sizeof(abd) };
    return (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) != 0;
}

// Gap = [ right edge of the task-button list , left edge of the tray ], spanning
// the taskbar's full height. On Win11 the per-button MSTaskListWClass is gone
// (XAML), but the MSTaskSwWClass container rect still exists and is what we want;
// on Win10 the same container path holds. Works for centered and left layouts:
// as apps open/close the container's right edge moves and the gap tracks it.
TaskbarOverlayWindow::Gap TaskbarOverlayWindow::MeasureGap() const
{
    static constexpr Gap kInvalid = { {}, false };

    if (IsAutoHide()) return kInvalid;

    HWND tray = FindTaskbar();
    if (!tray) return kInvalid;

    RECT rTray;
    if (!GetWindowRect(tray, &rTray)) return kInvalid;

    // MSTaskSwWClass sits under ReBarWindow32 on Win10; on Win11 the rebar wrapper
    // may be absent, so fall back to searching the tray directly.
    HWND rebar  = FindWindowExW(tray, nullptr, L"ReBarWindow32", nullptr);
    HWND taskSw = FindWindowExW(rebar ? rebar : tray, nullptr, L"MSTaskSwWClass", nullptr);
    HWND trayNd = FindWindowExW(tray, nullptr, L"TrayNotifyWnd", nullptr);
    if (!taskSw || !trayNd) return kInvalid;

    RECT rTaskSw, rTrayNd;
    if (!GetWindowRect(taskSw, &rTaskSw) || !GetWindowRect(trayNd, &rTrayNd))
        return kInvalid;

    // Sleep-wake stale-rect guard: after resume MSTaskSwWClass can report a stale
    // position. A rect that isn't sanely nested inside the taskbar is untrustworthy.
    const int taskH = rTaskSw.bottom - rTaskSw.top;
    const int trayH = rTray.bottom - rTray.top;
    const bool sane = taskH > 0 && taskH <= trayH &&
                      rTaskSw.left >= rTray.left && rTaskSw.right <= rTray.right;
    if (!sane) return kInvalid;

    // Scale the min-gap threshold by the TASKBAR monitor's DPI, read off the tray
    // HWND directly — m_hwnd is still at (0,0) on the primary monitor until the
    // first SetWindowPos, so it would report the wrong DPI on mixed-DPI multimon.
    // GetDpiForWindow works cross-process.
    const UINT rawDpi = GetDpiForWindow(tray);
    const int dpi = rawDpi ? static_cast<int>(rawDpi) : 96;
    const RECT gap = { rTaskSw.right, rTray.top, rTrayNd.left, rTray.bottom };
    if (gap.right - gap.left < ScalePx(kMinGapDip, dpi)) return kInvalid;

    return { gap, true };
}

void TaskbarOverlayWindow::Update()
{
    if (!m_hwnd) return;

    const Gap g = MeasureGap();
    if (!g.valid)
    {
        if (m_shown) { ShowWindow(m_hwnd, SW_HIDE); m_shown = false; }
        return;
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST,
                 g.rc.left, g.rc.top,
                 g.rc.right - g.rc.left, g.rc.bottom - g.rc.top,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_shown = true;
    InvalidateRect(m_hwnd, nullptr, TRUE);
    UpdateWindow(m_hwnd);
}

// static
LRESULT CALLBACK TaskbarOverlayWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    TaskbarOverlayWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<TaskbarOverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<TaskbarOverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            self->Paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void TaskbarOverlayWindow::Paint(HDC hdc)
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    HBRUSH key = CreateSolidBrush(kColorKey);
    FillRect(hdc, &rc, key);          // transparent interior via LWA_COLORKEY
    DeleteObject(key);

    const UINT rawDpi = GetDpiForWindow(m_hwnd);
    const int dpi = rawDpi ? static_cast<int>(rawDpi) : 96;
    const int th  = ScalePx(2, dpi);

    HBRUSH frame = CreateSolidBrush(kOutline);
    for (int i = 0; i < th; ++i)
    {
        RECT r = { rc.left + i, rc.top + i, rc.right - i, rc.bottom - i };
        FrameRect(hdc, &r, frame);
    }
    DeleteObject(frame);
}

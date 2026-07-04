#pragma once
#include <windows.h>

// Stage 5b. ALL taskbar-geometry heuristics live in this one file (CLAUDE.md
// hard rule 6): a Windows update that reshuffles the taskbar is a one-file fix.
//
// 5b.1 scope: measure the empty gap between the task-button list and the system
// tray, and draw a click-through debug outline over it. No AppBar is registered
// here (this is not an AppBar — the dock owns that), so there is no ABM_REMOVE
// obligation on this window's exit paths.
//
// Measurement is pure HWND-rect + SHAppBarMessage queries (no UIA, no blocking),
// so Update() runs safely on the dock's UI thread — no worker needed (rule 5).
class TaskbarOverlayWindow
{
public:
    TaskbarOverlayWindow() = default;
    ~TaskbarOverlayWindow();
    TaskbarOverlayWindow(const TaskbarOverlayWindow&) = delete;
    TaskbarOverlayWindow& operator=(const TaskbarOverlayWindow&) = delete;

    bool Create(HINSTANCE instance);
    void Destroy();

    // Re-measure the gap and reposition/redraw the outline (hides it if the gap
    // is unmeasurable, too small, or the taskbar is auto-hidden). Cheap; safe to
    // call from a timer or on ABN_POSCHANGED / WM_DISPLAYCHANGE / WM_DPICHANGED.
    void Update();

    HWND Hwnd() const { return m_hwnd; }

private:
    struct Gap { RECT rc; bool valid; };

    static LRESULT CALLBACK StaticWndProc(HWND, UINT, WPARAM, LPARAM);
    void Paint(HDC hdc);

    // The isolated heuristic. Screen physical pixels (PMv2 → no conversion).
    Gap MeasureGap() const;
    static HWND FindTaskbar();          // Shell_TrayWnd verified owned by explorer.exe
    static bool IsAutoHide();

    HWND m_hwnd  = nullptr;
    bool m_shown = false;
};

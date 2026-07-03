#pragma once

#include <windows.h>

// Dock window. UI thread only owns this (CLAUDE.md rule 5).
// Step 1.2: bare borderless topmost rect, fixed debug size. No AppBar yet.
class DockWindow
{
public:
    DockWindow() = default;
    DockWindow(const DockWindow&) = delete;
    DockWindow& operator=(const DockWindow&) = delete;

    // Register class + create + show. False if Win32 say no.
    bool Create(HINSTANCE instance);

    HWND Hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd = nullptr;
};

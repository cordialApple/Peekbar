#pragma once
#include <windows.h>
#include <vector>

// All browser-window heuristics live here only (CLAUDE.md rule 6).
// IsBrowserFrame: pure, no state. IsTargetProcess (OpenProcess +
// QueryFullProcessImageNameW) only runs after IsTargetClass passes — so
// per-event calls on the UI pump are acceptable because non-Chrome-class
// windows exit before the syscalls. ScanBrowserFrames enumerates all
// windows and is blocking; call it pre-loop or from a worker.
bool IsBrowserFrame(HWND hwnd);

// Synchronous scan of all top-level windows via EnumWindows.
std::vector<HWND> ScanBrowserFrames();

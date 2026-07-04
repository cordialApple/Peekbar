# Win11/Win10 taskbar gap geometry (scout findings, 2026-07-04)

Reference for `TaskbarOverlayWindow` (Stage 5b). ALL heuristics isolated to that
one file (hard rule 6); this doc records *why* the code looks the way it does so
a future Windows update that breaks it is a one-file + one-doc fix.

## The headline: Win11 killed the per-button HWNDs

Win11 (22H2/23H2/24H2, incl. build 26200) rebuilt the taskbar as XAML islands
(`Windows.UI.Composition.DesktopWindowContentBridge` under `Shell_TrayWnd`). The
Win10 per-button `MSTaskListWClass` is **absent** — `FindWindowEx(...,
L"MSTaskListWClass", ...)` returns NULL on Win11.

BUT the **container** `MSTaskSwWClass` still exists as a real HWND with a valid
bounding rect on both Win10 and Win11. So a **pure HWND-rect** measurement works
on both OSes — no UIA needed. (UIA path exists as a heavier fallback: `IUIAutomation`
→ `ElementFromHandle(tray)` → find `Name="Running applications"` → `get_CurrentBoundingRectangle`.
Not used in 5b.1.)

## Measurement (both OSes)

```
tray   = FindWindowExW(NULL, prev, L"Shell_TrayWnd", NULL)  // loop; verify owner
rebar  = FindWindowExW(tray, 0, L"ReBarWindow32", NULL)      // may be NULL on Win11
taskSw = FindWindowExW(rebar?rebar:tray, 0, L"MSTaskSwWClass", NULL)  // task-list container
trayNd = FindWindowExW(tray, 0, L"TrayNotifyWnd", NULL)      // system tray
gap    = { taskSw.right, tray.top, trayNd.left, tray.bottom }  // physical px
```

Win10 only: `MSTaskListWClass` under `MSTaskSwWClass` gives a tighter right edge,
but the container's `.right` is close enough and works uniformly — we use the
container on both.

## Mandatory guards (each maps to a real failure mode)

1. **Explorer-owner check.** Third-party shells (YASB, Zebar, Managed Shell)
   register `Shell_TrayWnd` for fake taskbars. Verify `GetWindowThreadProcessId`
   → `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` →
   `QueryFullProcessImageNameW` basename == `explorer.exe` (case-insensitive).
2. **Sleep-wake stale rect.** After resume, `MSTaskSwWClass`'s rect can be stale
   (documented by Windows11DragAndDropToTaskbarFix). Sanity-check: task height
   > 0 and ≤ tray height, and `taskSw.left/right` within `tray.left/right`. If
   not sane → skip this measurement (retry on next tick / delayed remeasure).
3. **Auto-hide.** `SHAppBarMessage(ABM_GETSTATE)` & `ABS_AUTOHIDE` → taskbar is a
   1–2px sliver; hide the overlay. (`ABM_GETSTATE` ignores `hWnd`; null is fine.)
   ⚠️ This is a synchronous `SendMessage` to explorer on the UI thread — fine as a
   light query, but 5b.3 should fence it (`SendMessageTimeout`) so a hung explorer
   can't hold the pump.

## Layout variants

- `HKCU\...\Explorer\Advanced\TaskbarAl`: 0 = left-aligned, 1 = centered (Win11
  default). Both use the SAME formula — `MSTaskSwWClass.right` moves with app
  count either way. Centered ALSO leaves a left-side region, but that holds
  Start/Search/Widgets/TaskView — not clean; we only use the right-side gap.
- As apps open/close, `MSTaskSwWClass` grows/shrinks → right-side gap tracks it →
  fall back to 5a dock-hosted buttons when gap < min threshold.

## DPI / multimon

- PMv2 process: `GetWindowRect`, `SetWindowPos`, UIA `BoundingRectangle` are ALL
  physical screen pixels → pass measured rect straight to `SetWindowPos`, no
  conversion.
- Scale DPI-dependent thresholds by the **taskbar monitor's** DPI, not the
  overlay's (it starts at 0,0 on the primary monitor). `GetDpiForWindow(tray)`
  works cross-process and is monitor-authoritative.
- Scope: primary `Shell_TrayWnd` only (matches Stage 1 dock scope). Secondary
  taskbars are `Shell_SecondaryTrayWnd` — out of scope until multimon stage.

## Dynamic re-measure (for 5b.3)

`SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, ..., explorerPid, 0, OUTOFCONTEXT)`,
filter `hwnd==taskSw` or `hwnd==tray` with `idObject==OBJID_WINDOW`, debounce
~200ms, remeasure on UI thread. Also remeasure on `WM_DISPLAYCHANGE`,
`ABN_POSCHANGED`, and `PBT_APMRESUMEAUTOMATIC` (delayed ~500ms for the stale-rect
bug). 5b.1 uses a 500ms poll timer as a stand-in.

## Sources

TaskbarXI (`Taskbar11.cpp`), Windows11DragAndDropToTaskbarFix CHANGELOG (sleep-wake
bug), windhawk-mods #1704 (class-name spoofing → verify PID), Ramen Software Win11
analysis (XAML rebuild), MSDN "UI Automation and Screen Scaling" (physical-px APIs),
NVDA PR #13691 (`Shell_TrayWnd`/`XamlExplorerHostIslandWindow` are real HWNDs).

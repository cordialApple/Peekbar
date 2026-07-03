# Win32 edge-case research (agent sweep, 2026-07-03)

Raw findings from parallel research agents. Feed into steps 1.5–1.7 (AppBar
negotiation), Stage 2+ (TabReader), Stage 5 (TaskbarOverlayWindow). Not
gospel — verify against live Windows when the step lands.

## A. Auto-hide taskbar × AppBar (for steps 1.5/1.6)

| ID | Edge case | Fix | Conf |
|---|---|---|---|
| EC-1 | Auto-hide taskbar reserves 0px; QUERYPOS rect reaches screen bottom → dock covers the 2px reveal strip, taskbar can't pop up | After SETPOS: `ABM_GETSTATE` + `ABM_GETAUTOHIDEBAREX`; if auto-hide bar owns ABE_BOTTOM, shrink `rc.bottom -= 2` before MoveWindow. Re-check on `ABN_STATECHANGE`/`ABN_POSCHANGED` | HIGH |
| EC-2 | One auto-hide appbar per edge; taskbar already owns ABE_BOTTOM | Never make the dock auto-hide; plain `ABM_NEW` only | HIGH |
| EC-3 | `ABN_STATECHANGE` fires only for user UI toggles, not programmatic `ABM_SETSTATE` by other apps | Handle ABN_STATECHANGE w/ inline `ABM_GETSTATE`; cheap re-check piggybacked on WM_ACTIVATE. No polling thread | HIGH/MED |
| EC-4 | MSDN appbar sample has bugs: uninitialized `uState`, missing `break` in ABN_FULLSCREENAPP (falls into ABN_POSCHANGED); classic sample's ABN_STATECHANGE empty | Write handlers from scratch; use sample only for QUERYPOS/SETPOS sequence | HIGH |
| EC-5 | Explorer restart silently drops all appbar registrations + notifications | Handle `TaskbarCreated` broadcast (`RegisterWindowMessageW`) → re-ABM_NEW + renegotiate. Non-optional | HIGH |
| EC-6 | Win11 22621.1314+: `ABM_SETSTATE` → infinite taskbar flicker loop (MS-confirmed) | Never call ABM_SETSTATE, ever | HIGH |
| EC-7 | Broken Win11 updates (XAML pkg regression): `ABM_GETTASKBARPOS` returns zero rect | Validate rect nonzero before use; fallback = screen bottom | MED |
| EC-8 | Win11 XAML taskbar fires ABN_POSCHANGED aggressively (animations) → jitter loop risk | Compare negotiated rect vs current; MoveWindow only if delta >1px | MED |
| EC-9 | SHAppBarMessage coords = calling thread's DPI context; unaware caller gets virtualized coords | All SHAppBarMessage calls on UI thread w/ PMv2 (physical==logical, no conversion) | HIGH |
| EC-12 | `ABM_GETAUTOHIDEBAR` is primary-monitor-only | Use `ABM_GETAUTOHIDEBAREX` w/ monitor rect; Chromium-style fallback to GETSTATE+GETTASKBARPOS | HIGH |

## B. Multi-monitor × AppBar (for steps 1.5/1.6)

- QUERYPOS rect must come from `GetMonitorInfoW(...).rcMonitor` of the TARGET
  monitor, never `SM_CXSCREEN` (primary-only; official sample gets this
  wrong). Handles negative virtual-screen coords for free.
- QUERYPOS adjusts by pure rect subtraction — does NOT preserve size.
  Re-apply desired height after QUERYPOS (plan 1.5 already says re-anchor).
- `WM_DPICHANGED`: recompute height `MulDiv(h, newDpi, 96)`, then full
  QUERYPOS→SETPOS→MoveWindow cycle. Don't blindly take lParam suggested rect
  (bypasses reservation). `WM_GETDPISCALEDSIZE` can pre-hint size (PMv2).
  Do NOT re-register (no REMOVE+NEW).
- `WM_DISPLAYCHANGE`: monitor may be gone / primary reassigned →
  `MonitorFromWindow` revalidate, fall back to primary, full renegotiate.
  Treat WM_DISPLAYCHANGE + WM_DPICHANGED + ABN_POSCHANGED all as
  "re-query everything" — never cache monitor geometry/taskbar height.
- Win11 "taskbar on all displays": secondary taskbars are
  `Shell_SecondaryTrayWnd`, participate in QUERYPOS arbitration per-monitor.
  `ABM_GETTASKBARPOS` reports PRIMARY taskbar only; secondary reserved height
  proxy = `rcMonitor.bottom - rcWork.bottom`.
- Mouse-msg coords on negative-coord monitors: `GET_X_LPARAM`/`GET_Y_LPARAM`,
  never LOWORD/HIWORD.

## C. Win11 taskbar internals + browser UIA (for Stages 2–5)

- `Shell_TrayWnd` still root HWND (21H2→24H2) BUT third-party shells (YASB,
  Zebar) register fake `Shell_TrayWnd` windows → always PID-check against
  explorer.exe after FindWindow. Keep in one file (rule 6).
- Classic child chain survives as layout scaffold: Shell_TrayWnd →
  ReBarWindow32 → MSTaskSwWClass → MSTaskListWClass; TrayNotifyWnd for tray.
  Pixels rendered by XAML (`Windows.UI.Composition.DesktopWindowContentBridge`,
  "DesktopWindowXamlSource" UIA nodes are bridges, not content).
- Task buttons = XAML `Taskbar.TaskListButton` peers, ControlType=Button,
  Name=window title. Old `TB_BUTTONCOUNT`-style toolbar messages dead since
  21H2.
- Empty-space geometry (Stage 5): compute dynamically from GetWindowRect of
  Shell_TrayWnd / MSTaskSwWClass / TrayNotifyWnd; alignment via
  `HKCU\...\Explorer\Advanced\TaskbarAl` (0=left, 1=center). Never hard-code.
  Overlay = separate layered WS_EX_TOOLWINDOW window, NOT a second appbar.
- 24H2 XAML registration race: taskbar HWND may exist with zero-rect children
  at startup → retry-with-backoff init.
- Chrome: class `chrome_WidgetWin_1`. Chrome 138+ native UIA provider:
  TabItem (50019) under Tab (50036), Name=title, background tabs populated.
  Pre-138 fallback = LegacyIAccessible pattern. Accessibility tree inits
  lazily on first external UIA query — probe + retry. Chrome 146+ vertical
  tabs move the strip geometry.
- Firefox: class `MozillaWindowClass`, MSAA/IA2 bridged to UIA; TabItem
  enumeration works incl. background tabs. Use FindAll(Descendants, TabItem)
  not fixed tree paths.

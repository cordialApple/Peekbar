# Architecture & Design Spec

Native Windows shell tool: a dock strip above the taskbar that keeps minimized
browser windows' tab information visible, aggregating multiple windows as
staggered card stacks.

This spec defines the component model, the four iterative stages, the exact
Win32/UIA APIs each stage rests on, acceptance criteria, and known risks. Each
stage is **minimum functionality** and independently runnable — no stage depends
on a later stage's polish.

---

## 1. Guiding principles

1. **Minimum functionality per stage.** Every stage produces a demonstrable
   executable. Ship the smallest thing that proves the stage's capability.
2. **Native and dependency-light.** C++17, Win32 API only. No UI framework, no
   Electron, no embedded web view. The dock *is* a shell citizen: it registers
   as an application desktop toolbar (AppBar) and reserves real screen space —
   something an ordinary always-on-top window cannot do.
3. **One UI thread.** A single message loop owns the dock window; all Win32 UI
   calls happen on that thread. Anything that can block (UI Automation tree
   walks, window enumeration) runs on a worker thread and marshals results back
   via `PostMessage`.
4. **Fail visible, exit clean.** The AppBar must always deregister
   (`ABM_REMOVE`) on every exit path, or Windows keeps the strip reserved as
   dead space until logoff.

## 2. Component model

Components are introduced incrementally; the stage that introduces each is
noted.

| Component | Stage | Responsibility |
|---|---|---|
| `DockWindow` | 1 | The AppBar-registered, always-on-top, borderless window anchored above the taskbar. Owns the message loop. |
| `WindowMonitor` | 2 | Discovers browser top-level windows and tracks lifecycle (create / destroy / minimize / restore) via `EnumWindows` + `SetWinEventHook`. |
| `TabReader` | 3 | Reads a browser window's tab titles through UI Automation. Interface is deliberately narrow so a native-messaging implementation can replace it later. |
| `Store` | 3 | In-memory model: tracked windows and their last-known tabs. Single writer (UI thread); workers post snapshots into it. |
| `Renderer` | 2–4 | Paints the dock. Grows from a text indicator (2) → one tab card (3) → staggered multi-card stack with hover-fan and click-to-restore (4). |

Data model (introduced Stage 3, generalized Stage 4):

```cpp
struct Tab {
    std::wstring title;
    // url: not reliably available via UIA; see §8 upgrade path
};

struct TrackedWindow {
    HWND hwnd;
    std::wstring title;        // window title
    bool minimized;
    std::vector<Tab> tabs;     // last-known snapshot
};
```

## 3. Stage 1 — Hello-world shell tool: the AppBar dock

**The hardest stage.** All of the native shell plumbing lives here; stages 2–4
are additive once this is solid.

### Goal
A borderless, always-on-top window that registers as an **application desktop
toolbar** anchored to the bottom screen edge, sitting directly above the
taskbar, **reserving** its strip (maximized windows do not cover it), and
painting placeholder text.

### Key APIs and flow
1. `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`
   — before any window is created.
2. `RegisterClassExW` + `CreateWindowExW` with style `WS_POPUP` and extended
   styles `WS_EX_TOOLWINDOW | WS_EX_TOPMOST` (tool window keeps the dock out of
   Alt-Tab and the taskbar itself).
3. AppBar registration via `SHAppBarMessage`:
   - `ABM_NEW` with a private callback message ID registers the appbar.
   - `ABM_QUERYPOS` proposes a bottom-edge rectangle; the shell adjusts it so it
     does not overlap the taskbar or other appbars.
   - `ABM_SETPOS` commits the adjusted rectangle, then `SetWindowPos` moves the
     window into it.
   - On the callback message, handle `ABN_POSCHANGED` (taskbar moved/resized,
     another appbar appeared) by re-running QUERYPOS/SETPOS.
4. `WM_PAINT`: plain GDI text for Stage 1.
5. `WM_DPICHANGED` / `WM_DISPLAYCHANGE`: recompute the desired rect and
   renegotiate.
6. `WM_DESTROY`: `SHAppBarMessage(ABM_REMOVE, ...)` then `PostQuitMessage`.
   Also remove on `WM_ENDSESSION` and fatal-error paths.

Reference implementation to mirror: the official AppBar sample at
`microsoft/Windows-classic-samples` →
`Samples/Win7Samples/winui/shell/legacysamples/appbar/AppBar.cpp`, plus MS Learn
"Using Application Desktop Toolbars".

### Acceptance criteria
- Dock strip renders directly above the taskbar.
- A maximized window stops at the top of the dock (space genuinely reserved).
- Moving the taskbar to another edge, or a resolution/DPI change, repositions
  the dock correctly.
- Exiting the tool releases the strip — no dead reserved band remains.

### Risks / gotchas
- QUERYPOS/SETPOS negotiation is stateful and order-sensitive; getting it wrong
  yields overlaps or ghost gaps.
- Taskbar auto-hide (`ABS_AUTOHIDE`) changes the geometry rules.
- Multi-monitor: Stage 1 targets the primary monitor only; the rect must still
  be computed from that monitor's work area, not virtual-screen coordinates.
- Crash before `ABM_REMOVE` leaves reserved space until explorer restart —
  register a last-chance handler that removes the appbar.

## 4. Stage 2 — Detect a browser is open; perform a simple action

### Goal
The dock reflects browser presence: when a Chrome/Edge window exists, the dock
lights an indicator and shows the window's title; when the last one closes, it
clears. No polling loops.

### Key APIs
- **Discovery:** `EnumWindows`, filtering to real browser frames:
  - window class `Chrome_WidgetWin_1` (Chromium family: Chrome, Edge, Brave...),
  - visible (`IsWindowVisible`), unowned (`GetWindow(hwnd, GW_OWNER) == NULL`),
    non-empty title — Chromium spawns several helper top-level windows with the
    same class that must be excluded,
  - process image name via `GetWindowThreadProcessId` +
    `OpenProcess`/`QueryFullProcessImageNameW` (`chrome.exe`, `msedge.exe`).
- **Lifecycle without polling:** `SetWinEventHook` (out-of-context,
  `WINEVENT_OUTOFCONTEXT`) for `EVENT_OBJECT_CREATE`, `EVENT_OBJECT_DESTROY`,
  `EVENT_OBJECT_SHOW`, `EVENT_OBJECT_HIDE`; the hook callback re-validates the
  HWND against the filter and posts add/remove messages to the UI thread.

### Acceptance criteria
- Launch browser → indicator appears within ~1s. Close last browser window →
  indicator clears. CPU usage idles at ~0% (event-driven, no timer scans).

### Risks
- Helper-window false positives (mitigated by the filter above).
- Hook callbacks arrive on arbitrary threads' contexts — do no UI work there;
  post to the dock thread.

## 5. Stage 3 — Track one browser's tabs; keep them on-screen after minimize

### Goal
When a tracked browser window is **minimized**, the dock shows a card listing
that window's tab titles, and the card persists while the window stays
minimized. Restoring the window clears the card. This is the core "toolbar
stays visible above the taskbar" behavior.

### Key APIs
- **Minimize/restore detection:** extend the Stage-2 hook set with
  `EVENT_SYSTEM_MINIMIZESTART` and `EVENT_SYSTEM_MINIMIZEEND`.
- **Tab reading (UI Automation):**
  - `CoCreateInstance(CLSID_CUIAutomation)` → `IUIAutomation`.
  - `ElementFromHandle(browserHwnd)`, then a condition-based find for the tab
    strip: control type `UIA_TabControlTypeId`, children
    `UIA_TabItemControlTypeId`; each tab item's `Name` property is the tab
    title.
  - Use a cache request (`IUIAutomationCacheRequest`) to pull all names in one
    cross-process round trip.
- **Timing:** snapshot tabs on `MINIMIZESTART` (the UIA tree is still live at
  that moment) and opportunistically on foreground/title changes, storing the
  result in `Store`. The card renders from the stored snapshot, so it remains
  valid even if the minimized window's UIA tree goes dormant (Chromium can
  suspend renderers of minimized windows).

### Acceptance criteria
- Minimize a browser with N tabs → dock card lists those N titles.
- Card persists for as long as the window is minimized.
- Restore → card clears (or collapses back to the Stage-2 indicator).

### Risks — where UIA is known to be fragile
- **Tab titles: yes. Full URLs: no.** UIA exposes only the *active* tab's
  address-bar value; background-tab URLs are not available. This is accepted
  for Stage 3 and is the trigger for the §8 upgrade path.
- Chromium's accessibility tree may be inactive until a client queries it;
  first query can be slow. Do all UIA work on a worker thread; debounce.
- Browser updates can rearrange the automation tree; keep the tab-strip lookup
  heuristic isolated inside `TabReader` so fixes are one-file.

## 6. Stage 4 — Track multiple browsers' tabs; staggered stack aggregation

### Goal
Generalize Stage 3 to *every* tracked browser window. Each minimized window is
a card; multiple cards render as a **staggered, layered stack** in the dock:
offset x/y like a fanned deck, expanding upward on hover, and restoring the
corresponding window when a card is clicked.

### Key additions
- `Store` becomes a map `HWND → TrackedWindow`; `WindowMonitor` keeps it in
  sync across all browser windows (including multiple processes/profiles —
  Chrome and Edge tracked simultaneously).
- `Renderer` gains:
  - staggered layout (per-card offset + z-order; newest minimized on top),
  - hover hit-testing → fan animation opening upward into the space above the
    dock (`WM_MOUSEMOVE`/`TrackMouseEvent` for enter/leave),
  - click → `ShowWindow(hwnd, SW_RESTORE)` + `SetForegroundWindow(hwnd)`, then
    the stack re-settles.
- Fan overlay taller than the reserved strip: the reserved AppBar band stays
  slim; the fan renders in a transient pop-up window (`WS_EX_TOPMOST`,
  layered) that appears above the dock on hover and dismisses on leave — the
  reserved area itself never grows.

### Acceptance criteria
- Minimize three browser windows → three staggered cards, each listing its own
  window's tabs.
- Hover fans the stack; leaving collapses it.
- Clicking a card restores exactly that window and removes its card.

### Risks
- Input routing/z-order across overlapping cards (rigorous hit-test rects).
- Dock overflow with many minimized windows: cap visible cards, spill into a
  "+N more" affordance rather than growing the reserved strip.
- Repeated UIA snapshots across many windows: debounce per-window, snapshot
  only on minimize/title-change events, never on a timer.

## 7. Cross-cutting concerns

- **Threading model:** UI thread = dock message loop, sole `Store` writer.
  Worker thread(s) run `EnumWindows` re-validation and UIA snapshots; they
  communicate exclusively by `PostMessage`-ing owned heap payloads to the dock
  window. `SetWinEventHook` callbacks do the minimum (validate + post).
- **DPI / display changes:** per-monitor-v2 awareness set at startup; renegotiate
  the AppBar rect on `WM_DPICHANGED`, `WM_DISPLAYCHANGE`, and `ABN_POSCHANGED`.
- **Lifetime hygiene:** `ABM_REMOVE` on every exit path; unhook all WinEvent
  hooks; `CoUninitialize` after UIA teardown.
- **Configuration (later):** a small config file for which browser processes to
  track and dock appearance. Not in stages 1–4 minimum functionality.

## 8. Upgrade path: browser extension + native messaging (documented, not built)

If/when UIA's limits bite (no background-tab URLs, tree fragility), the
replacement is the browsers' official **native messaging** mechanism:

- A WebExtension (Chrome/Edge, Manifest V3) with the `tabs` permission observes
  exact tab titles, URLs, favicons, and window membership, and connects to a
  registered **native messaging host**.
- The host is a small native executable (stdin/stdout, length-prefixed JSON)
  that forwards tab state to the shell tool over a local pipe — or the shell
  tool itself acts as the host.
- Integration point: this becomes an alternative implementation behind the
  `TabReader` interface. `Store`, `Renderer`, `WindowMonitor`, and `DockWindow`
  are untouched.

Note: **no Electron anywhere in this architecture.** Electron was considered in
early ideation as a host for a web-based dock UI; with a native C++ AppBar,
the dock already owns real shell-level screen space, and the native messaging
host is a plain executable. Electron would add a Chromium runtime without
adding capability.

## 9. Repository layout

```
README.md
docs/ARCHITECTURE.md        ← this document
CMakeLists.txt              (lands with Stage 1)
src/
  main.cpp                  entry point, DPI setup, message loop
  DockWindow.{h,cpp}        Stage 1
  WindowMonitor.{h,cpp}     Stage 2
  TabReader.{h,cpp}         Stage 3 (UIA implementation)
  Store.{h,cpp}             Stage 3
  Renderer.{h,cpp}          Stages 2–4
```

## 10. Per-stage verification (run on Windows)

| Stage | Test |
|---|---|
| 1 | Build, run; maximize Notepad → it must stop above the dock. Exit tool → maximize again → window reaches the taskbar (strip released). Move taskbar to left edge → dock renegotiates. |
| 2 | Start tool, then open Chrome → indicator on. Close all Chrome windows → indicator off. Task Manager shows ~0% CPU at idle. |
| 3 | Open a browser with 5 known tabs, minimize → dock card lists the 5 titles. Restore → card clears. |
| 4 | Open 3 browser windows (mixed Chrome + Edge), minimize all → 3 staggered cards. Hover → fan. Click card #2 → exactly that window restores. |

## 11. References

- [Using Application Desktop Toolbars — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/shell/application-desktop-toolbars)
- [`SHAppBarMessage` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shappbarmessage)
- [`APPBARDATA` — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-appbardata)
- [Official AppBar sample (`AppBar.cpp`) — microsoft/Windows-classic-samples](https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winui/shell/legacysamples/appbar/AppBar.cpp)
- [Native messaging — Microsoft Edge developer docs](https://learn.microsoft.com/en-us/microsoft-edge/extensions-chromium/developer-guide/native-messaging)

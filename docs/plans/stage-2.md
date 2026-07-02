# Stage 2 plan — detect a browser is open; simple dock action

Spec: `docs/ARCHITECTURE.md` §4. Acceptance: §12 row 2.
Draft — refine against the actual Stage-1 code before starting.

## Step 2.1 — Browser-window predicate

**Build:** `src/WindowMonitor.{h,cpp}` with a pure function
`bool IsBrowserFrame(HWND)`: class name `Chrome_WidgetWin_1`, visible,
unowned, non-empty title, process image name in {`chrome.exe`, `msedge.exe`}
(via `GetWindowThreadProcessId` + `OpenProcess` +
`QueryFullProcessImageNameW`). No hooks yet. Add a temporary debug command
(e.g. `--scan`) that runs `EnumWindows` with the predicate and prints matches
to `OutputDebugStringW`.

**Checkpoint:** with Chrome open (several windows + a helper window like a
tooltip), the scan lists exactly the real browser frames — no helpers, no
duplicates. With Edge open too, both appear.

## Step 2.2 — Initial scan + dock indicator

**Build:** on dock startup, run the scan and keep a `std::vector<HWND>` of
tracked windows in `DockWindow` (the full `Store` arrives in Stage 3).
`src/Renderer.{h,cpp}` first version: paint an indicator region — "browser:
none" vs "browser: <title of first tracked window> (+N)".

**Checkpoint:** start the dock while a browser is already open → indicator
lit with the title. Start with none open → "none".

## Step 2.3 — Live tracking via WinEvent hooks

**Build:** `SetWinEventHook` (`WINEVENT_OUTOFCONTEXT`) for
`EVENT_OBJECT_CREATE`, `EVENT_OBJECT_DESTROY`, `EVENT_OBJECT_SHOW`,
`EVENT_OBJECT_HIDE`. Callback: cheap pre-filter (idObject == OBJID_WINDOW,
top-level), then `PostMessage(dock, WM_APP_WINDOWEVENT, event, (LPARAM)hwnd)`.
The dock thread re-validates with `IsBrowserFrame` and adds/removes from the
tracked list, repainting the indicator. Unhook in `WM_DESTROY` (before AppBar
removal is fine, but before `PostQuitMessage` always).

**Checkpoint:** dock running → open a browser → indicator appears within ~1s;
close all browser windows → clears. Task Manager: ~0% CPU while idle — no
polling.

## Step 2.4 — Debounce + acceptance

**Build:** coalesce event bursts (Chromium fires many create/show events per
window): a short `SetTimer`-based debounce (~200 ms) before re-validating.
Remove the `--scan` debug path or keep behind `#ifdef _DEBUG`.

**Checkpoint:** full §12 row 2 acceptance. Also: open 10 tabs rapidly →
indicator doesn't flicker; CPU stays ~0%.

## Definition of done

- [ ] All checkpoints pass on Windows 10/11.
- [ ] No timers except the debounce timer; no polling loops.
- [ ] All heuristics (class names, process names) live only in
      `WindowMonitor`.
- [ ] Stage 1 acceptance still passes (no regressions to AppBar behavior).

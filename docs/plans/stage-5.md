# Stage 5 plan — automation buttons in taskbar empty space

Spec: `docs/ARCHITECTURE.md` §7. Acceptance: §12 rows 5a/5b.
Draft — refine against the actual Stage-4 code before starting.
Two sub-phases: 5a (dock-hosted buttons) must fully land before 5b (taskbar
overlay) begins; 5a remains the permanent fallback.

## Phase 5a — buttons hosted in the dock strip

### Step 5a.1 — Config load ✅ (done 2026-07-04)
**Built:** `src/Launcher.{h,cpp}`. Chose the plan's line-based format (not JSON)
to honor hard rule 2 (no dep) and avoid a fragile hand-rolled JSON parser:
`%LOCALAPPDATA%\browser_shell_os\config.txt`, one button per line
`style|label|action|target|iconPath?` (`style`∈pill|icon, `action`∈url|shortcut|
command). Blank / `#` / `;` lines ignored; malformed lines skipped with
`OutputDebugStringW` reason; missing file → zero buttons. Decodes UTF-16LE-BOM /
UTF-8-BOM / raw-UTF-8. `Button` struct + `Launcher::Buttons()` accessor for
5a.2/5a.3. Loaded synchronously in `DockWindow::Create` (startup, like
`ScanBrowserFrames`). Two inspector bursts (parsing-robustness + resource-
hygiene): fixed a swprintf_s-overflow process-kill (→ `_snwprintf_s`/`_TRUNCATE`),
a misaligned-wchar_t UB (→ memcpy), an unchecked MBTWC size-query, dropped a
redundant link pragma → re-burst clean → adjudicator MAY PROCEED. Sample config
written to the config path for the runtime check.

**Checkpoint:** config with 2 buttons loads; malformed file → dock still
starts, buttons skipped, `OutputDebugStringW` notes why. (Runtime check pending
on Windows via debugger; render/execute land in 5a.2–5a.3.)

### Step 5a.2 — Actions ✅ (done 2026-07-04)
**Built:** `Launcher::Execute(const Button&)` — copies action+target into a
detached **MTA** worker (pump-less fire-and-forget; STA would need a pump and
could hang a DDE handler) → `url`/`shortcut`: `ShellExecuteW(open, target)`;
`command`: `CreateProcessW` (both handles closed). `CoUninitialize` only on
`SUCCEEDED(CoInitializeEx)`. Never blocks the UI thread (spawn+detach returns
immediately). Debug trigger: `#ifdef _DEBUG` `WM_MBUTTONUP` cycles configured
buttons. Two bursts (threading + resource): fixed unconditional-CoUninitialize,
STA→MTA, ConfigPath free-of-garbage guard, MBTWC guard → re-burst clean →
adjudicator MAY PROCEED.

**Checkpoint:** each action type works from the debug middle-click trigger.
(Runtime check pending on Windows.)

### Step 5a.3 — Button strip rendering + clicks ✅ (done 2026-07-04)
**Built:** user chose **right-column overlay** (buttons pinned top-right, drawn
over the cards). `Renderer::ButtonLayout` (single source for paint + hit-test,
like `CardLayout`) right-anchors ScalePx(84) pills in the top strip, drops
non-fitting silently. `DrawButton` = RoundRect (light `kButtonBg` fill +
`kButtonBorder`, radius clamped to height/2) + centered ellipsized label
(`kTextOnBg`). `Paint` draws buttons last (over cards + empty-state).
`DockWindow::ButtonAt` hit-test; `WM_LBUTTONUP` routes button-first → card;
`WM_MOUSEMOVE` suppresses the fan over a button. Removed the 5a.2 `_DEBUG`
middle-click trigger (real left-clicks execute now). Icon-image rendering
(`LoadImageW`/`SHGetFileInfoW`) deferred — icon style falls back to a labeled
pill. Burst (DPI/visual/GDI+hit-test): fixed pill invisibility over dark cards
(→ light `kButtonBg`), unscaled border pen, unclamped radius → re-burst → MAY
PROCEED.

**Checkpoint:** §12 row 5a — URL button opens site in default browser; shortcut
runs; buttons persist across restart. **Awaiting user visual acceptance** (pill
look; empty-state text vs pills; light-pill legibility where blue shows).

### Step 5a.4 — Config hot-reload ✅ (done 2026-07-04)
**Built:** `src/ConfigWatcher.{h,cpp}` — worker thread, overlapped
`ReadDirectoryChangesW` on the config dir; matches `config.txt` →
`PostMessageW(kConfigChangedMsg)`. Manual-reset stop-event +
`WaitForMultipleObjects` cancels the blocking wait; teardown drains a pending
I/O (`CancelIo` → `GetOverlappedResult(TRUE)`, guarded by a `pending` flag so no
break path deadlocks). `DockWindow`: `kConfigChangedMsg` → 300ms `kConfigTimer`
debounce → `m_launcher.Load()` + repaint on the UI thread; `Create` makes the
dir + starts the watcher; `WM_DESTROY` joins the watcher before `AppBarRemove`.
`Launcher` refactored → `ConfigDir()`/`ConfigFileName()`/`ConfigPath()`. Burst
(threading + AppBar + resource): fixed a BLOCKING undrained-overlapped-I/O
teardown → re-burst → adjudicator MAY PROCEED.

**Checkpoint:** edit config while running → buttons update within ~1s, no
restart. (Runtime check pending on Windows.)

## Phase 5a COMPLETE (code): dock-hosted automation buttons — config → actions
## → render → hot-reload. Permanent fallback for 5b. Next: Phase 5b taskbar overlay.

## Phase 5b — overlay on the taskbar's empty region

### Step 5b.1 — Gap measurement ✅ (code done 2026-07-04)
**Built:** `src/TaskbarOverlayWindow.{h,cpp}` (ALL taskbar-geometry heuristics
here — hard rule 6). Scout (`docs/research/win11-taskbar-geometry.md`) settled the
approach: on Win11 the per-button `MSTaskListWClass` is gone (XAML islands), but
the `MSTaskSwWClass` *container* rect still exists — so a **pure HWND-rect** path
works on both Win10 and Win11, no UIA needed (keeps measurement on the UI thread,
non-blocking). `FindTaskbar()` finds `Shell_TrayWnd` and verifies explorer.exe
owner (class-name spoofing guard). `MeasureGap()` = `[MSTaskSwWClass.right,
TrayNotifyWnd.left]` × full tray height, with: rebar-or-tray fallback for the task
container, sleep-wake stale-rect sanity check (task rect must nest inside tray),
`SHAppBarMessage(ABM_GETSTATE)` auto-hide bail, min-gap threshold scaled by the
**taskbar monitor's** DPI (`GetDpiForWindow(tray)`, cross-process). Debug outline
= click-through layered window (`WS_EX_TRANSPARENT|LAYERED`, `LWA_COLORKEY`) with
a bright green frame. `DockWindow` owns it; `Update()` fires from `Create`, a
500ms `kOverlayTimer` (5b.1 scaffold — 5b.3 swaps in the `LOCATIONCHANGE` hook),
`ABN_POSCHANGED`, `WM_DISPLAYCHANGE`, `WM_DPICHANGED`; torn down in `WM_DESTROY`
(+ `KillTimer` in `WM_ENDSESSION`). Overlay never registers an AppBar → no
ABM_REMOVE obligation. Burst (AppBar/threading/DPI/visual) → adjudicator
MAY PROCEED; applied F-01 (taskbar-monitor DPI), F-03 (endsession KillTimer),
F-05 (dead member). Debt for 5b.3: fence the `ABM_GETSTATE` explorer round-trip
(F-02), cache explorer-owner check (F-04).

**Checkpoint:** outline hugs the empty region on Win10 and Win11, centered and
left-aligned layouts; opening/closing apps moves it. **Awaiting user visual/
runtime check on Windows** (build clean; can't verify placement off-Windows).

### Step 5b.2 — Overlay window + click-through
**Build:** borderless topmost tool window positioned in the measured gap;
`WM_NCHITTEST` returns `HTTRANSPARENT` outside button rects. Buttons render
via the SAME `Launcher`/button model as 5a (shared component, different
host).

**Checkpoint:** buttons work in the taskbar gap; clicks outside buttons
reach the taskbar normally (e.g. right-click taskbar menu still opens).

### Step 5b.3 — Dynamic re-measure + fallback
**Build:** re-measure on task-list `EVENT_OBJECT_LOCATIONCHANGE`
(debounced), `WM_DISPLAYCHANGE`, `ABN_POSCHANGED`. If measurement fails or
the gap is too small → hide the overlay and fall back to 5a's dock-hosted
strip automatically.

**Checkpoint:** §12 row 5b: open 15 apps → overlay yields; close them →
overlay grows back; simulate failure (rename detection heuristic) → buttons
appear in the dock instead.

## Definition of done

- [ ] 5a passes independently of 5b; 5b failure degrades to 5a silently.
- [ ] No code injection into explorer.exe anywhere; overlay only.
- [ ] Stage 1–4 acceptance rows still pass.

# Stage 5 plan — automation buttons in taskbar empty space

Spec: `docs/ARCHITECTURE.md` §7. Acceptance: §12 rows 5a/5b.
Draft — refine against the actual Stage-4 code before starting.
Two sub-phases: 5a (dock-hosted buttons) must fully land before 5b (taskbar
overlay) begins; 5a remains the permanent fallback.

## Phase 5a — buttons hosted in the dock strip

### Step 5a.1 — Config load
**Build:** `src/Launcher.{h,cpp}`: read
`%LOCALAPPDATA%\browser_shell_os\config.json` at startup — array of buttons
`{ id, style: "pill"|"icon", label, iconPath?, action: "url"|"shortcut"|
"command", target }`. Hand-rolled minimal JSON parse or a single-header
parser ONLY with explicit approval (hard rule 2); otherwise a simple
line-based format is acceptable for minimum functionality. Missing file →
zero buttons, no error.

**Checkpoint:** config with 2 buttons loads; malformed file → dock still
starts, buttons skipped, `OutputDebugStringW` notes why.

### Step 5a.2 — Actions
**Build:** `Launcher::Execute(button)`: `url` → `ShellExecuteW(open, target)`
(default browser); `shortcut` → `ShellExecuteW` on the `.lnk`; `command` →
`CreateProcessW`. Fire-and-forget, never block the UI thread.

**Checkpoint:** each action type works from a debug key binding.

### Step 5a.3 — Button strip rendering + clicks
**Build:** `Renderer` draws pill/icon buttons at the dock's right end;
hit-testing routes clicks to `Launcher::Execute`. Icons via
`LoadImageW`/`SHGetFileInfoW` for shortcut targets; pills are rounded rects
with label text.

**Checkpoint:** §12 row 5a: URL button opens the site in the default
browser; shortcut button runs; buttons persist across restart.

### Step 5a.4 — Config hot-reload
**Build:** watch the config directory (`ReadDirectoryChangesW` on a worker,
post to dock thread); reload and repaint on change.

**Checkpoint:** edit config while running → buttons update within ~1s, no
restart.

## Phase 5b — overlay on the taskbar's empty region

### Step 5b.1 — Gap measurement
**Build:** `src/TaskbarOverlayWindow.{h,cpp}` (ALL taskbar-geometry
heuristics live here — hard rule 6). Find `Shell_TrayWnd`; measure the empty
gap between the task-button list and the tray via UIA over the taskbar tree
(Win11) / child-window rects (`ReBarWindow32`, Win10). Debug overlay outline
only.

**Checkpoint:** outline hugs the empty region on Win10 and Win11, centered
and left-aligned taskbar layouts; opening/closing apps moves it correctly.

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

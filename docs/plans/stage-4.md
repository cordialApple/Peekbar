# Stage 4 plan — multiple browsers, staggered stack aggregation

Spec: `docs/ARCHITECTURE.md` §6. Acceptance: §12 row 4.
Draft — refine against the actual Stage-3 code before starting.

## Step 4.1 — Multi-window Store

**Build:** generalize `Store` and `WindowMonitor` to track every browser
frame simultaneously (map `HWND → TrackedWindow`), including windows from
different browsers/profiles at once. TabReader snapshots are per-window and
already keyed — verify no cross-window state.

**Checkpoint:** three windows (2 Chrome + 1 Edge) minimized one after
another → `Store` holds three entries with the right tab sets (debug dump).

## Step 4.2 — Staggered card layout

**Build:** `Renderer` lays minimized-window cards as a stack: fixed offset
per card (x/y stagger), newest on top (z-order = minimize order). Collapsed
state shows the top card fully and the edges of the ones beneath.

**Checkpoint:** minimize 3 windows → visibly layered staggered cards in the
dock, newest on top. Restore one externally (taskbar) → stack re-settles.

## Step 4.3 — Hover fan (transient popup)

**Build:** `TrackMouseEvent` for hover enter/leave over the stack region. On
hover, show a transient popup window (`WS_EX_TOOLWINDOW | WS_EX_TOPMOST`,
same window class family as the dock) ABOVE the reserved strip that fans the
cards vertically so each is readable. Dismiss on leave / `WM_ACTIVATEAPP`
loss. The reserved AppBar strip itself never grows.

**Checkpoint:** hover → fan opens upward over other windows without stealing
focus; move mouse away → closes. The reserved work area does not change
(maximized Notepad's size is identical before/during/after).

## Step 4.4 — Click-to-restore + hit testing

**Build:** hit-test map from card rects (collapsed and fanned) to HWND.
Click → `ShowWindow(SW_RESTORE)` + `SetForegroundWindow`; remove the card;
re-settle. Handle the foreground-permission quirk: if
`SetForegroundWindow` is denied, fall back to
`SwitchToThisWindow`/flashing.

**Checkpoint:** §12 row 4: click card #2 of 3 → exactly that window restores
and gets focus; its card disappears; the other two remain correctly stacked.

## Step 4.5 — Overflow + polish

**Build:** cap visible cards (e.g. 5); further windows collapse into a
"+N more" tail card whose fan shows the rest. Debounce UIA snapshots when
many windows minimize at once (e.g. Win+M).

**Checkpoint:** minimize 8 windows via Win+M → dock stays responsive, shows
5 cards + "+3", fan reveals all; CPU settles back to ~0%.

## Definition of done

- [ ] All checkpoints pass on Windows 10/11 with mixed Chrome + Edge.
- [ ] Reserved strip height constant; fan is a transient popup only.
- [ ] Stage 1–3 acceptance rows still pass.

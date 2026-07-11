# Taskbar-chip feasibility (win32-scout, 2026-07-04)

Platform research backing the "kill the dock, chips in the taskbar" rework. All
taskbar-geometry heuristics stay isolated in `TaskbarOverlayWindow` (rule 6).

## Q1: Hover on a topmost layered overlay (HTCLIENT chips / HTTRANSPARENT gaps)

`WM_NCHITTEST` fires on EVERY cursor move over the overlay rect, regardless of
the value returned. `HTCLIENT` makes the overlay get `WM_MOUSEMOVE`.
`HTTRANSPARENT` routes input to the window below, and the overlay gets NO
`WM_MOUSEMOVE`.

`TrackMouseEvent(TME_LEAVE)`/`WM_MOUSELEAVE` track the whole HWND rect, not
HTCLIENT sub-regions. Leaving a chip for the inter-chip gap does NOT fire
`WM_MOUSELEAVE`.

**Trap:** chip (HTCLIENT) to gap (HTTRANSPARENT) stops `WM_MOUSEMOVE` but
doesn't fire `WM_MOUSELEAVE`, so the chip stays highlighted (stale hover).
Intermittent, easy to miss.

**Fix (canonical):** drive hover state from `WM_NCHITTEST`. Convert lParam
(screen) to client, hit-test chip rects, set/clear `m_hoverChip`, return
`HTCLIENT`/`HTTRANSPARENT`. Keep `TrackMouseEvent(TME_LEAVE)` armed as the
whole-overlay leave fallback. `WM_NCHITTEST` also fires for drag/drop queries
(Raymond Chen 2011-02-18); clearing hover on those is harmless. `LWA_COLORKEY`
is visual only. It never affects mouse routing (only the NCHITTEST return
does).

## Q2: Fan z-order above the Win11 taskbar (both topmost)

Within the topmost band, the last `SetWindowPos(HWND_TOPMOST, ...)` without
`SWP_NOZORDER` wins. Recipe: `SetWindowPos(fan, HWND_TOPMOST, x,y,w,h,
SWP_SHOWWINDOW|SWP_NOACTIVATE)`.

`SWP_NOACTIVATE` makes `hWndInsertAfter` honored regardless of foreground.
`SW_SHOWNA` alone does NOT set topmost position: always use SetWindowPos.

Keep `MA_NOACTIVATE` (hover-activate can activate even WS_EX_NOACTIVATE,
per Raymond Chen 2024-09-19). Robustness: on `WM_WINDOWPOSCHANGED`, if
demoted below the taskbar, re-assert `HWND_TOPMOST` (once per message, not a
loop).

## Q3: Multiple HTCLIENT sub-regions in one window

No documented limit. Unlimited disjoint chip rects are the standard overlay
pattern (GLFW mouse-passthrough, PowerToys, Windhawk).

Coord-space trap: `WM_NCHITTEST` lParam is SCREEN coords (`ScreenToClient`
first); `WM_MOUSEMOVE`/`WM_LBUTTONUP` lParam is CLIENT coords already. Don't
mix.

## Q4: Gap width reality on Win11

No official pixel spec. Community-observed via `GetWindowRect` on
`MSTaskSwWClass` (center cluster) and `TrayNotifyWnd` (tray): gap =
`TrayNotifyWnd.left - MSTaskSwWClass.right`.

Center-aligned @1920x1080/100%: ~400 to 650px (0 to 2 apps), 250 to 400px
(4 to 6), 100 to 250px (8 to 10), <100px to 0 (12+). DPI scales physical px;
chips must be DPI-scaled.

Left-aligned taskbar keeps the chip area in the same right-side gap (usually
more room than center for the same app count). Overflow is mandatory: on
`MSTaskSwWClass` `EVENT_OBJECT_LOCATIONCHANGE` (explorer-PID-scoped),
remeasure and re-layout, dropping chips when they don't fit.

## Q5: Thumbnail-preview interference

Thumbnails trigger from hovering task buttons INSIDE `MSTaskSwWClass`, which
is not under the overlay (overlay covers only the empty gap). No
interference.

Real risk: measurement/DPI rounding overlapping a taskbar edge blocks that
edge's input. Mitigation: 4px dead-zone inside BOTH gap edges; never lay out
over raw gap width.

## Q6: Clean AppBar removal

A single `SHAppBarMessage(ABM_REMOVE, ...)` releases the reservation
immediately (no delay, no explorer restart). Never calling `ABM_NEW` again
means no reservation ever, which is fully sufficient.

Crash without `ABM_REMOVE` still leaks reserved space until explorer
restarts. So any early-exit path that runs after the old AppBar was
registered but before removal must still fire `ABM_REMOVE` (covered by the
AppBar-hygiene lens on the transition commit).

## Sources
MS Learn: Mouse Input Overview; TrackMouseEvent; WM_MOUSELEAVE; SetWindowPos;
ABM_REMOVE; Application Desktop Toolbars. Raymond Chen (oldnewthing):
2011-02-18 (NCHITTEST non-mouse), 2024-09-19 (WS_EX_NOACTIVATE hover-activate).
Gap-width figures are community/observed (no official MS spec).

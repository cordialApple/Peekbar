# HANDOFF — read this first, every session

Entry point for any fresh Claude session (or human) working on this repo.
Purpose: give full project direction with minimal context, so work starts at
the right step with no drift.

## Project in three sentences

`browser_shell_os` is a native C++17/Win32 Windows shell tool: an AppBar dock
that reserves a strip directly above the taskbar. Minimized browser windows
leave their tab info visible there — eventually as staggered card stacks
aggregating multiple windows — plus automation buttons in the taskbar's empty
space. A separate, never-bundled `shell_profiler` observes the shell's
performance over ETW.

## How to work here

1. Obey `CLAUDE.md` (working rules — short, read it whole).
2. Find the next action on the status board below.
3. Open ONLY that stage's plan in `docs/plans/` and the matching
   `docs/ARCHITECTURE.md` section. Do not load everything.
4. Implement exactly one step; pass its checkpoint; update this file
   (status board + session log); commit.
5. Building/running requires Windows (MSVC + CMake). From a non-Windows
   session: write the code, say clearly that runtime verification is pending.

## Status board

| Workstream | State |
|---|---|
| Docs & plans | ✅ Complete (architecture + per-stage plans) |
| Stage 1 — AppBar dock | ✅ Complete — all 7 steps + acceptance row passed on Win11 |
| Stage 2 — browser detection | ✅ Complete — all 4 steps + §12 row 2 accepted on Win11 |
| Stage 3 — single-window tabs | ✅ Complete — tabs render per-window on minimize, accepted on Win11 |
| Stage 4 — multi-window stacks | 🟡 code complete (4.1–4.5 + 4.5a) — §12 row 4 acceptance pending on Windows |
| Stage 5 — taskbar buttons | ✅ ACCEPTED on Win11 — 5a dock buttons + 5b gap overlay (pills-in-gap, event re-measure, single-host dock fallback); all 5 visual checks pass |
| Profiler (parallel workstream) | 🟡 core loop RUNTIME-VERIFIED on Windows 2026-07-08 — elevated `shell_profiler --raw`/`--csv` both ran live against the shell, all 6 events fired and decoded correctly (`Paint`, `FanActivateLatency`, `WinEventCallback`, `UiaSnapshot`, `StoreUpdate`, `LauncherAction`); `AppBarNegotiate` dropped (dead, AppBar gone). §12 row P NOT fully closed — the kill-profiler/delete-binary decoupling-proof sub-checks weren't exercised this session — see `docs/plans/profiler.md` |
| Deployment — permanent run ("service" goal) | ⬜ v1 (logon autostart) after Stage 1; v2 (watchdog service) after Stage 5 — see `ARCHITECTURE.md` §13 |

**RESOLVED 2026-07-08 (user-confirmed on Windows): overlay-instability fix holds — no AV crash, no stuck
visible-but-empty overlay under terminal churn; fullscreen suppression latency OK. Former top priority, now
closed.**

**Next action: 5-field split's live capture came back AMBIGUOUS vs. the pre-registered fork — Opus re-read it
and a new "cold-provider warm-up" probe is now code, needs its own live capture.** 23-click capture (elevated
`shell_profiler --raw`, all `gate2_attempts=1`) gave a clean breakdown of the ~327ms avg walk:
`us_findall_tabctrls` 54.7% (178.6ms avg, still the single largest piece), `us_element_from_handle` **31.0%
(101.3ms avg) — NOT anticipated**, `us_findall_tabitems` 7.6%, `us_is_inside_document` 6.5% (rules out the
candidate-culling branch outright — `tabctrl_candidates` averaged 2.17, never 1, but the parent-walk cost to
reject them is cheap). The 4 fields sum to 99.8% of the walk (clean accounting). Sent this to Opus (per this
investigation's "check with Opus before choosing a fix" rule): `ElementFromHandle` is normally a near-instant
HWND→element lookup, not a tree walk — 101ms means it's paying the cost of being the FIRST UIA call after
restore, i.e. Chromium's lazy accessibility-tree materialization, not the lookup itself. Opus's read: this
means guided descent (the pre-registered fix for a `us_findall_tabctrls`-dominant result) is capped at well
under its naive 54.7% headroom if step 3 is paying leftover materialization cost rather than pure scope-
proportional walking — so guided descent's real ceiling is unknown until that's ruled out. **Opus's explicit
recommendation: do NOT write guided descent yet.** Instead landed a diagnostic-only "warm-up touch" — one
throwaway `automation->ElementFromHandle(hwnd, &warmupElem)` fired as early as possible in `ActivateTab`
(before Gate 1's poll-sleep even starts), timed, result/HRESULT fully discarded, reported as a new
`us_warmup_touch` field on `FanActivateLatency` (append-only, after all 16 pre-existing fields). Zero
control-flow dependency — cannot affect tab selection. If a capture with this shows both
`us_element_from_handle` AND `us_findall_tabctrls` collapse together, that confirms cold-provider
materialization (not FindAll's scope) is the real bottleneck, and the fix becomes "warm the provider during
the restore-settle window already being slept through" — no tree-shape assumptions, no wrong-tab risk. Only
if `us_findall_tabctrls` stays large after warm-up does guided descent become the next move — and even then
with a mandatory safety backstop Opus specified: re-verify `ControlType==TabControl && !IsInsideDocument`
after descending, falling back to the existing blanket `FindAll` if either check fails (since candidates
average 2.17, never 1 — there's always a decoy to land on wrong). Threading-rule-violations inspector (only
applicable lens) → one INFORMATIONAL nit (warm-up call isn't `stop`-cancelable, but neither is any of the
pre-existing walk's UIA calls — same accepted shape) → adjudicator MAY PROCEED. Simplifier ran clean (no
changes). Both targets build green. Adjudicator's explicit note: this warm-up block is throwaway diagnostic
scaffolding by design — rip it out once the hypothesis is resolved, don't let it calcify into shipped code.
**NEXT STEP: another live capture on Windows** (same method — elevated `shell_profiler --raw > file.txt`,
~15-20 real fan-clicks), then compare `us_warmup_touch`/`us_element_from_handle`/`us_findall_tabctrls`
against this session's numbers to settle materialization-vs-scope before writing any real fix. **Caching the
TabControl element across minimize/restore stays explicitly rejected** — a stale post-restore UIA element can
return silently-wrong data (S_OK, no thrown exception, no FAILED(hr)), risking the exact "select the wrong
tab silently" bug `ActivateTab` already guards against; only revisit if guided descent (once/if it's the
chosen fix) proves insufficient, and only with explicit staleness validation added. PowerBI model can be
extended the same way once more segment data exists.
Feature A (pill icon-fallback) is parked, code-complete, on branch `feat/pill-icon-fallback` (69064ee/bfb1d54,
based off this branch) — not merged, picked up whenever.
Tiny doc polish DONE this session (ef2f51c): CLAUDE.md rule 4 + project blurb reworded, ARCHITECTURE.md's
stale current-state AppBar/"dock strip" mentions fixed (historical Stage 1 narrative section left as-is,
deliberately — that's changelog, not live spec). All Windows visual checks below still pending.
Debt: simplifier pass attempted twice on the gate1/gate2 instrumentation diff (`ActivateTab`,
`activatetab-complexity` debt) — both attempts hit a transient "529 Overloaded" API error before doing any
work (0 tool calls). Not yet successfully run; retry in a future session, low priority (diagnostics-only diff,
already adjudicator-clean).
**Meanwhile (non-Windows sessions, can't runtime-verify the above): profiler P.1 workstream progressed —
see 2026-07-08 session log entry. Remaining P.1 sites (`WinEventCallback`, `UiaSnapshot`, `StoreUpdate`,
`LauncherAction`) are one-liners, fair game for a non-Windows session same as this one.**
Active workstream: **taskbar-chip rework** — kill the dock, put minimized-window chips in the taskbar gap
(plan: `~/.claude/plans/dreamy-stirring-walrus.md`; feasibility: `docs/research/taskbar-chip-feasibility.md`).
Stage 1 done: chips (minimized windows, title-only, insertion-ordered) render side-by-side in the gap
overlay next to the automation pills (chips first, pills fill leftover + drop first, 4px edge dead-zone);
`WM_NCHITTEST` covers chips+pills; clicking a chip restores its window (`kChipClickMsg`→`RestoreWindow`);
`RefreshContent()` re-fits on chip-set change.
Stage 2 done: hover a chip → fan opens above it (`WM_NCHITTEST`-driven hover, live-cursor via GetCursorPos;
overlay posts `kChipHoverMsg`→dock `ShowFanForChip`→`ChipRectScreen` anchor→`FanPopup::Show`); 150ms grace +
`BeginGrace`/timer cursor-in-fan guard bridges the chip→fan seam; fan row click still uses the existing
`kFanActivateMsg` tab-activate flow.
Stage 3 done (the AppBar-transition commit): dock KILLED. `DockWindow` is now a never-shown hidden
coordinator — deleted all `ABM_*`/`m_abd`/`AppBarSetPos`/`AppBarRemove`/`kCallbackMsg`/`DockHeightPx`, dock
paint, dock mouse handlers, `kHoverTimer`+delayed-switch, `CardAt`/`ButtonAt`/`DockButtons`/`ShowFanFor`,
and the `kGapStateMsg`/`m_gapActive`/`m_gapResolved` single-host fallback (+ overlay `PostState`/`m_stateMsg`).
`main.cpp` crash filter (its only job was `ABM_REMOVE`) deleted. Overlay gained `TaskbarMonitor()`
(`FullscreenOnDockMonitor` sources its monitor there now — hidden host is 1x1 at origin); safety-timer gate →
`!overlay->Shown()`. **Only `SHAppBarMessage` left in src/ is `ABM_GETSTATE` in `IsAutoHide` (query).**
Rule 4 now vacuously satisfied (no registration → no removal obligation). Both targets build green;
inspector burst (AppBar-hygiene + teardown + threading + taskbar-geometry) → adjudicator MAY PROCEED.
**Windows visual check pending (Stages 1–3):** minimize 2-3 browsers → title chips in gap; hover a chip →
fan opens above it, slide up into fan (must NOT vanish), click a row → window restores + that tab activates;
move to another chip → fan repositions instantly (no dwell/hijack); click a chip → window restores;
**no reserved strip above the taskbar remains, exit leaves no dead space**; Start/Search flyout + fullscreen
still suppress the overlay; config hot-reload still works.
Debt: [S2-getcursorpos] NCHITTEST hover skips update if GetCursorPos fails (unreachable on live UI thread;
WM_MOUSELEAVE recovers) — logged, not fixed.
Debt (from Stage 3 adjudication, non-blocking):
- [S3-rule6-flyout] Start/Search process-name heuristics (`StartMenuExperienceHost.exe`/`SearchHost.exe`)
  live in `DockWindow::UpdateOverlaySuppression`, not `TaskbarOverlayWindow` (rule-6 drift; pre-existing).
- [S3-taskbarmon-openprocess] ✅ RESOLVED 2026-07-05 — `TaskbarMonitor()` now caches the verified tray
  HWND (`m_uiTray`, UI-thread-only, `IsWindow`-guarded, dropped on `TaskbarCreated`/display/DPI change);
  `MonitorFromWindow` recomputed cheaply each call. Worker's `MeasureGap` keeps its own `FindTaskbar`.
- [S3-gap-shutdown-leak] `TaskbarOverlayWindow::Destroy` join-then-DestroyWindow can drop one queued
  `kApplyGapMsg` `Gap*` (shutdown-only; OS reclaims the heap).

Prior (Stage 5) next action, now superseded by the rework: user visual check of Phase 5b + re-verify Stage 1–4 (§12).
Phase 5b is CODE COMPLETE (5b.1 accepted; 5b.2 pills-in-gap; 5b.3 event-driven re-measure + single-host
dock fallback). **Windows visual check pending** for 5b.3: open many apps → gap shrinks, pills drop, dock
strip stays empty; close apps → pills grow back in the gap; make the gap fail (too small) → pills reappear
in the dock strip (fallback); edit config while gap-active → pills update in-place; empty-gap right-click
still opens taskbar menu. Then run §12 acceptance rows (esp. row 5b) + re-check Stage 1–4 rows on Windows.
Geometry + UIA element reference: `docs/research/win11-taskbar-geometry.md`. `TaskbarOverlayWindow.{h,cpp}`
isolates ALL taskbar heuristics (hard rule 6). Carried debt: host-handoff paints one doubled frame ~tens
of ms at startup/reload (transient); overflow-chevron class unconfirmed (live overflowed taskbar); fence
`ABM_GETSTATE`/worker-join vs hung explorer at shutdown; bounded one-`Gap` shutdown leak; RoundRect
radius-vs-diameter (cosmetic). Also still pending 5a check: dock grows on minimize; config hot-reload ~1s
(%LOCALAPPDATA%\browser_shell_os\config.txt). Profiler workstream still unstarted (see profiler.md).

Deferred debt:
- [F-02 activate-com-hang] ✅ RESOLVED 2026-07-04 — ~TabReader now bounded-joins (2s wait on a
  lifetime-safe shared ExitSignal), then join()s a clean exit or detach()es a worker wedged in an
  uninterruptible cross-process UIA/COM call (ActivateTab Select/SetFocus/DoDefaultAction OR
  SnapshotTabs FindAll). WM_DESTROY → AppBarRemove can no longer stall (hard rule 4). Residual: a
  detached worker touching freed members is shutdown-only UB, unobservable (process exiting) — documented
  at the fix site. Burst (AppBar-hygiene + threading) → adjudicator MAY PROCEED. Isolated in TabReader.
- [renderer-tiny-card] Very narrow cards (rowW < ~48px, i.e. many minimized windows) drop the
  "+N" overflow indicator silently. Degenerate many-window case; revisit if window count grows.
- [tabreader-locale] CleanTabTitle strips English suffixes only (" - Sleeping", " - Pinned",
  " - Memory usage - N MB"). Non-English Chrome/Edge won't strip. Isolated in TabReader (rule 6 OK).
- [F-01 threading] g_dockHwnd non-atomic; CrashFilter reads from faulting thread. HARD GATE:
  must fix (std::atomic<HWND>) before Step 2.3 checkpoint (first worker thread). Also marshal
  SHAppBarMessage call to UI thread.
- [DPI] Mixed-DPI AppBarSetPos monitor/DPI-source mismatch — defer to multi-monitor stage.
- [fanactivate-telemetry-blindspot] `FanActivateLatency`'s `outcome=Selected` cannot distinguish a
  correct-tab activation from a wrong-duplicate-title match (`ActivateTab`'s title-first/fallbackIndex-
  tiebreak scheme) — confirm passes either way. Telemetry does NOT de-risk deferring the queued RuntimeId-
  matching fix; don't read a healthy `Selected` rate as proof activation is picking the right tab.
- [activatetab-complexity] `ActivateTab` (TabReader.cpp) now interleaves UIA gate/retry logic with latency
  stopwatch bookkeeping (`tTabFoundUs`/`tSelectAttemptUs`/`tConfirmUs`, `Finish()` lambda) — at the edge of
  rule-6 isolation working against maintainability. Next substantive change to this function should
  consider extracting the gate loop from the telemetry capture.
- [activatetab-restore-to-tabfound-bottleneck] CONFIRMED with live `FanActivateLatency` data (2026-07-08,
  7 real clicks, elevated `shell_profiler --raw`): `us_restore_to_tabfound` averages 452ms and is **74% of
  the 602ms average total** — click→restore (1.3ms) and select→confirm (72ms) are noise by comparison.
  This is `ActivateTab`'s readiness-gate/tree-gate retry loop (`TabReader.cpp`) waiting for the restored
  window's UIA tab tree to become walkable again — matches the earlier spike-test note ("~330ms single UIA
  walk latency, not retries") but the live retry-loop number runs higher, so there's retry overhead on top
  of the raw walk cost worth isolating. This is the concrete next target for the "tree-walk is the future
  optimization target" note elsewhere in this file — not a new problem, but now it has numbers instead of a
  vibe. Raw per-click data + a dashboard: `scratchpad` on the machine that ran the capture (not checked into
  the repo — regenerate via `shell_profiler --raw` if needed, see profiler/README.md).

**Build note (this machine):** VS2022 Pro's C++ install now works — the
canonical CLAUDE.md commands (`cmake -B build -G "Visual Studio 17 2022"`,
`cmake --build build --config Debug`) succeed. Old NMake/VS2019 workaround
obsolete (2019 BuildTools partially removed). `C:\Program Files\CMake\bin`
still needs adding to PATH.

## Doc map

| Doc | What it is | When to read |
|---|---|---|
| `CLAUDE.md` | Hard rules + session protocol | Always |
| `docs/HANDOFF.md` | This file: state + next action | Always, first |
| `docs/plans/stage-N.md` | Step breakdown w/ checkpoints for stage N | Only the current stage |
| `docs/plans/profiler.md` | Observability workstream steps | Only when working it |
| `docs/ARCHITECTURE.md` | Full design: components, APIs, risks, acceptance tests (§12) | The section for the current stage; §12 row after every step |

## Key invariants (details in CLAUDE.md — these are the expensive ones)

- No AppBar registered today (chip-rework removed it); `ABM_REMOVE` on every exit path only applies if one is ever re-registered.
- One UI thread; workers talk via `PostMessage` only.
- Win32 only; no frameworks; no third-party deps without approval.
- Profiler is separate software; shell telemetry is ETW TraceLogging only.
- Never claim runtime verification that wasn't done on Windows.

## Update protocol for this file

After each completed step: flip the status board entry (⬜→✅ for finished
stages, move the **NEXT** marker), rewrite the "Next action" line, and append
one line to the session log. Keep this file short — prune, don't accumulate.

## Session log (append one line per work session)

- 2026-07-08 — Landed Opus's 5-field `FindLiveTabItems` split (`TabReader.cpp`): new `WalkTiming` struct
  times `ElementFromHandle`, the TabControl `FindAll(Descendants)` (prime suspect), the per-candidate
  `IsInsideDocument` parent-walk (accrued over every candidate incl. `continue`d ones), the TabItem
  `FindAllBuildCache` (accrued only when it runs), and a `tabctrl_candidates` count — all from the winning
  gate-2 call only, mirroring the existing `tLastWalkUs` overwrite pattern. Appended to `FanActivateLatency`
  strictly after all pre-existing fields (positional TDH decode preserved); `Contract.h` + `ARCHITECTURE.md`
  §10 updated to match. Threading-rule-violations inspector (only applicable lens — diagnostics-only diff,
  no lifetime/exit-path or paint/sizing code touched) → clean → adjudicator MAY PROCEED. Simplifier ran clean
  on this new diff (found nothing to change — matches the file's established timing idiom); does NOT resolve
  the still-open `activatetab-complexity` debt below, which is about the earlier already-committed gate1/gate2
  diff, a separate simplifier target. Both targets build clean on Windows. Not yet done: an actual live capture with this
  instrumentation (elevated `shell_profiler --raw`, ~15-20 real fan-clicks) — that's the next session's first
  move, per the Next-action note above; only after reading those numbers should a fix be chosen.
- 2026-07-08 — 23-click live capture of the 5-field `FindLiveTabItems` split (elevated `shell_profiler --raw`,
  same UTF-16LE-via-Grep-tool workflow as the 19-click capture) + Opus re-review + a follow-up warm-up-touch
  probe landed in response. Capture confirmed `gate2_attempts=1` in all 23 rows (retries still ruled out) and
  gave a clean breakdown of the walk: `us_findall_tabctrls` 54.7% avg (still the largest single piece, as
  anticipated), but `us_element_from_handle` came back 31.0% avg — NOT one of the two pre-registered fork
  branches. Sent the full table to an Opus review agent (per this investigation's "check with Opus before
  choosing a fix" precedent): Opus's read is that 101ms average for a normally-near-instant HWND→element
  lookup means `ElementFromHandle` is paying the cost of being the *first* UIA call after restore — Chromium's
  lazy accessibility-tree materialization tax — not genuine lookup work, and `FindAll` (step 3) likely pays a
  second installment of that same tax while walking the still-inflating tree. Opus's conclusion: guided
  descent's real ceiling is unknown until materialization-vs-scope is disentangled, so writing it now risks
  effort on a sub-55%-or-much-less win; also confirmed `us_is_inside_document` at 6.5% rules out the
  candidate-culling branch entirely (`tabctrl_candidates` avg 2.17, never 1, but rejecting them is cheap).
  Opus's recommended next code change (implemented this session, user confirmed via AskUserQuestion before
  writing it): a throwaway diagnostic "warm-up touch" — one `automation->ElementFromHandle(hwnd, &warmupElem)`
  fired as early as possible in `ActivateTab`, before Gate 1's poll-sleep, timed, result/HRESULT fully
  discarded, reported as append-only `us_warmup_touch` on `FanActivateLatency`. `tWarmupUs` had to be hoisted
  above the `Finish` lambda's `[&]` capture (first attempt hit a real compile error — lambdas can't capture a
  not-yet-declared reference — fixed by moving the declaration up next to the sibling diagnostic locals, same
  pattern `tReadyUs`/`gate1Attempts` already use). `Contract.h` + `ARCHITECTURE.md` §10 updated. Threading-
  rule-violations inspector (only applicable lens) → one INFORMATIONAL nit (warm-up call isn't `stop`-
  cancelable — same accepted shape as the pre-existing walk's UIA calls, not a new risk class) → adjudicator
  MAY PROCEED, with an explicit note that this is throwaway scaffolding to rip out once the hypothesis
  resolves. Simplifier ran clean (no changes on either this diff or the earlier 5-field split's diff — the
  latter's simplifier run succeeded on the first attempt this session, unlike two prior transient-error
  attempts logged as debt below for a *different*, still-committed diff). Both targets build clean
  (had to `taskkill` a leftover `browser_shell_os.exe` PID holding the link lock, same as prior sessions).
  Not yet done: a live capture WITH this warm-up field — that's the next session's first move, per the
  Next-action note above.
- 2026-07-08 — Restore-to-tabfound root cause LOCALIZED (no code changed this entry — analysis only,
  continuing the same-day instrumentation work below). Live capture: 19 real fan-clicks, elevated
  `shell_profiler --raw` redirected to a file (`--csv`'s MetricsView can't show per-event fields, only
  aggregates `duration_us` — learned this the hard way mid-session after wrongly suggesting `--csv` first;
  `--raw > file.txt` is what actually exposes the new gate1/gate2/walk fields per click). Capture file was
  UTF-16LE (Windows console redirect) — plain `grep`/`awk` silently matched zero lines until piped through
  `iconv -f UTF-16LE -t UTF-8` first; the dedicated Grep tool handled the encoding fine on its own. Computed
  stats across all 19 `outcome=Selected` samples: `gate2_attempts` exactly 1 in 100% of rows (0 ever needed a
  retry), `us_gate2_wait` (283ms avg) matches `us_first_walk` (283ms avg) to within 12µs — proving the gate-2
  wait time IS a single `FindLiveTabItems` call's duration, not retry overhead. `us_gate1_wait` avg 80ms/15%,
  `us_gate2_wait` avg 283ms/78% of the ~541ms total. Sent full data + interpretation to Opus for the fix-
  direction call (per user's explicit "check with opus often" ask on this investigation): Opus independently
  re-derived the same conclusion via the us_gate2_wait≈us_first_walk cross-check, ruled out survivorship bias
  (gate2_attempts increments unconditionally before the walk, so a retried case cannot misreport as 1), flagged
  one honest caveat (this batch is all-success, so "no retries" is proven only for the happy path, not for
  failure/retry cases we haven't sampled), and gave a decisive recommendation: do NOT attempt a fix yet — run
  ONE more instrumentation pass splitting `FindLiveTabItems`'s 3 internal UIA calls (`ElementFromHandle`,
  `FindAll(Descendants)` for TabControls — prime suspect, walks the whole window incl. the web-content
  document that then gets discarded via `IsInsideDocument` — and `FindAllBuildCache(Descendants)` for
  TabItems) before choosing between guided-descent (if the TabControl search dominates) or accepting it as
  inherent Chromium lazy-rebuild cost and switching to perceived-latency UI instead. Opus also explicitly
  rejected caching the TabControl UIA element across minimize/restore as unsafe to attempt first: a stale
  post-restore element can return silently-wrong data (S_OK, not a catchable exception or FAILED(hr)),
  risking exactly the "select the wrong tab silently" failure mode `ActivateTab`'s existing design already
  guards against — only revisit if guided descent proves insufficient, and only with explicit staleness
  validation added. Built an HTML artifact visualizing the 19-click breakdown (stacked composition bar +
  per-click table) — private, not committed to the repo. Two simplifier-agent attempts on the earlier
  instrumentation diff both hit a transient "529 Overloaded" server error (0 tool calls each) — not yet
  successfully run, logged as debt above. Next session: implement the 3-way `FindLiveTabItems` split per
  Opus's recommendation, capture again, then and only then pick a fix.
- 2026-07-08 — Restore-to-tabfound latency instrumentation (commit 67e90b4). User flagged this as
  high-stakes ("check with opus often, we can't get this wrong"), so every decision point went through an
  Opus pass before code: (1) Opus (win32-scout) research on whether the 452ms/74–75% `us_restore_to_tabfound`
  cost is one slow UIA walk, retry churn, or COM setup overhead — inconclusive from public docs alone (100–
  500ms is a known band for Chromium's lazy accessibility-tree rebuild after restore-from-minimized, but too
  wide to fix blind); recommended instrumenting before touching the fix. (2) Opus review of the exact
  instrumentation plan caught a real gap: timing only the first Gate-2 retry attempt couldn't distinguish
  "many fast-failing retries" from "retries that get slow only once the tree is mid-rebuild" — required
  timing every attempt (first + last, not just first) and restructuring so `FindLiveTabItems` is called
  exactly once per loop iteration (both applied). Implemented in `ActivateTab` (`src/TabReader.cpp`): new
  `FanActivateLatency` fields `us_gate1_wait`/`gate1_attempts` (window-visible wait), `us_gate2_wait`/
  `gate2_attempts` (UIA-tree-walkable wait), `us_first_walk`/`us_last_walk` (duration of first vs. most
  recent tab-tree walk) — diagnostics only, no behavior change, `us_restore_to_tabfound` kept unchanged for
  PowerBI-dashboard continuity. Threading/control-flow inspector confirmed both Opus-mandated invariants held
  in the actual diff (walk called exactly once per iteration; pre-existing `CancelableSleep` byte-identical) →
  adjudicator MAY PROCEED (one cosmetic field-order nit fixed: new fields now literally appended after all
  pre-existing ones, matching the "append-only positional decode" doc claim exactly). `profiler/Contract.h` +
  `docs/ARCHITECTURE.md` §10 updated — no profiler code change needed (TraceLogging is self-describing).
  Both targets build clean (had to `taskkill` a leftover `browser_shell_os.exe` holding the link lock).
  **Not yet done: an actual live capture with this instrumentation** — that's the next session's first move,
  and only after reading those numbers should a real fix be attempted. Also this session: finished Feature A
  (pill icon-fallback) end-to-end — see its own commits — and parked it on branch `feat/pill-icon-fallback`
  (not merged) per user request, so it doesn't block this latency work; finished the PowerBI model from the
  prior session's live capture (sort order, 4 measures, caught and fixed a real ~4x bug in the first draft of
  `Total Avg Latency (ms)`).
- 2026-07-08 — First live profiler run + root-cause find for the fan-activate lag. Ran
  `browser_shell_os.exe` + elevated `shell_profiler.exe --csv`/`--raw` together on Windows (first time the
  two have run together — P.2–P.4 were "builds green" only before this). All 6 events fired and decoded;
  `--csv` MetricsView confirmed `FanActivateLatency` `duration_us` ~600ms average, matching the long-known
  "perceivable lag" baseline. `--csv` can't split `FanActivateLatency`'s sub-fields though (MetricsView only
  aggregates the single `duration_us` field per event) — switched to `--raw` for 7 real fan-clicks and got
  the actual segment breakdown: `us_click_to_restore` 1.3ms, **`us_restore_to_tabfound` 452ms (74% of the
  602ms total)**, `us_tabfound_to_select` 77ms, `us_select_to_confirm` 72ms. Root cause is now numbers, not
  a guess — see the new `activatetab-restore-to-tabfound-bottleneck` debt entry above; this is the concrete
  target the old "tree-walk is the future optimization target" note was gesturing at. Built a dashboard
  artifact of the breakdown (stacked-bar per click + table) and two CSVs (wide + long) for the user to pull
  into PowerBI, which was already open on their machine. No official PowerBI MCP server was connected;
  registered Microsoft's local `powerbi-modeling-mcp` (user scope, via `claude mcp add`) after confirming
  via its docs that it only authors/edits an *existing* semantic model (tables/measures/DAX) — it can't
  create a dataset from a CSV or build report-canvas visuals, so the real division of labor is: user does
  Get Data → CSV in Desktop (already done, imported the long CSV), MCP tools (once the session restarts —
  a server added mid-session doesn't appear in this session's tool index) can shape the model from there
  (rename/type columns, add measures) but won't draw the chart. No shell code touched this session — pure
  profiling + tooling. Nothing committed to the repo from the live-capture data itself (lives in the
  session's scratchpad on the machine that ran it, regenerable via `shell_profiler.exe --raw`).
- 2026-07-08 — User confirmed on Windows: overlay-instability fix (2026-07-05) holds — no AV crash, no
  stuck visible-but-empty overlay under terminal churn, fullscreen suppression latency OK. Former TOP
  PRIORITY closed. Then finished profiler P.1 (non-Windows session): wired the four remaining call sites.
  `WinEventCallback` in `HostWindow::WinEventProc` (`src/HostWindow.cpp`, right before the browser-window
  `PostMessageW` — NOT the fg/taskbar LOCATIONCHANGE hooks, those are unrelated internal plumbing) — event
  id + hwnd. `UiaSnapshot` wraps the `SnapshotTabs` call in the worker loop (`TabReader.cpp`, around the
  existing `Snapshot` request branch) — duration_us, hwnd, tab_count, and a coarse hr proxy
  (S_OK/E_FAIL/E_HANDLE) since `SnapshotTabs` itself has many internal UIA-call early-returns not worth
  restructuring to thread a real HRESULT out (same complexity trade-off already flagged as
  `activatetab-complexity` debt). `StoreUpdate` added to `Store::Set/SetTabs/Remove` (`Store.cpp`) via a new
  private `TraceUpdate()` helper (tracked_windows + total_tabs) — `SetMinimized`/`MarkTabsStale` don't
  change either count so left untraced (avoids identical-payload noise). `LauncherAction` added to
  `Launcher::Execute`'s detached MTA worker thread (`Launcher.cpp`) — action type, duration_us, and hr
  (HRESULT_FROM_WIN32(GetLastError()) on ShellExecuteW/CreateProcessW failure); cross-thread TraceLoggingWrite
  is safe by the same precedent `FanActivateLatency` already established from TabReader's worker thread.
  P.1 now ✅ code-complete (all 6 events from ARCHITECTURE §10 wired); P.2–P.4 already were. Both targets
  build clean. Checkpoint burst skipped as inapplicable to every site (self-reviewed): no new threads, no
  lifetime/exit-path changes — just TRACE_EVENT calls added to already-existing UI-thread callbacks and
  worker threads. Only §12 row P runtime acceptance (wpr/traceview showing all 6 events fire, elevated) is
  left, on Windows.
- 2026-07-08 — Profiler P.1 continued (non-Windows session; runtime-verify TOP PRIORITY above still
  blocked on a Windows session). Wired `TaskbarOverlayWindow::Paint` (`src/TaskbarOverlayWindow.cpp:613`)
  with `TRACE_EVENT("Paint", duration_us, dirty_w, dirty_h)` — this is the real per-frame paint site now
  (HostWindow itself is a hidden 1x1 coordinator, never paints). Resolved the open question from the prior
  session's profiler.md note: dropped `AppBarNegotiate` from the contract entirely — chip-rework Stage 3
  fully removed the AppBar (rule 4 vacuous), so there is nothing left to negotiate; updated
  `docs/ARCHITECTURE.md` §10 event table + §12 row P, `profiler/Contract.h`, and `docs/plans/profiler.md`
  to match. Both targets build clean. Checkpoint burst skipped as inapplicable — no lifetime/exit-path
  change (AppBar-hygiene lens) and no new threading (threading lens); change is a two-field addition to an
  existing UI-thread-only paint path, self-reviewed against the FanActivateLatency TRACE_EVENT precedent.
  Also backfilled this file: the immediately-preceding session (commits 4d226c1/cf07cac/64105a5) landed
  `src/Trace.h`/`Trace.cpp` (provider + `TRACE_EVENT`/`TRACE_SCOPE` macros, register/unregister in
  `wWinMain`) and the `FanActivateLatency` call site in `TabReader.cpp::ActivateTab` (outcome +
  click→restore→tabfound→select→confirm latency chain) but never updated the status board/session log —
  status board profiler row was stale until this entry. Remaining P.1 sites (`WinEventCallback`,
  `UiaSnapshot`, `StoreUpdate`, `LauncherAction`) still unwired, each one-liners for a future session.
- 2026-07-06 — Fan z-order + flicker fixes (branch `dev/fan-icon-flicker-research`, commit 366c0e4). (1) Fan z-order: kSafetyTimer→ReassertVisibility→ApplyGap was re-raising overlay TOPMOST above the fan every 1.5s → pills rendered on top of fan rows in two-row layout. Fix: `m_topSibling` (fan HWND) registered via `SetTopSibling` (called from HostWindow::Create after both windows exist); `RaiseSibling()` re-raises fan above overlay after every overlay TOPMOST re-assert (in ApplyGap fits-branch + ReassertVisibility). (2) ReassertVisibility no-op gate: replaced full `ApplyGap(m_lastGap, false)` with targeted check — if shown+healthy, `SetWindowPos(SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOREDRAW)` + RaiseSibling (no repaint); if shown+empty, `ShowWindow(SW_HIDE)` (self-heal). Eliminates the 1.5s periodic repaint flicker. (3) Double-buffer `Paint()`: render chips+pills into a memory DC (CreateCompatibleDC/CreateCompatibleBitmap + null-guard), BitBlt once — eliminates colorkey flash-through frame. (4) `bErase=FALSE` in InvalidateRect (semantically correct; WM_ERASEBKGND=1 already suppressed). Inspector burst (threading + AppBar-hygiene) → adjudicator MAY PROCEED. GDI null-guard applied inline. Build clean (both targets). Runtime/visual verify pending on Windows. Research: Opus confirmed items 1–3 of the flicker strategy apply; item 2 (ResolveActiveTab) skipped — no active-chip concept exists; VisualState enum skipped — Store already commit-before-invalidate. No icon rendering exists yet (ButtonStyle::Icon parsed but DrawButton ignores it — dead weight until Feature A implemented).

- 2026-07-05 — **Zero-latency fullscreen suppression restored (user rejected the 1.5s-poll trade); bug memory
  closed per user ("it's okay now, not production-ready").** The prior entry's fix routed in-place fullscreen
  through the 1.5s safety poll (0→1.5s latency). User wanted instant back. Re-added the fg watch but in the shape
  that avoids the crash suspect: ONE set-once system-wide EVENT_OBJECT_LOCATIONCHANGE hook (idProcess=0/idThread=0,
  set in Create, NEVER re-scoped — the old per-foreground unhook+hook storm was the churn/crash suspect). Callback
  (`WinEventProc`, dispatched by `hHook==s_fgLocationHook`) hard-filters to `idObject==OBJID_WINDOW &&
  hwnd==GetForegroundWindow()` → posts kFgLocationMsg → 120ms kSuppressTimer debounce → UpdateOverlaySuppression.
  Re-added: kFgLocationMsg/kSuppressTimer/kSuppressMs, s_fgLocationHook, m_winEventHookFgLocation; killed in
  WM_ENDSESSION+WM_DESTROY; unhooked (+null) in WM_DESTROY. ReassertVisibility()+safety poll kept as backstop.
  Honest risk: this reintroduces a fg LOCATIONCHANGE hook — removing the fg hook is what the user confirmed made
  it stable — so the AV *could* return; if so, the set-once global hook is the new suspect. Build+link clean.
  Inspector burst (threading+teardown) → 3 findings, ALL informational/pre-existing (GetForegroundWindow-in-
  callback = deliberate flood filter; WM_ENDSESSION-no-unhook = same as every hook in the file, OS reclaims;
  post-WM_DESTROY timer re-arm self-kills behind a guard) → adjudicator CHECKPOINT MAY PROCEED (no regression;
  set-once hook strictly better than re-scoping). Simplifier skipped (vetted hook/timer plumbing). RUNTIME/visual
  verify pending on Windows (see TOP PRIORITY).
- 2026-07-05 — **Overlay-instability FIX applied (was the OPEN top-priority bug below); runtime-verify pending
  on Windows.** Root-cause direction confirmed: killed the per-foreground-window EVENT_OBJECT_LOCATIONCHANGE hook
  (`HookForegroundLocation`/`m_winEventHookFgLocation`/`s_fgLocationHook`/`kFgLocationMsg`/`kSuppressTimer`/
  `kSuppressMs`) — it re-scoped on EVERY foreground change and hammered a race path under rapid churn. In-place
  fullscreen (F11/video/borderless, no EVENT_SYSTEM_FOREGROUND) is now detected by the existing 1.5s `kSafetyTimer`
  poll only (`UpdateOverlaySuppression`), plus the immediate EVENT_SYSTEM_FOREGROUND path for the common cases —
  far fewer events (matches user's "stop listening to so many non-browser events"). Trade: fullscreen suppression
  latency ~0→1.5s (acceptable). Hardened the stuck visible-but-empty state: new
  `TaskbarOverlayWindow::ReassertVisibility()` = `ApplyGap(m_lastGap, /*hysteresis*/false)` called each safety tick
  — hides a shown-but-empty overlay (from a lost `kApplyGapMsg`) and re-asserts `HWND_TOPMOST` against a taskbar
  re-layout that z-occluded it; the `!Shown()`→RequestMeasure recovery for stuck-hidden stays. Kept the debounced
  taskbar-LOCATIONCHANGE gap re-fit (explorer-PID hook) — user still wants re-fit when a terminal opens.
  WinEventProc's LOCATIONCHANGE branch is now taskbar-only. Both targets compile+link clean (had to regen VS2022
  cache — stale VS2019). Inspector burst (threading + teardown/exit-path) → BOTH no findings → adjudicator
  CHECKPOINT MAY PROCEED (informational: ReassertVisibility doesn't gate on m_measurePending — intentional, that's
  the self-heal for a lost measure; cosmetic 1-frame stale at worst on the slow tick). Simplifier skipped
  (deletion-heavy vetted threading change). RUNTIME/visual verify pending on Windows (see TOP PRIORITY above).
- 2026-07-05 — **OPEN BUG (top priority next session): overlay instability on first real run.** The chip-rework
  build finally ran (link was blocked all prior sessions by the live dock). Two failure modes when the user
  rapidly opens/closes a terminal (taskbar churn) with a browser minimized: (1) AV `0xC0000005` — process dies
  (WER: fault RVA 0x93fb, maps near a folded std::wstring ctor / Store region — unreliable, COMDAT folding);
  (2) overlay STUCK visible-but-empty — window IsWindowVisible=true + sized to gap (353x48) but paints 0
  chips/0 pills → transparent → "gone", process ALIVE. HEISENBUG: any layout perturbation (crash probe, ASan)
  masks the AV → race or uninitialized-read; MSVC ASan found NO heap UAF/overflow. NOT reproducible in dev env
  with notepad churn @200% DPI (overlay.log showed chips=1 pills=2 painting fine); user env differs (user
  overlay 353px≈100%, dev 706px@192 → possible mixed-DPI/multi-monitor). PRIME SUSPECT: this session's
  fullscreen fix (c3d0caa) added a per-foreground-window EVENT_OBJECT_LOCATIONCHANGE hook
  (`HookForegroundLocation`, re-scoped on EVERY foreground change) + kFgLocationMsg/kSuppressTimer + always-on
  safety-timer suppression re-derive → rapid foreground churn hammers it. RECOMMENDED FIX: drop the per-fg
  LOCATIONCHANGE hook; detect in-place fullscreen via the 1.5s safety-timer poll only (keeps feature, ~1.5s
  latency, far less churn — matches user's "don't listen to so many events" ask). Also harden the measure
  re-fit against the stuck visible-but-empty state (shown window with empty GapChipLayout, and re-assert
  HWND_TOPMOST in case the overlay is z-occluded behind the taskbar after a taskbar re-layout). KEEP gap re-fit
  on taskbar change (debounced) — the user still wants it to re-fit when a terminal opens. Diagnostics used
  (all reverted/uncommitted): crash probe (SetUnhandledExceptionFilter+dbghelp StackWalk→crash.txt), overlay.log
  paint logging, ASan build (/fsanitize=address, strip /RTC1), /MAP symbol resolve. Also this session: metallic
  theme polish committed (specular FillMetallic on pills+chips, chips brightened to match) — user wanted the
  metallic to read right on chips.

- 2026-07-05 — Feature D (themes+gradient) + fullscreen-suppression fix, both by parallel Opus worktree agents,
  merged clean (non-overlapping files). D: `Paint::Theme` struct + `FillVGradient` (msimg32 `GradientFill`) +
  `PaintUtil.cpp` themes slate(default,dark top-lit metallic)/matte(flat=old look)/steel; `Renderer::FillRoundedThemed`
  (clip round rgn → gradient/flat → border) rewrites DrawButton/DrawChip; FanPopup rows themed; `Launcher` parses
  optional `theme=<name>`; host calls `Paint::SetActiveTheme(ThemeName())` at Create + config-reload (live re-skin
  via ConfigWatcher). Fullscreen fix: overlay stayed visible on IN-PLACE fullscreen (F11/video/borderless) — no
  EVENT_SYSTEM_FOREGROUND fires; safety timer only re-derived while already suppressed; ABN_FULLSCREENAPP dead
  since Stage-3 AppBar removal. Fix: thread-scoped fg-window `EVENT_OBJECT_LOCATIONCHANGE` hook
  (`HookForegroundLocation`, re-scoped each foreground change, routed via `s_fgLocationHook`, OBJID_WINDOW +120ms
  debounce → UpdateOverlaySuppression) + safety timer now ALWAYS re-derives. Burst: fullscreen lens (threading/
  teardown) 5 nits, no blocker (WM_ENDSESSION dtor-chain unhook = same as all hooks; host is stack obj). GDI lens
  BLOCKED 2: F1 SelectClipRgn(NULL) nuked whole-HDC clip → SaveDC/RestoreDC; F2 rgn rc.right+1 vs border rc.right
  → gradient corner bleed → matched box; +F3 fan inactive text → th.chipText. All fixed, rebuilt clean → MAY
  PROCEED. Build links clean. Runtime/visual verify pending on Windows. Both agent worktrees left for reclaim.
- 2026-07-05 — Quit-affordance regression fix. Stage 3 killed the dock strip AND its right-click-to-quit;
  the hidden host never shows and the gap overlay passes right-clicks to the taskbar → app had NO close path.
  Restored two: (1) right-click a chip/pill → overlay `WM_RBUTTONUP` `PostMessageW(host, WM_CLOSE)` (empty gap
  still HTTRANSPARENT → taskbar menu); (2) global hotkey Ctrl+Alt+Shift+Q → host `RegisterHotKey`/`WM_HOTKEY`
  → `DestroyWindow` (`UnregisterHotKey` in WM_DESTROY). Both hit the existing full teardown→PostQuitMessage.
  FIRST attempt used a `TrackPopupMenu` "Quit" menu — reentrancy burst found 2 BLOCKERS: F2 real UaF (hotkey
  during the modal loop tears down the overlay object whose WM_RBUTTONUP frame is running TrackPopupMenu, then
  derefs freed `self`); F1/F3 worker-reposition/safety-timer hide the menu owner → menu auto-dismissed. Fix:
  DROPPED the modal menu → direct right-click quit (async PostMessage, no nested pump) — erases all 5 findings.
  Teardown lens CLEAN both rounds. Build links clean (dock closed). Runtime verify pending on Windows.
- 2026-07-05 — Overlay persistence + perf hardening (parallel Opus worktree agent, merged clean — 4 files, no
  overlap w/ the purge/fan commits). (1) HIGH: overlay froze forever after an explorer restart — LOCATIONCHANGE
  hook was scoped to explorer's old PID. Fix: `RegisterWindowMessageW(L"TaskbarCreated")` (broadcast to top-level
  windows on taskbar (re)creation) → `HookTaskbarLocation()` re-scopes hook + `InvalidateTaskbarCache` +
  re-suppress + re-measure. (2) HIGH: transient PID=0 during restart left hook null forever → `kSafetyTimer`
  re-hooks whenever `m_winEventHookLocation` is null (no-op in steady state). (3) MED: cached tray HWND kills the
  per-foreground `OpenProcess` (debt S3-taskbarmon-openprocess). (4) LOW: `InvalidateTaskbarCache` on
  DISPLAYCHANGE/DPICHANGED. Rule 6 tightened: `DockWindow` drops its own `FindWindowW(Shell_TrayWnd)` →
  `TaskbarOverlayWindow::TaskbarProcessId()`. WM_DESTROY still unhooks all hooks once (595); no AppBar.
  Compile clean; link blocked only by live dock PID. Agent self-inspected (threading/teardown/geometry) + diff
  re-reviewed against rules 4/5/6. Runtime/visual verify pending on Windows (explorer restart; open/close a
  terminal → gap re-fits). Next: `DockWindow`→`HostWindow` rename, then A (pill icons from exe), D (Theme+gradient).
- 2026-07-05 — Chip-rework Stage 4 started: dead-code purge. Deleted `Renderer::Paint`(dock)/`CardLayout`/
  `CardHit`/`DrawCard`/`ButtonLayout`/`GapButtonLayout` + PaintUtil `kBandHeightDip`/`kBandPadDip`/`kMaxBands`/
  `kChipBg` (all self-referential, zero live callers — overlay paints via `TaskbarOverlayWindow::Paint`→
  `GapChipLayout`+`DrawChip`+`DrawButton`). Grep confirms 0 dangling refs; compile clean (both TUs), link
  blocked only by running dock PID lock (LNK1168, not code). Rename `DockWindow`→`HostWindow` + rule-2/ARCH
  wording DEFERRED behind a parallel Opus worktree agent hardening overlay persistence/perf (survive taskbar
  button changes when opening apps like a terminal; explorer-restart re-hook; cache TaskbarMonitor OpenProcess).
  User design decisions locked: (A) pill icon-fallback icons = extract from target exe; (D) colorway = Theme
  struct + GradientFill first (defer bubbles = alpha/UpdateLayeredWindow rework). B/C = fan polish next.
- 2026-07-05 — Chip-rework Stage 2 done (fan from chips). Overlay: `WM_NCHITTEST`-driven hover (live cursor via
  GetCursorPos so a spurious drag query can't drop hover), `UpdateHover`→`kChipHoverMsg`(HWND/0), `ChipRectScreen`.
  Dock: `kChipHoverMsg`→`ShowFanForChip` (anchors fan above the chip, edge-adjacent) / `BeginGrace` on 0. FanPopup:
  Show params renamed anchor*, grace 200→150ms, `CursorInFan()` guard in `BeginGrace` + WM_TIMER keep-open (arms
  TME_LEAVE) — closes the seam race so the fan can't hide mid-read. Fan row click reuses `kFanActivateMsg` flow.
  Burst (threading/DPI/teardown/hover-seam): threading+DPI+teardown CLEAN; hover-seam → adjudicator BLOCKED F-01
  (seam race) → fixed (BeginGrace cursor guard) → re-burst found residual F-02 (drag NCHITTEST) + timer-arm gap →
  fixed (live-cursor hover + TME_LEAVE in keep-open) → re-burst CLEAN → MAY PROCEED (F1 double-compute folded by
  simplifier; F2 GetCursorPos-fail = debt, unreachable). Simplifier: `ChipHitOf`/`ButtonHitOf`/`CursorInFan` helpers.
  Build green (both targets). Runtime/visual check pending on Windows. Next: Stage 3 kill the dock.
- 2026-07-04 — Chip-rework STARTED (kill dock → taskbar chips). Design steered by Fable consult + win32-scout
  feasibility (both folded into `~/.claude/plans/dreamy-stirring-walrus.md` + `docs/research/taskbar-chip-
  feasibility.md`). Stage 1 done: Store insertion-order (`Ordered()`); Renderer `GapChipLayout` (chips-first,
  pills-fill-leftover-drop-first, 4px edge dead-zone, overflow drops tail) + `DrawChip`; TaskbarOverlayWindow
  takes `const Store*`+`chipClickMsg`, paints chips+pills, `WM_NCHITTEST`/`WM_LBUTTONUP` cover both, chip click
  posts `kChipClickMsg`→dock `RestoreWindow`, `RefreshContent()` re-fits on chip mutations, caches `m_lastGap`.
  Dock unchanged this stage (cards+chips both show — transitional). Burst (5 lenses: taskbar-geometry/DPI/
  threading/teardown/visual) → adjudicator MAY PROCEED (0 blocking). 3 warnings ALL fixed in-step: F-01
  stale-rect reposition guarded by `m_measurePending`; F-02 content-hide skips 300ms UIA hysteresis
  (`allowHysteresis=false`); F-03 stale color-key comment. Simplifier folded `Buttons()`+`ComputeGapLayout()`
  helpers. Build green (both targets). Runtime/visual check pending on Windows. Next: Stage 2 fan-from-chips.
- 2026-07-04 — Gap-pill (Stage 5b overlay) stability fixes — worked w/ win32-scout advisor. User reported 3
  bugs: (1) YouTube/browser fullscreen didn't hide pills; (2) Start-click → pills stuck hidden; (3) intermittent
  flapping. Advisor root causes: (1) ABN_FULLSCREENAPP only moves the DOCK — overlay is a separate topmost
  window w/ no fullscreen path; (2) Start/Search use DWM cloaking (StartMenuExperienceHost.exe/SearchHost.exe)
  → no taskbar geometry event on close → measure-driven overlay never recovers; (3) task-button UIA rects read
  {0,0,0,0} mid-animation → transient invalid → hide/show churn (PID-scoping already handled cursor noise).
  Fixes (isolated in TaskbarOverlayWindow + dock handlers, rule 6): overlay SetSuppressed(force-hide) + ApplyGap
  hysteresis (one 300ms kRetryTimer before hiding a healthy overlay); dock UpdateOverlaySuppression = m_flyoutOpen
  (foreground proc classify) || FullscreenOnDockMonitor (fg rect==dock-monitor rcMonitor ±2px, NOT a latch),
  wired into existing EVENT_SYSTEM_FOREGROUND + ABN_FULLSCREENAPP; periodic kSafetyTimer(1500ms) self-heals any
  missed clearing event (re-derive while suppressed, re-measure while hidden — gated !m_gapActive so no
  steady-state churn). Burst: threading + DPI + stuck-state (3 inspectors) → 4 stuck-state regressions found →
  fixed (dropped m_fullscreenApp latch, added safety timer) → re-burst stuck-state CLOSED → adjudicator
  MAY PROCEED (residuals: OpenProcess AV-stall bounded; flag-divergence unreachable; both nits addressed —
  ±2px tol, safety churn gate). AppBar invariant verified (kSafetyTimer killed in WM_DESTROY + WM_ENDSESSION).
  Build green. ⇒ VISUAL CHECK PENDING on Win11: open/close Start+Search → pills return; drag windows → no
  flapping; YouTube fullscreen in → pills vanish, exit → pills return; multi-monitor fullscreen (edge).
- 2026-07-04 — Carried-debt polish: F-02 (activate-com-hang) RESOLVED. Root cause: ~TabReader's
  unconditional join() could block forever on a worker wedged in an uninterruptible cross-process
  UIA/COM call; since WM_DESTROY resets TabReader (line 702) BEFORE AppBarRemove (line 715), that
  meant ABM_REMOVE never ran → dead strip (hard rule 4). Fix (isolated in TabReader, rule 6):
  bounded-join — set m_stop/notify, wait_for(2s) on a shared_ptr<ExitSignal> the worker sets after
  WorkerLoop returns, then join() clean or detach() wedged. WM_ENDSESSION path already AppBarRemove's
  first (line 441), so only the normal-quit path was exposed. Compile clean (link blocked only by the
  running dock PID 29992 holding the exe lock — not killed). Burst (AppBar-hygiene + threading) →
  adjudicator MAY PROCEED; applied both warnings (comment now names SnapshotTabs path + corrects the
  race-window claim). Residual detach UAF is shutdown-only + unobservable, documented at fix site.
  Simplifier skipped (small vetted threading fix). Runtime verify pending on Windows (close running
  instance to relink). Remaining open debt: renderer-tiny-card, tabreader-locale, DPI multi-monitor.
- 2026-07-04 — Interactive-fan post-accept fixes (user tested steps 1-5, all 3 behaviors work). (A) Multi-window
  fan-nav bug: vertical card stacking means moving from a lower card up to its fan transits the cards above it;
  Step-4 instant-switch hijacked the fan mid-transit. Fix: reverted to DELAYED switch — a newly hovered card
  always (re)starts kHoverTimer(250ms)→ShowFanFor; fast transit to the fan no longer switches, and WM_MOUSELEAVE
  →BeginHoverGrace KillTimer(kHoverTimer) cancels the pending switch as the cursor enters the fan. (B) Gmail/
  GitHub gap pills 16% narrower: GapButtonLayout pillW ScalePx(84)→71. (Tried dock-fallback ButtonLayout 48→40
  too but 40 clips "GitHub" → reverted to 48; user only sees the gap pills.) Burst (visual/layout + threading/
  interaction) → adjudicator MAY PROCEED (3 threading findings all cleared: CancelGrace=KillTimer no-op; ShowFanFor
  same-card re-show idempotent + not a regression; WM_DESTROY order pre-existing+safe). Applied optional hygiene:
  hoisted KillTimer(kHoverTimer) above m_fanPopup.reset() in WM_DESTROY. Build clean. USER-CONFIRMED WORKING;
  these two are refinements (visual re-check optional).
- 2026-07-04 — Interactive-fan Step 5 done → FEATURE CODE-COMPLETE (all 5 steps). Empty-state option (a):
  Renderer::Paint dropped the "no minimized browsers" placeholder — empty dock now paints only bg fill +
  fallback buttons (card loop unconditional, iterates 0×). Paint-only: DockHeightPx still clamps to ≥1 idle
  band (no AppBar churn, no ABM_*/height change). Burst (paint/DPI/GDI-hygiene + AppBar-no-churn) → adjudicator
  MAY PROCEED (one AppBar finding was git-HEAD-vs-working-tree confusion, dismissed; substance clean: 0 AppBar
  calls, exit paths intact, no leaked HFONT). Trivial deletion → simplifier skipped. Build clean.
  ⇒ CONSOLIDATED VISUAL CHECK PENDING on Win11 for the whole feature (steps 1-5): (1) minimize browser → hover
  card → fan shows tabs → slide up into fan (must not vanish) → click tab → window restores + that tab
  activates (~560ms, perceivable lag OK); (2) move off fan → closes ~200ms; hover different card → switches
  instantly; (3) close all browsers → dock empty, NO placeholder text, fallback buttons still show, dock strip
  still reserved. Debt still open: [F-02 activate-com-hang] (see Deferred debt). Throwaway spike at
  scratchpad/spike_activate.{cpp,exe} (SPIKE A/B confirmed) can be deleted.
- 2026-07-04 — Interactive-fan Step 4 done → fan now REACHABLE (hover-bridge). Was a real bug: dock
  WM_MOUSELEAVE→ClearHover hid the fan instantly, so the cursor crossing the card→fan seam killed it before
  a click (Step 3 unreachable without this). Fix: FanPopup grace timer (kGraceTimer, 200ms) + BeginGrace/
  CancelGrace. Fan WM_MOUSEMOVE→CancelGrace (+TME_LEAVE, m_fanTracking-guarded); fan WM_MOUSELEAVE→BeginGrace;
  fan WM_TIMER→Hide. Dock WM_MOUSEMOVE: CancelGrace over a card, instant ShowFanFor on card-switch when fan
  already visible, BeginHoverGrace over empty dock; WM_MOUSELEAVE→BeginHoverGrace (defer, not hide). Grace
  fires only when cursor is on neither window (each window's mouse-move cancels it) → close. Timer killed on
  Hide/Destroy/WM_TIMER (all teardown paths). Burst (threading + AppBar-timer-teardown + DPI) → adjudicator
  MAY PROCEED (only nit: benign redundant KillTimer, idempotent-teardown, left as-is). Simplifier extracted
  BeginHoverGrace() helper (symmetric to ClearHover). Build clean. FEATURE NOW FULLY INTERACTIVE.
  ⇒ VISUAL CHECK PENDING on Win11 (Steps 3+4 together): minimize a browser → hover its card → fan shows tabs
  → slide up into the fan (must NOT vanish) → click a tab → window restores + that tab activates; also move
  off → fan closes after ~200ms; hover a different card → fan switches instantly. Remaining: Step 5 empty-state
  (paint-only, no AppBar churn; lenses DPI/paint + AppBar-assert-no-setpos).
- 2026-07-04 — Interactive-fan Step 3 done → feature now END-TO-END (hover card → fan → click row → tab
  activates). FanPopup: Create(+ownerHwnd,+activateMsg), Show(+targetHwnd), RowAt hit-test (shares Paint's
  ScalePx(6/24) geometry; "+N more" row → -1), WM_LBUTTONDOWN → PostMessageW(kFanActivateMsg, target, idx)
  when idx>=0; MA_NOACTIVATE kept. Dock passes hwnd+kFanActivateMsg to Create, card hwnd to Show. Displayed
  row == original Store tab index (tabs shown contiguous from front) so idx indexes Store's full vector
  directly. Burst (DPI + threading) BOTH CLEAN → adjudicator MAY PROCEED (m_targetHwnd null-guard ruled
  unnecessary: idx>=0 ⟹ m_tabs non-empty ⟹ Show ran ⟹ target set; dock also re-guards via Store). Simplifier:
  no churn. Build clean. VISUAL CHECK PENDING on Win11: hover a minimized browser's card → fan shows tabs →
  click a tab → that window restores & the clicked tab activates. Remaining: Step 4 (hover-bridge grace so
  the fan survives the card→fan gap; lenses threading/DPI/AppBar-timer) + Step 5 (empty-state paint-only;
  lenses DPI/AppBar-assert-no-setpos).
- 2026-07-04 — Interactive-fan Step 2 done + SPIKE A/B CONFIRMED on Win11. Spike run: SetForegroundWindow
  ret=1 no ASFW → tab switched (R1 recipe good; GetForegroundWindow!=target right-after is just async
  restore, harmless since ret=1 skips flash); Select hr=0 confirmed IsSelected=1; tab-tree ready ~330ms
  (single UIA walk latency, NOT retries) → 2.3 constants UNCHANGED (50ms poll / 3s ceilings / 60ms settle).
  ~560ms total = perceivable lag (window-forward→330ms re-walk→confirm); acceptable v1, tree-walk is the
  future optimization target. Step 2 code: DockWindow kFanActivateMsg=WM_APP+7 handler — resolve wanted title
  from Store (pre-restore) → RestoreWindow (restore+SFW FIRST) → RequestActivate(target,title,idx) → fan
  Hide() (close-on-click, user UX). Burst (threading + AppBar-hygiene) BOTH CLEAN → adjudicator MAY PROCEED.
  Folded the one debt it surfaced: IsWindow(hwnd) guard on ActivateTab gate-1 so a click racing a close
  fails fast instead of spinning 3s. Build clean. Next: Step 3 — FanPopup RowAt + row→original-index map +
  POST kFanActivateMsg on WM_LBUTTONDOWN (the wiring that actually fires the handler); lenses DPI + threading.
- 2026-07-04 — Interactive-fan FEATURE Step 1 done (TabReader Activate path). Built combined throwaway
  SPIKE (scratchpad/spike_activate.exe, VS2026 vcvars) reproducing the full flow (NOACTIVATE fan click →
  UI SetForegroundWindow [SPIKE A] → worker UIA re-walk w/ readiness+tree retry → Select → confirm [SPIKE
  B]) — ready to run, PENDING USER on a live browser to confirm R1 recipe + set 2.3 constants. Step 1 real
  code: TabReader.h ActivateOutcome/TabActivateResult/Request(ReqKind Snapshot|Activate), ctor +activateMsg;
  TabReader.cpp CancelableSleep + FindLiveTabItems (keeps live element array, index-parity w/ Tab vec) +
  IsItemSelected + ActivateTab (readiness gate → tree gate → title-first match w/ fallbackIndex tiebreak →
  Select → confirm-via-reread → SetFocus→LegacyIAccessible fallback, NOT Invoke); WorkerLoop Snapshot/Activate
  dispatch; RequestActivate (no de-dupe). DockWindow: kTabActivateResultMsg=WM_APP+8 handler (_DEBUG log +
  UI-thread Store refresh from freshTabs + delete). Burst (threading + COM/resource) → adjudicator BLOCKED
  F-01 (poll sleeps ignored m_stop → join could freeze UI ~6s) → fixed (CancelableSleep threads m_stop into
  every sleep) → re-burst threading CLEAN → MAY PROCEED. F-02 (unbounded COM into hung provider) tracked as
  debt. Simplifier: reused IsItemSelected, markSelected lambda, dropped matchCount. Build clean (shell+profiler).
  Next: user runs spike + reports numbers; then Step 2 (dock kFanActivateMsg + restore-first) — kFanActivateMsg
  reserved WM_APP+7. kFanActivateMsg NOT yet added (step 2).
- 2026-07-04 — Feature DESIGN (parallel thread): `docs/plans/feature-interactive-fan.md` — interactive fan
  (click a fan row → restore+foreground→worker re-snapshot→title-match→SelectionItemPattern.Select) +
  empty-state "no cards, keep fallback buttons" (option a, no AppBar churn). Fable steered (UIA Select not
  synthetic input; staleness = re-snapshot at click, never wrong tab; hover-bridge union+grace; empty-state
  paint-only). win32-scout resolved R1 (NOACTIVATE click → SetForegroundWindow works, no ASFW; MA_NOACTIVATE
  mandatory) + R2 (SelectionItemPattern supported; gate on IsWindowVisible&&!IsIconic + TabControl retry, no
  fixed sleep, Select S_OK-silent-fails early; fallback SetFocus→LegacyIAccessible, NOT Invoke). Design only,
  no src/ changes. Open UX call: close fan on click vs on confirm. Ready to implement (spikes→5 steps).
- 2026-07-04 — Profiler consumer P.2–P.4 built (separate `shell_profiler` target under `profiler/`,
  own CMakeLists, zero shared shell code — hard rule 8). EtwSession: name-derived provider GUID
  (SHA-1/EventSource algo, runtime — no hardcoded GUID; verified `{C943A625-2D01-532A-B9E9-19613974D9AD}`
  == .NET reference), StartTraceW real-time + stale-session recovery (ERROR_ALREADY_EXISTS→STOP→retry),
  EnableTraceEx2, OpenTraceW/ProcessTrace on consumer thread, TDH decode via TdhFormatProperty; Stop()
  never leaks the session (CloseTrace+ControlTrace STOP, idempotent, dtor-guarded). MetricsView:
  per-event count/rate/p50/p95/max(duration_us) + shell CPU%/WS/handle sampling; `--csv` per-interval.
  main: `--raw|--csv|--image|--provider`, Ctrl+C clean stop. Both targets build green (VS2022 Debug);
  shell target unaffected. Wrote `docs/profiler-project-structure.md` (one repo/two targets rationale).
  P.1 (shell-side Trace.h + call sites) NOT done — stayed out of src/ per task; shell emits no events yet,
  so live capture + §12 row P are pending P.1 + elevated run. profiler/README.md documents elevation.
- 2026-07-04 — 5b debt polish (5b.4). (A) MeasureGap gap-left by geometry (leftKnown + chevron extension)
  → overflow chevron can't be overlapped; Widgets by aid (>=, rect-fail→kInvalid). (C) pill corner radius
  (RoundRect = ellipse diameter). (D) killed startup/reload double-frame: dock defers strip until first
  host verdict (m_gapResolved) + overlay posts on first measure (m_statePosted) + Create one-shot backstop
  timer. (B) tried AppBarRemove-first, REVERTED — ABM_REMOVE is itself a blocking SendMessage so it can't
  outrun a hung explorer (inherent, no app fix). 2 burst rounds (AppBar/threading/visual-geom): r1 MAY
  PROCEED w/ warnings→applied; r2 fixed Widgets-rect-fail overlap + dead push → final MAY PROCEED, no
  residual debt. Simplifier: pending. Build clean. Stage 5 fully done incl. debt.
- 2026-07-04 — Stage 5 ACCEPTED on Win11: all 5 5b visual checks pass (pills in gap; open apps → gap
  shrinks/pills drop; close → grow back; gap fail → dock-strip fallback; empty-gap right-click → taskbar
  menu; config edit → live pill update). All 5 stages now accepted. Next: profiler or deployment workstream.
- 2026-07-04 — 5b.3 done → Phase 5b CODE COMPLETE. Event-driven re-measure + single-host fallback. Dropped
  500ms poll: explorer-PID-scoped EVENT_OBJECT_LOCATIONCHANGE hook (OBJID_WINDOW|CLIENT) → kRemeasureMsg →
  200ms one-shot debounce → RequestMeasure; also WM_POWERBROADCAST resume (500ms delayed). Overlay posts
  kGapStateMsg(1/0) to dock on host flip (ApplyGap); DockButtons()=m_gapActive?none:launcher.Buttons() gates
  dock Paint+ButtonAt → single host (gap active hides dock strip; measure fail → dock fallback). Overlay
  Create+(dockHwnd,stateMsg); config reload→RequestMeasure (re-eval fit); Refresh() removed; WorkerLoop
  split-try posts invalid gap on any failure/throw. Bursts (threading/AppBar/fallback): AppBar clean; r1
  BLOCKED F-02 (reload didn't re-eval fit) → fixed + F-03 (invalid-gap-on-throw) + T-1 (LOCATIONCHANGE obj
  filter); r2 re-burst → MAY PROCEED. Simplifier: named kResumeRemeasureMs. Build clean. Debt: host-handoff
  1-frame both-show at startup/reload (transient). Visual check pending. Next: user check 5b + §12 rows.
- 2026-07-04 — 5b.1 ACCEPTED on Win11 (green outline hugs [Postman,Widgets]); 5b.2 buttons-in-gap code done,
  MAY PROCEED. Overlay now hosts the automation pills: dropped WS_EX_TRANSPARENT, WM_NCHITTEST→HTCLIENT on
  pill / HTTRANSPARENT elsewhere (empty-gap + right-click menu still reach taskbar), WM_LBUTTONUP→Launcher::
  Execute. Renderer: DrawButton promoted public + new GapButtonLayout (left-anchored, drops overflow =
  auto-downsize). ButtonAt hit-tests same layout Paint draws. ApplyGap shows only when ≥1 pill fits. Config
  reload→Refresh. Overlay holds const Launcher* (UI thread; worker never touches it). Burst (threading/DPI/
  visual)→MAY PROCEED; applied T3 (m_taskbarOverlay declared after m_launcher → destructs first) + V2 (hide
  when no pill fits). Simplifier: pending. Build clean. Visual pending. Both dock+gap show buttons till 5b.3.
- 2026-07-04 — Phase 5b started; 5b.1 gap-measurement + debug outline code done, MAY PROCEED. First tried a
  pure-HWND path but USER SCREENSHOT showed the outline far too wide. Live-probed the taskbar: on Win11 the
  MSTaskSwWClass HWND rect is a legacy STUB (45..577) that doesn't match the XAML layout (app icons render
  to ~1418). PIVOTED to UIA. `TaskbarOverlayWindow.{h,cpp}`: MeasureGap = ElementFromHandle(Shell_TrayWnd) →
  find TaskbarFrame → walk children: gap.left = max right over task buttons (Taskbar.TaskListButtonAutomation
  Peer + Start/Search/TaskView), gap.right = WidgetsButton left (when in-gap) else TrayNotifyWnd left. Win10
  fallback = legacy MSTaskListWClass/TrayNotifyWnd HWND path. UIA is blocking → WORKER thread (mirrors
  TabReader): CoInit MTA + CLSID_CUIAutomation, posts heap Gap* → UI thread SetWindowPos's a click-through
  layered outline (WS_EX_TRANSPARENT|LAYERED, LWA_COLORKEY, green frame). FindTaskbar verifies explorer.exe
  owner. Guards→kInvalid: auto-hide, null tray, GetDpiForWindow(tray)==0, no-task-button. DockWindow owns it;
  RequestMeasure (signals worker) on Create + 500ms kOverlayTimer + ABN_POSCHANGED/DISPLAYCHANGE/DPICHANGED;
  WM_DESTROY joins worker before AppBarRemove + WM_ENDSESSION KillTimer. No AppBar → no ABM_REMOVE. Two burst
  rounds (AppBar/threading/DPI/visual): r1 fixed CoUninitialize-before-ComPtr-release (BLOCKING) + null-tray/
  dpi-0/no-button guards + CoInit-HRESULT balance; r2 re-burst (threading+visual) clean, [1418,1948] traced.
  Simplifier: no churn. Live UIA layout saved to research doc. Build clean. Visual check pending on Windows.
  Debt→5b.3: overflow-chevron class (live), worker-join vs hung explorer at shutdown, one-Gap shutdown leak.
- 2026-07-04 — Stage 5a.4 done + Phase 5a complete: config hot-reload. ConfigWatcher.{h,cpp} worker
  (overlapped ReadDirectoryChangesW + stop-event; pending-flag drain teardown so no break path deadlocks
  or leaks). DockWindow: kConfigChangedMsg → 300ms kConfigTimer debounce → Launcher.Load()+repaint;
  Create makes dir + starts watcher; WM_DESTROY joins before AppBarRemove. Launcher split into
  ConfigDir/ConfigFileName/ConfigPath. Burst (threading+AppBar+resource): fixed BLOCKING undrained
  overlapped-IO teardown (CancelIo→GetOverlappedResult(TRUE), guarded by pending) → re-burst →
  MAY PROCEED. Also this session: button pills halved (user), Search→Gmail config. Next: Phase 5b overlay.
- 2026-07-04 — Stage 5a.3 done (awaiting user visual OK): button strip. User chose right-column overlay
  (pills top-right over cards). Renderer::ButtonLayout (single paint+hit-test source) + DrawButton
  (RoundRect light kButtonBg pill, radius clamp, ellipsized label). Paint draws buttons last. DockWindow
  ButtonAt + WM_LBUTTONUP button-first→card routing + WM_MOUSEMOVE fan-suppress; dropped 5a.2 debug
  middle-click. Burst (DPI/visual/GDI+hit-test): fixed BLOCKING pill invisibility over dark cards
  (→light kButtonBg+dark text), unscaled pen, unclamped radius → re-burst → MAY PROCEED. Icon-image
  render deferred (icon style → labeled pill). Simplifier: no churn. Build clean. Next: 5a.4 hot-reload.
- 2026-07-04 — Stage 5a.2 done: Launcher::Execute — detached MTA worker (pump-less fire-and-forget;
  STA would need a pump) → ShellExecuteW (url/shortcut) / CreateProcessW (command, handles closed);
  CoUninitialize only on SUCCEEDED(CoInitializeEx). #ifdef _DEBUG WM_MBUTTONUP cycles buttons as the
  trigger. Two bursts (threading + resource): fixed unconditional-CoUninitialize (BLOCKING), STA→MTA,
  ConfigPath free-of-garbage guard, MBTWC guard → re-burst clean → MAY PROCEED. Simplifier: no churn.
  Build clean (had to nuke a VS2019-reverted CMake cache + regen VS2022 mid-step). Next: 5a.3 (visual).
- 2026-07-04 — Stage 5a.1 done: Launcher.{h,cpp} line-based config load (config.txt,
  style|label|action|target|iconPath, BOM/UTF-8 decode, malformed→skip+debug, missing→zero). Chose
  line-based over JSON per hard rule 2. Loaded in DockWindow::Create. Two bursts (parsing + resource
  hygiene): fixed swprintf_s process-kill (→_snwprintf_s/_TRUNCATE), misaligned-wchar_t UB (→memcpy),
  unchecked MBTWC guard, dropped redundant link pragma → re-burst clean → MAY PROCEED. Simplifier
  folded a DebugPrintf helper. Build clean. Next: 5a.2 actions.
- 2026-07-04 — Stage 4 refinement (4.5b): real vertical window stacking. CardLayout stacks full-width
  bands top→bottom (was side-by-side); dock height now DYNAMIC (DockHeightPx: one kBandHeightDip=34
  band per minimized window + pad, clamped 1..kMaxBands=4), AppBarSetPos re-negotiated on minimize
  events + validation timer. Colors: kBgColor #00A2ED, kChipActiveBg #f87e73, kTextActive dark,
  new kTextOnBg for empty-state. Two inspector bursts: first flagged 4 blockers (band inversion N≥4,
  empty-state text invisible on blue, WM_DPICHANGED no-invalidate stale paint, AppBarSetPos-after-
  ABM_REMOVE on ENDSESSION drain) + 2 suspicious (chip-text inversion, WM_DISPLAYCHANGE invalidate).
  All fixed (dynamic height, kTextOnBg, InvalidateRect ×2, m_appBarRegistered guard, txt-rect guard);
  re-burst (AppBar/DPI/visual) clean → adjudicator MAY PROCEED. Simplifier: comment fixes only. Build
  clean. Runtime/visual check pending on Windows. Next: Stage 5a.1.
- 2026-07-04 — Step 4.5 (snapshot debounce) + 4.5a (drop card header) done in one run. Debounce:
  RequestSnapshotDebounced coalesces MINIMIZESTART/NAMECHANGE bursts into one 150ms-quiet flush to
  TabReader; FOREGROUND pre-warm stays immediate (must beat UIA strip); store/paint stay immediate;
  KillTimer(kSnapshotTimer) in WM_DESTROY. 4.5a: removed per-card title + "N tabs" header (echoed
  active tab → no info); chips now fill full card height. Inspector burst (threading/AppBar/DPI clean;
  visual F1 tall-slab font = user taste call) → adjudicator → MAY PROCEED. Simplifier: no churn. Build
  clean. Runtime/visual check pending on Windows. Stage 4 code complete; §12 row 4 acceptance next.
- 2026-07-03 — Step 4.4 done: click-to-restore. WM_LBUTTONUP → CardAt hit-test (shared Renderer::
  CardLayout, client coords) → RestoreWindow. Card removal driven solely by EVENT_SYSTEM_MINIMIZEEND
  (Renderer filters minimized-only), no optimistic store write. Inspector burst (threading, AppBar,
  click-restore correctness) → adjudicator → BLOCKED (rule-5 pump-block on hung target). Fixed:
  ShowWindow→ShowWindowAsync (M1), dropped undocumented SwitchToThisWindow for FlashWindowEx fallback
  (M2, also clears rule-6), event-driven removal (M3, no orphaned card). Re-burst (threading+AppBar) →
  re-adjudicate → MAY PROCEED (SetForegroundWindow ruled non-blocking). Simplifier extracted ClearHover
  helper. Build clean. Runtime/visual check pending with user. Next: 4.5 snapshot debounce.
- 2026-07-03 — fanpopup: dropped window-title header (duplicated active-tab row); fan opens straight
  into the tab list. User-requested tweak. Build clean.
- 2026-07-03 — Step 4.3 done: per-window hover-fan. FanPopup.{h,cpp} (transient WS_POPUP, NOACTIVATE,
  grows upward from strip top, monitor-clamped, DPI-scaled rows + "+N more" overflow). PaintUtil.h
  extracts shared palette/ScalePx/MakeFont. Renderer exposes CardLayout (single source of card rects,
  shared with hit-test). DockWindow owns FanPopup: TrackMouseEvent(TME_LEAVE) + 250ms kHoverTimer
  hover-intent, ShowFanFor maps card→screen anchor, WM_MOUSELEAVE/WM_DESTROY tear down. Inspector burst
  (AppBar clean, threading clean, DPI 2 deferred, visual/layout V1/V2/V3) → adjudicator → MAY PROCEED.
  Applied F-01 top-clamp (height≤avail), F-02 empty-tabs guard (Show→Hide), F-03 +N padding align.
  Simplifier: no churn. Build clean. Runtime/visual check pending with user. Next: 4.4 click-restore.
- 2026-07-03 — Step 4.2a done: TabReader caches UIA_SelectionItemIsSelectedPropertyId → Tab.active
  (clamped to one active tab max at source); Renderer promotes active chip to front + highlights it
  (kChipActiveBg/kTextActive). Inspector burst (visual/layout, DPI, threading) → adjudicator →
  MAY PROCEED (F-01 split-highlight fixed at source in TabReader; F-02 unconditional VariantClear
  applied; F-03 weak active-chip contrast = user visual call). Build clean. Runtime/visual check on
  active-tab highlight pending with user. Next: 4.3 hover-fan.
- 2026-07-02 — Docs bootstrapped: architecture (stages 1–5 + observability),
  per-stage plans, CLAUDE.md rules, this handoff. No code yet. Next: 1.1.
- 2026-07-02 — Step 1.1 done: CMakeLists.txt, stub `wWinMain`, .gitignore.
  Built + ran on Win11 (exit 0) via VS2019 BuildTools + NMake (VS2022 C++
  install broken on this machine — see build note above). 4-reviewer
  adversarial burst + verifier: PASS, no code findings; DPI items deferred
  to 1.2. Next: 1.2.
- 2026-07-03 — Step 1.2 done: PMv2 DPI call (abort on FALSE), SDK-version
  pins in CMake, DockWindow (WS_POPUP, TOOLWINDOW|TOPMOST|NOACTIVATE),
  message loop, right-click quit. Built + checkpoint-verified on Win11
  (window visible, PMv2 confirmed, not in taskbar/Alt-Tab by style, exit 0)
  via VS2022 (install fixed itself — build note updated). 4-reviewer burst +
  Opus verifier: 2 fixes applied (WS_EX_NOACTIVATE, rationale comments),
  5 deferred as carry-overs above, 5 rejected. 3 research agents' Win32
  edge-case findings saved to docs/research/win32-edge-cases.md. Next: 1.3.
- 2026-07-03 — Step 1.5 done: AppBarSetPos() — MonitorFromWindow+GetMonitorInfoW
  (rcMonitor), MulDiv(64,dpi,96) dock height, ABM_QUERYPOS→re-anchor→ABM_SETPOS→
  SetWindowPos. kCallbackMsg/WM_DISPLAYCHANGE/WM_DPICHANGED all call AppBarSetPos.
  ABM_NEW moved before ShowWindow so position negotiated before first paint.
  Inspector burst (AppBar-hygiene clean, DPI 2 suspicious dismissed, threading
  2 suspicious dismissed) → adjudicator → MAY PROCEED. Checkpoint on Windows:
  dock flush above taskbar full-width ✅, Notepad maximize stops at dock edge ✅,
  exit→maximize→no gap ✅. Next: 1.6.
- 2026-07-03 — Stage 1 ACCEPTED on Win11: dock visible ✅, Notepad stops at dock edge ✅,
  exit releases strip ✅, ABN_POSCHANGED (auto-hide toggle) renegotiates ✅ (taskbar-to-left
  not testable on Win11 — taskbar locked to bottom), second instance exits immediately ✅,
  fullscreen video dock steps aside ✅. Stage 2 + profiler workstream unlocked.
- 2026-07-03 — Step 1.7 done: single-instance named mutex (CreateMutexW/ERROR_ALREADY_EXISTS),
  WM_QUERYENDSESSION→TRUE + WM_ENDSESSION(wParam!=0)→AppBarRemove+PostQuitMessage,
  SetUnhandledExceptionFilter crash filter (best-effort ABM_REMOVE, EXCEPTION_CONTINUE_SEARCH).
  Inspector burst (AppBar-hygiene: narrow-gap A1 dismissed as best-effort design contract;
  threading: F-01 crash-filter-off-UI-thread deferred to Stage 2 first worker step) →
  adjudicator → MAY PROCEED. Build clean; Stage 1 acceptance row (§12) pending on Windows.
- 2026-07-03 — Step 1.6 done: ABN_FULLSCREENAPP handler (SetWindowPos HWND_BOTTOM/HWND_TOPMOST on
  lparam toggle) + GetDpiForWindow==0 guards in AppBarSetPos and WM_PAINT. Inspector burst
  (AppBar-hygiene clean, threading clean, DPI: F-01 pre-existing deferred, F-02/F-03 nits) →
  adjudicator → MAY PROCEED. Build verified; runtime checkpoint (taskbar-move, DPI-change,
  fullscreen-video) pending on Windows. Next: 1.7.
- 2026-07-03 — Step 1.4 done: ABM_NEW after ShowWindow (kCallbackMsg=WM_APP+1),
  AppBarRemove() helper with m_appBarRegistered guard, ABM_REMOVE in WM_DESTROY
  before PostQuitMessage, ~DockWindow covers GetMessage==-1 path, WS_EX_TOPMOST
  kept. Inspector burst (AppBar-hygiene, threading) → adjudicator → MAY PROCEED
  (all findings informational). ABM_NEW confirmed working (window visible); gap/
  kill checkpoint tests not formally observed (user moved on; Task Manager kill
  behavior deferred to 1.7 doc). Debug rect adjusted to (160,1645) for testing.
  Next: 1.5.
- 2026-07-03 — Stage 2 ACCEPTED on Win11: indicator shows Edge title + "(+1)" for Chrome,
  "browser: none" when closed, live tracking works within ~1s, CPU ~0% idle.
- 2026-07-03 — Step 2.4 done: debounce via m_pendingValidation + 200ms SetTimer. kWindowEventMsg
  deduplicates HWND into pending list + restarts timer. WM_TIMER drains list, validates each
  HWND with IsBrowserFrame, updates m_browsers, repaints. KillTimer in WM_DESTROY. --scan
  already #ifdef _DEBUG. Inspector burst (AppBar: clean; threading: F-T1 unbounded-pending
  informational/no-fix; F-T2 dismissed) → adjudicator → MAY PROCEED. Stage 2 code complete;
  §12 row 2 runtime acceptance pending on Windows. Next: Stage 3.
- 2026-07-03 — Step 2.3 done: WinEventHook (EVENT_OBJECT_CREATE..HIDE, OUTOFCONTEXT,
  system-wide). WinEventProc pre-filters idObject==OBJID_WINDOW then PostMessage.
  kWindowEventMsg handler: IsBrowserFrame re-validate, add/remove m_browsers,
  InvalidateRect. Unhook in WM_DESTROY before AppBarRemove. F-01 fix: g_dockHwnd
  std::atomic<HWND> + exchange(nullptr) in CrashFilter + clear after message loop.
  Inspector burst → adjudicator → MAY PROCEED (F-T1 class-gated pump call accepted;
  F-A1/A2 infos applied). Build/runtime pending. Next: 2.4.
- 2026-07-03 — Step 2.2 done: Renderer.{h,cpp} with Renderer::Paint (dark bg + DPI-scaled
  indicator: "browser: none" vs "browser: <title> (+N)"). ScanBrowserFrames() called in
  Create() before ShowWindow to populate m_browsers. EnumProc made noexcept+try/catch
  (F-A1 warning fix). Inspector burst (AppBar: F-A1 fixed; threading: F-T1/F-T2 dismissed) →
  adjudicator → MAY PROCEED. Build/runtime pending on Windows. Next: 2.3.
- 2026-07-03 — Step 2.1 done: WindowMonitor.{h,cpp} with IsBrowserFrame (Chrome_WidgetWin_1,
  visible, unowned, non-empty title, chrome.exe/msedge.exe via QueryFullProcessImageNameW) +
  ScanBrowserFrames. --scan debug flag in #ifdef _DEBUG. Inspector burst (AppBar: clean;
  threading: F-01 pre-existing deferred to 2.3, comment tightened) → adjudicator → MAY PROCEED.
  Build pending on Windows. Next: 2.2.
- 2026-07-03 — Fix: pre-warm UIA on EVENT_SYSTEM_FOREGROUND (fourth hook). Tab tree stripped on minimized window → snapshot while still visible. Inspector burst (AppBar: F-A1/A2 dismissed nits; threading: F-T1 informational narrow-lock watch item) → adjudicator → MAY PROCEED. Build clean; runtime re-test needed.
- 2026-07-03 — Stage 4 direction set to variant D (side-by-side minimized-only cards + per-window
  hover-fan; drop many-window overflow — user has 2-3 windows × 10+ tabs). Plan rewritten in
  stage-4.md. Step 4.1 done (Paint shows only minimized windows; open windows hidden). Step 4.2
  done (N tabs badge + legible ~110px chips stretched to fill + guards for narrow cards/badge-fit).
  Inspector bursts (DPI + visual/layout) + adjudicate on each. Loop disabled per user; run
  autonomously (see memory). Next: 4.2a selected-tab state, then 4.3 hover-fan.
- 2026-07-03 — Stage 3 ACCEPTED on Win11: minimize browser → per-window card shows real tab
  titles. Root-caused the empty-tabs bug via a temp UIA tree dump (browser TabItems nest two
  panes deep under the "Tab bar" control → TreeScope_Children found nothing; fixed to
  TreeScope_Descendants). Also: /utf-8 flag fixed middle-dot corruption; EVENT_SYSTEM_FOREGROUND
  pre-warm snapshot; IsInsideDocument skips web-content tablists; CleanTabTitle strips status
  suffixes; per-tab chips fill full card width with +N overflow; stale marker dropped. Inspector
  bursts (DPI + visual/layout added when Renderer changes) + adjudicator on each. Stage 4 unlocked.
- 2026-07-03 — Step 3.5 done: staleness handling — TabSnapshot::failed flag (set when tabs.empty()); kTabSnapshotMsg branches: failure→MarkTabsStale (keeps prior tabs), success→SetTabs (clears stale); DrawCard appends "(stale)" to tab line. Inspector burst (AppBar: clean; threading: clean) → adjudicator → MAY PROCEED. Build clean; §12 row 3 runtime acceptance pending on Windows. Stage 3 code complete.
- 2026-07-03 — Step 3.4 done: Renderer draws tab card for minimized windows — title header (FW_SEMIBOLD 10pt) + tab line (FW_NORMAL 9pt, " · " separator, DT_END_ELLIPSIS) from Store::tabs; DrawCard helper; active-window path unchanged. Pre-existing TabReader ComPtr/.Get() compile bug fixed. Inspector burst (AppBar: clean; threading: clean; DPI: F-D1 nit fixed inline, F-D2 informational pre-existing) → adjudicator → MAY PROCEED. Build clean; §12 row 3 runtime acceptance pending on Windows. Next: 3.5.
- 2026-07-03 — Step 3.3 done: TabReader.{h,cpp} — worker thread (COINIT_MULTITHREADED), CoCreateInstance IUIAutomation, SnapshotTabs (ElementFromHandle→TabControl→TabItems→CachedName), PostMessage kTabSnapshotMsg heap payload. Third WinEvent hook (NAMECHANGE). RequestSnapshot on MINIMIZESTART + NAMECHANGE. kTabSnapshotMsg handler: SetTabs+delete+invalidate. Debug OutputDebugString dump. F-T1 fix: worker exits immediately on m_stop (no drain-then-stop). Inspector burst (AppBar: clean; threading: F-T2 MTA/UIA dismissed — MTA is correct; F-T1 fixed inline) → adjudicator → MAY PROCEED. Build clean; runtime checkpoint pending on Windows. Next: 3.4.
- 2026-07-03 — Step 3.2 done: second WinEvent hook (MINIMIZESTART..END); Store::SetMinimized; kWindowEventMsg fast-paths minimize events (bypass debounce); IsIconic check on initial scan; Renderer shows "<title> — minimized". Inspector burst (AppBar: F-A2 WM_ENDSESSION no-unhook — informational, ABM_REMOVE confirmed; threading: clean) → adjudicator → MAY PROCEED. Build clean; runtime checkpoint pending on Windows. Next: 3.3.
- 2026-07-03 — Step 3.1 done: Store.{h,cpp} (Tab/TrackedWindow structs, HWND-keyed map, UI-thread-only). Migrated DockWindow::m_browsers→m_store:Store; Renderer now takes const Store&. Simplifier extracted StoreWindow helper. Inspector burst (AppBar: F-A1 pre-existing double-PostQuitMessage — safe, no double ABM_REMOVE; threading: F-T1 comment nit, F-T2 ordering note — both pre-existing) → adjudicator → MAY PROCEED. Build clean; runtime checkpoint pending on Windows. Next: 3.2.
- 2026-07-03 — Step 1.3 done: WM_PAINT (dark fill + DPI-scaled Segoe UI 12pt
  via GetDpiForWindow+MulDiv), WM_ERASEBKGND→1, hbrBackground=nullptr,
  UpdateWindow after ShowWindow. Inspector burst (AppBar-hygiene, threading,
  DPI) → adjudicator → BLOCKED on default-font not DPI-scaled → fixed →
  re-burst → MAY PROCEED. Checkpoint: text crisp at 100%/150% — visual
  verification pending on Windows (hard rule 3). Checkpoint protocol added to
  CLAUDE.md. Next: 1.4.

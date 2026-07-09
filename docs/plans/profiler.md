# Profiler workstream plan — decoupled observability

Spec: `docs/ARCHITECTURE.md` §10. Acceptance: §12 row P.
**Unlocks after Stage 1 is accepted.** Runs in parallel with stages 2–5;
each stage adds its events per the §10 event table.

Non-negotiable (hard rule 8): the shell gets ONLY the TraceLogging wrapper
and call sites — no files, no threads, no frameworks. `shell_profiler` is a
separate executable/target; no shared code with the shell. The contract is
the provider name `BrowserShellOs.Perf` and the event/field names in §10.

## Status

- P.1 — ✅ code complete, builds green. `src/Trace.h`/`Trace.cpp` land the
  provider (name-derived GUID pinned to match `ProviderGuidFromName`),
  `TRACE_EVENT`/`TRACE_SCOPE` macros, register/unregister bracketed in
  `wWinMain` via `TraceGuard`. All six live call sites wired: `Paint`
  (`TaskbarOverlayWindow::Paint`), `FanActivateLatency` (fan click →
  tab-visible latency chain, `TabReader.cpp::ActivateTab`),
  `WinEventCallback` (`HostWindow::WinEventProc`, browser-window events only —
  the fg/taskbar hooks are unrelated internal plumbing, not this event),
  `UiaSnapshot` (worker-loop call site around `SnapshotTabs`, `hr` is a
  coarse S_OK/E_FAIL/E_HANDLE proxy — `SnapshotTabs`'s many internal
  early-returns were NOT restructured to thread a real HRESULT out, same
  complexity trade-off as the existing `activatetab-complexity` debt),
  `StoreUpdate` (`Store::Set/SetTabs/Remove` — the three mutators that
  actually change tracked_windows/total_tabs; `SetMinimized`/`MarkTabsStale`
  don't, so left untraced), `LauncherAction` (`Launcher::Execute`'s detached
  worker thread — thread-safe by the same precedent as `FanActivateLatency`
  already emitting from a worker thread). `AppBarNegotiate` DROPPED from the
  contract — chip-rework Stage 3 killed the AppBar dock entirely (rule 4 now
  vacuous), so there was nothing left to negotiate; ARCHITECTURE §10 and
  `profiler/Contract.h` updated to match. Runtime verification (wpr/traceview
  showing all six events fire) pending on Windows.
- P.2 — ✅ code complete, builds green. Real-time session + name-derived GUID +
  TDH decode. GUID `{C943A625-2D01-532A-B9E9-19613974D9AD}` verified against the
  .NET EventSource reference algorithm. Live decode from the shell pending a
  runtime session on Windows (elevated).
- P.3 — ✅ code complete, builds green. Per-event count/rate/p50/p95/max +
  shell process CPU%/working-set/handle sampling; console table each interval.
- P.4 — ✅ `--csv <path>` code complete, builds green. §12 row P end-to-end
  acceptance pending a running shell + `shell_profiler` session on Windows
  (elevated).

## Step P.1 — Shell-side instrumentation (touches shell, minimally)

**Build:** `src/Trace.h`: `TRACELOGGING_DEFINE_PROVIDER` for
`BrowserShellOs.Perf`, register/unregister in `wWinMain`, and two macros —
`TRACE_EVENT(name, fields...)` and `TRACE_SCOPE(name)` (RAII QPC timer that
emits duration_us on scope exit). Instrumented: `TaskbarOverlayWindow::Paint`
(`Paint`, duration_us + dirty_w/dirty_h) and the fan-activate chain
(`FanActivateLatency`, see ARCHITECTURE §10). No AppBar site — chip-rework
Stage 3 removed the AppBar dock, so `AppBarNegotiate` was dropped from the
contract rather than wired to nothing.

**Checkpoint:** shell builds and behaves identically; capture with
`wpr -start GeneralProfile` or `traceview`/`tracelog` shows both events
firing. Binary size delta and idle CPU delta ≈ 0.

## Step P.2 — Profiler skeleton: real-time ETW session

**Build:** `profiler/` with its own CMake target `shell_profiler` (console,
links `advapi32 tdh`). `EtwSession.{h,cpp}`: `StartTraceW` (real-time mode) +
`EnableTraceEx2` on the provider by name-derived GUID, `OpenTraceW` +
`ProcessTrace` on a consumer thread, TDH decode (`TdhGetEventInformation`)
of the self-describing events. Print raw decoded events to stdout. Handle
the already-running-session case (`ERROR_ALREADY_EXISTS` → stop stale
session and retry). Document the elevation / Performance Log Users
requirement in `profiler/README.md`.

**Checkpoint:** run shell + `shell_profiler` → decoded `AppBarNegotiate` /
`Paint` events stream live. Ctrl+C stops the session cleanly
(`ControlTraceW(EVENT_TRACE_CONTROL_STOP)` — never leak the session).

## Step P.3 — Metrics view

**Build:** `MetricsView.{h,cpp}`: aggregate per event name — count, rate/s,
p50/p95/max of duration_us; redraw a console table every second. Add process
sampling of the shell (find PID by image name): CPU %, working set, handle
count via `GetProcessTimes`/`GetProcessMemoryInfo`, shown alongside.

**Checkpoint:** drag the taskbar / force repaints → `Paint` p95 visibly
moves; numbers match Task Manager's for the process row.

## Step P.4 — CSV export + acceptance

**Build:** `--csv <path>` flag: append one row per aggregation interval.
Run §12 row P end-to-end.

**Checkpoint:** §12 row P — including: delete `shell_profiler.exe` → shell
runs identically (the decoupling proof).

## Ongoing per-stage duty

When implementing stages 2–5, add that stage's events from the §10 table as
part of the stage's final step (one-line call sites only). The profiler needs
no changes — TraceLogging events are self-describing.

## Definition of done

- [ ] §12 row P passes.
- [ ] Shell contains no observability code beyond `Trace.h` + call sites.
- [ ] Profiler builds/runs with the shell target disabled, and vice versa.

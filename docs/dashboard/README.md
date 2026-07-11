# Fan-activate latency dashboard

Power BI dashboard over the `shell_profiler` `FanActivateLatency` captures. Visualizes where the
tab-restore latency goes and how it dropped across five capture runs (~602 ms → 271 ms via guided descent).

## Open it
Open `Logging_shell_real.pbip` in Power BI Desktop (needs the **"Store reports using enhanced metadata
format (PBIR)"** preview feature enabled to re-save). On first open, click **Home → Refresh** to populate
the calculated tables, then **Ctrl+S**.

## Layout
- **Format:** PBIP — text-only. Model is TMDL (`*.SemanticModel/definition/`), report is PBIR
  (`*.Report/definition/`). No `.pbix` binary blob, so it diffs and merges in git.
- **Data:** self-contained. Both fact tables are inline `DATATABLE` calculated tables (no external
  source, no refresh dependency). Figures are the aggregates from `docs/HANDOFF.md`'s capture log; the
  raw per-click dumps were not retained.
- **Pages:** Overview · Stage Bottlenecks · Run Trends · Capture Metadata.
- **Tables:** `Captures` (per-run), `SubFields` (per-stage), `CaptureMeta` (per-run instrumentation notes).
- **Theme:** `Logging_shell_real.Report/StaticResources/RegisteredResources/LatencyDark.json` (dark).

## Notes
- Not committed: `.pbi/localSettings.json`, `cache.abf` (see `.gitignore`).
- Known data caveat: Capture 2's `us_gate2_wait` and `us_first_walk` are the same physical time measured
  two ways — currently listed as two stages, so per-stage totals for that run double-count 283 ms.

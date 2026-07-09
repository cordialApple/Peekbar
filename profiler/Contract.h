#pragma once

// ETW contract with the shell — the ONLY coupling between the two programs
// (hard rule 8). Duplicated here independently; no shell header is included.
// Source of truth: docs/ARCHITECTURE.md §10.
//
// Provider name: BrowserShellOs.Perf
// The provider GUID is DERIVED from this name at runtime (TraceLogging /
// EventSource convention) — see ProviderGuidFromName in EtwSession.cpp — so a
// hardcoded GUID is never checked in and cannot drift from the name.
//
// Events (self-describing TraceLogging; decoded via TDH, no manifest):
//   AppBarNegotiate   duration_us, edge, resulting rect
//   Paint             duration_us, dirty w x h
//   WinEventCallback  event id, hwnd
//   UiaSnapshot       duration_us, hwnd, tab_count, hr
//   StoreUpdate       tracked_windows, total_tabs
//   LauncherAction    action type, duration_us, hr
//   FanActivateLatency  outcome, us_click_to_restore, us_restore_to_tabfound,
//                       us_tabfound_to_select, us_select_to_confirm, duration_us
//                       (fan click -> tab-visible latency chain; duration_us
//                       spans click to activation-confirmed, a proxy for first
//                       visible frame, not a true paint signal)

namespace contract {

inline constexpr wchar_t kProviderName[] = L"BrowserShellOs.Perf";

// Field name the profiler treats as a latency measurement to aggregate.
inline constexpr wchar_t kDurationField[] = L"duration_us";

}  // namespace contract

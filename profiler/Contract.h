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
//   Paint             duration_us, dirty_w, dirty_h (TaskbarOverlayWindow::Paint;
//                     AppBarNegotiate dropped — chip-rework killed the AppBar dock)
//   WinEventCallback  event id, hwnd
//   UiaSnapshot       duration_us, hwnd, tab_count, hr
//   StoreUpdate       tracked_windows, total_tabs
//   LauncherAction    action type, duration_us, hr
//   FanActivateLatency  outcome, us_click_to_restore, us_restore_to_tabfound,
//                       us_tabfound_to_select, us_select_to_confirm, duration_us,
//                       us_gate1_wait, gate1_attempts, us_gate2_wait, gate2_attempts,
//                       us_first_walk, us_last_walk, us_element_from_handle,
//                       us_findall_tabctrls, us_is_inside_document, us_findall_tabitems,
//                       tabctrl_candidates, guided_descent_used
//                       (fan click -> tab-visible latency chain; duration_us
//                       spans click to activation-confirmed, a proxy for first
//                       visible frame, not a true paint signal. The gate1/gate2/
//                       walk fields split us_restore_to_tabfound diagnostically —
//                       gate1 = window-visible wait, gate2 = UIA tab-tree-walkable
//                       wait, first/last_walk = duration of the first vs. most
//                       recent FindLiveTabItems call within gate 2. The next five
//                       fields split that winning FindLiveTabItems call itself:
//                       element_from_handle (ElementFromHandle), findall_tabctrls
//                       (TabControl search — guided TreeScope_Children descent that
//                       prunes Document/web-content subtrees, or the original
//                       TreeScope_Descendants blanket FindAll as a same-call fallback
//                       if guided descent finds nothing), is_inside_document
//                       (per-candidate parent-walk excluding web-content documents,
//                       accrued across all candidates tried — a redundant but cheap
//                       safety re-check even for guided candidates, which cannot be
//                       inside a Document by construction), findall_tabitems
//                       (FindAllBuildCache for TabItems, accrued only when it
//                       actually runs), tabctrl_candidates (how many TabControl
//                       candidates were tried). guided_descent_used is 1 if guided
//                       descent supplied the candidates, 0 if it found none and the
//                       call fell back to the blanket search — lets a live capture
//                       tell the two paths apart. Fields are appended after the
//                       pre-existing ones — TDH decodes this event positionally, so
//                       field ORDER must stay append-only; never reorder or insert
//                       ahead of existing fields.)

namespace contract {

inline constexpr wchar_t kProviderName[] = L"BrowserShellOs.Perf";

// Field name the profiler treats as a latency measurement to aggregate.
inline constexpr wchar_t kDurationField[] = L"duration_us";

}  // namespace contract

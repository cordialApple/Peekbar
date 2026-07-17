#pragma once

// ETW contract with the shell — the ONLY coupling between the two programs (hard rule 8).
// Duplicated here independently; no shell header included. Source of truth: docs/ARCHITECTURE.md §10.
//
// Provider name: Peekbar.Perf
// GUID is DERIVED from this name at runtime (TraceLogging/EventSource convention, see
// ProviderGuidFromName in EtwSession.cpp) so a hardcoded GUID never drifts from the name.
//
// Events (self-describing TraceLogging; decoded via TDH, no manifest):
//   Paint             duration_us, dirty_w, dirty_h (TaskbarOverlayWindow::Paint;
//                     AppBarNegotiate dropped — chip-rework killed the AppBar dock)
//   WinEventCallback  event id, hwnd
//   UiaSnapshot       duration_us, hwnd, tab_count, hr
//   StoreUpdate       tracked_windows, total_tabs
//   LauncherAction    action type, duration_us, hr
//   KeystrokeHopLatency  outcome, active_index, target_index, tab_count, hop_count, used_jump,
//                       us_click_to_restore, us_restore_to_ready, us_ready_to_done, duration_us
//                       (tab activation: pure cache->keystroke via the OPTIMAL ring-hop plan
//                       (PlanTabHops in src/TabHop.h). One batched SendInput of the minimal key
//                       sequence: min over {walk from active, Ctrl+digit anchor + walk} on the
//                       wrapping tab ring. hop_count = keystrokes in the plan (emitted on Selected;
//                       on Failed the plan was formed but not sent); used_jump=1 if the plan opened
//                       with a Ctrl+digit/Ctrl+9 anchor. duration_us ends at KEYS SENT.
//                       FIELD ORDER IS APPEND-ONLY — TDH decodes positionally; never reorder or
//                       insert ahead of existing fields.
//                       Retired: the pre-ring-hop UIA-walk FanActivateLatency event was removed with
//                       the UIA ActivateTab fallback; older captures/*.txt still contain its rows.)

namespace contract {

inline constexpr wchar_t kProviderName[] = L"Peekbar.Perf";

// Field name the profiler treats as a latency measurement to aggregate.
inline constexpr wchar_t kDurationField[] = L"duration_us";

}  // namespace contract

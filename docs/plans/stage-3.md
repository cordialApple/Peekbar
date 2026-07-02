# Stage 3 plan — track one browser's tabs; persist on minimize

Spec: `docs/ARCHITECTURE.md` §5. Acceptance: §12 row 3.
Draft — refine against the actual Stage-2 code before starting.

## Step 3.1 — Store + data model

**Build:** `src/Store.{h,cpp}` with `Tab` / `TrackedWindow` structs from
§2 of the architecture doc, keyed storage (single window is enough for this
stage but key by HWND from the start), written only from the dock thread.
Migrate Stage 2's tracked-window vector into it.

**Checkpoint:** Stage 2 acceptance still passes, now reading from `Store`.

## Step 3.2 — Minimize/restore detection

**Build:** extend the WinEvent hook set with `EVENT_SYSTEM_MINIMIZESTART` /
`EVENT_SYSTEM_MINIMIZEEND`; post to the dock thread; update
`TrackedWindow::minimized` in `Store`; repaint. Render a placeholder card
("<title> — minimized") when minimized.

**Checkpoint:** minimize browser → placeholder card appears; restore → card
clears. Works via title-bar button, Win+Down, and taskbar-icon click.

## Step 3.3 — TabReader: UIA snapshot on a worker thread

**Build:** `src/TabReader.{h,cpp}`. Worker thread with
`CoInitializeEx(COINIT_MULTITHREADED)`, `CoCreateInstance(CLSID_CUIAutomation)`.
`SnapshotTabs(HWND)`: `ElementFromHandle` → find descendant of control type
`UIA_TabControlTypeId` → children `UIA_TabItemControlTypeId` → cached `Name`
properties via `IUIAutomationCacheRequest` (one cross-process round trip).
Result posted to the dock thread as a heap payload; dock thread writes it
into `Store`. Trigger: on `MINIMIZESTART` (tree still live) and on
foreground-title changes of tracked windows (`EVENT_OBJECT_NAMECHANGE`,
debounced) so the cache is warm.

All UIA tree-shape assumptions stay inside `TabReader` (hard rule 6).

**Checkpoint:** debug-dump the snapshot for a browser with 5 known tabs → the
5 titles, in order, within ~200 ms on the warm path. No UI-thread stalls
(dock stays responsive while snapshotting).

## Step 3.4 — Render the tab card

**Build:** `Renderer` draws a card for the minimized window: window title
header + tab titles (truncated with ellipsis, `DT_END_ELLIPSIS`), from
`Store`'s last-known snapshot — never from a live UIA query at paint time.

**Checkpoint:** §12 row 3 acceptance: minimize with 5 tabs → card lists the
5 titles; card persists while minimized; restore → clears.

## Step 3.5 — Staleness + failure handling

**Build:** if a snapshot fails (UIA `hr` failure, zero tabs found), keep the
previous snapshot and mark the card "(stale)". Re-attempt once on the next
minimize. Never crash or block on UIA errors.

**Checkpoint:** kill the browser process while minimized → card is removed
(window destroy event), no dangling HWND use. Minimize immediately after
opening a fresh window → card renders (possibly stale-marked), no hang.

## Definition of done

- [ ] All checkpoints pass on Windows 10/11 against current Chrome AND Edge.
- [ ] Zero UIA calls on the dock thread; paint reads only from `Store`.
- [ ] Stage 1–2 acceptance rows still pass.
- [ ] Known limitation documented in code: tab titles only, no URLs (§9
      upgrade path).

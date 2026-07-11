#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "Store.h"
#include "TabReader.h"
#include "FanPopup.h"
#include "Launcher.h"
#include "ConfigWatcher.h"
#include "TaskbarOverlayWindow.h"

// Hidden coordinator window. Owns the message loop, WinEvent hooks, Store (sole
// writer), TabReader, ConfigWatcher, Launcher, FanPopup, and the taskbar overlay.
// Never shown — chips live in the overlay; there is no dock strip and no AppBar.
// UI thread only owns this (CLAUDE.md rule 5).
class HostWindow
{
public:
    HostWindow() = default;
    ~HostWindow();
    HostWindow(const HostWindow&) = delete;
    HostWindow& operator=(const HostWindow&) = delete;

    bool Create(HINSTANCE instance);

    HWND Hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                      LONG idObject, LONG, DWORD, DWORD) noexcept;

    void ShowFanForChip(HWND chip);
    void ShowFanForButton(int buttonIndex);
    // Spawn one detached worker thread per Launcher::PendingFolderScans() root (rule 5 —
    // the scan itself is blocking filesystem enumeration, never run on the UI thread);
    // each posts its result back via kFolderScanResultMsg. Call after every Launcher::Load().
    void RequestFolderScans();
    // Spawn one detached worker thread scanning `root`, posting the result via
    // kFolderScanResultMsg. Shared by RequestFolderScans, the debounced change-watcher
    // rescan, and the sleep/wake resume backstop.
    void ScanRootAsync(const std::wstring& root);
    // (Re)build one ConfigWatcher (any-change mode) per Launcher::FolderFanRoots() root,
    // replacing any prior set (each old watcher's dtor stops+joins its thread cleanly
    // first). Call after every Launcher::Load(), same as RequestFolderScans.
    void RebuildFolderFanWatchers();
    // Queue `root` for a debounced rescan (deduped against pending) and arm the one-shot
    // kFolderFanScanTimer, coalescing a change-burst into one rescan per root.
    void QueueFolderFanRescan(const std::wstring& root);
    void RestoreWindow(HWND target);
    void RequestSnapshotDebounced(HWND hwnd);
    // Hide the gap overlay while a Start/Search flyout is open or a fullscreen app owns
    // the taskbar's monitor; re-measure once it clears. All heuristics isolated here +
    // in TaskbarOverlayWindow (rule 6).
    void UpdateOverlaySuppression();
    bool FullscreenOnDockMonitor(HWND fg) const;
    // (Re)establish the explorer-PID-scoped LOCATIONCHANGE hook. Called at Create and
    // again on TaskbarCreated — an explorer restart gives the taskbar a new PID, so the
    // old hook is dead and the gap would never re-measure without re-scoping.
    void HookTaskbarLocation();

    HWND              m_hwnd             = nullptr;
    Store             m_store;
    std::vector<HWND> m_pendingValidation;
    std::vector<HWND> m_pendingSnapshots;
    HWINEVENTHOOK     m_winEventHook           = nullptr;
    HWINEVENTHOOK     m_winEventHookMinimize   = nullptr;
    HWINEVENTHOOK     m_winEventHookNameChange = nullptr;
    HWINEVENTHOOK     m_winEventHookForeground = nullptr;
    HWINEVENTHOOK     m_winEventHookLocation   = nullptr;
    HWINEVENTHOOK     m_winEventHookFgLocation = nullptr;  // set-once global fg LOCATIONCHANGE (in-place fullscreen)
    HWINEVENTHOOK     m_winEventHookFlyoutCloak = nullptr; // set-once global CLOAKED (Win11 Start/Search close)
    bool              m_overlaySuppressed      = false;  // current overlay suppression (dedupes)
    // The FolderFan button index the fan is currently showing folders for (-1 = none, or
    // showing tabs instead). Set in ShowFanForButton, read when a Folders-flavor
    // kFanActivateMsg arrives to resolve which button's folderEntries the row belongs to.
    int               m_fannedButtonIndex      = -1;
    // One any-change ConfigWatcher per distinct FolderFan root, rebuilt whenever the
    // config reloads (RebuildFolderFanWatchers). Roots pending a debounced rescan
    // (kFolderFanChangedMsg → kFolderFanScanTimer coalesces a change-burst into one
    // rescan per root) are collected in m_pendingFolderFanRescans.
    std::vector<std::unique_ptr<ConfigWatcher>> m_folderFanWatchers;
    std::vector<std::wstring>                   m_pendingFolderFanRescans;
    UINT              m_taskbarCreatedMsg      = 0;      // RegisterWindowMessageW(L"TaskbarCreated")
    std::unique_ptr<TabReader>     m_tabReader;
    std::unique_ptr<FanPopup>      m_fanPopup;
    std::unique_ptr<ConfigWatcher> m_configWatcher;
    Launcher                       m_launcher;
    // Declared after m_launcher: the overlay holds a Launcher* and its worker is
    // joined in ~TaskbarOverlayWindow, so it must destruct BEFORE m_launcher.
    std::unique_ptr<TaskbarOverlayWindow> m_taskbarOverlay;
};

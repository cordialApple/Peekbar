#pragma once
#include <windows.h>
#include <string>
#include <thread>

// Watches a directory on a worker thread (overlapped ReadDirectoryChangesW) and
// PostMessages the dock on a matching change. All blocking I/O stays off the UI thread
// and communication is PostMessage-only (CLAUDE.md rule 5). A manual-reset stop event
// cancels the blocking wait so teardown joins cleanly.
// Two modes, both via Start(): a non-empty fileName filters to that one file (config.txt
// hot-reload — posts wparam=0/lparam=0); an empty fileName matches ANY change under the
// directory (FolderFan subfolder add/remove/rename — posts lparam = a heap-allocated
// `std::wstring*` holding the watched dir, owned by the receiver — delete after reading,
// or on PostMessageW failure).
class ConfigWatcher
{
public:
    ConfigWatcher(HWND dockHwnd, UINT changeMsg);
    ~ConfigWatcher();
    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

    // fileName empty => any-change mode (see class comment).
    void Start(std::wstring dir, std::wstring fileName);

private:
    void WorkerLoop();

    HWND         m_hwnd;
    UINT         m_changeMsg;
    std::wstring m_dir;
    std::wstring m_file;
    HANDLE       m_stopEvent = nullptr;
    std::thread  m_thread;
};

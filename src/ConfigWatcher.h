#pragma once
#include <windows.h>
#include <string>
#include <thread>

// Watches the config directory on a worker thread (overlapped ReadDirectoryChangesW)
// and PostMessages the dock when the watched file changes. All blocking I/O stays off
// the UI thread and communication is PostMessage-only (CLAUDE.md rule 5). A manual-reset
// stop event cancels the blocking wait so teardown joins cleanly.
class ConfigWatcher
{
public:
    ConfigWatcher(HWND dockHwnd, UINT changeMsg);
    ~ConfigWatcher();
    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

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

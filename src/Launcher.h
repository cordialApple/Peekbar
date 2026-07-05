#pragma once
#include <windows.h>
#include <string>
#include <vector>

enum class ButtonStyle  { Pill, Icon };
enum class ButtonAction { Url, Shortcut, Command };

struct Button {
    std::wstring id;
    ButtonStyle  style  = ButtonStyle::Pill;
    std::wstring label;
    std::wstring iconPath;
    ButtonAction action = ButtonAction::Url;
    std::wstring target;
};

// Owns the automation-button config (CLAUDE.md: Launcher component, Stage 5).
// Minimum-functionality config is a line-based text file (hard rule 2 — no JSON
// dependency); one button per line: style|label|action|target|iconPath(optional).
// Blank lines and lines starting with '#' or ';' are ignored. Malformed lines are
// skipped with an OutputDebugStringW note; a missing file yields zero buttons.
class Launcher
{
public:
    void Load();
    const std::vector<Button>& Buttons() const { return m_buttons; }

    // Theme name from an optional `theme=<name>` config line; empty when unset.
    // Feed to Paint::SetActiveTheme (empty/unknown falls back to "slate").
    const std::wstring& ThemeName() const { return m_themeName; }

    // Fire-and-forget: launches on a detached MTA worker so a slow shell handler
    // never blocks the dock's UI pump (CLAUDE.md rule 5).
    void Execute(const Button& b) const;

    static std::wstring ConfigDir();       // %LOCALAPPDATA%\browser_shell_os
    static std::wstring ConfigFileName();  // config.txt
    static std::wstring ConfigPath();      // ConfigDir()\ConfigFileName()

private:
    std::vector<Button> m_buttons;
    std::wstring        m_themeName;
};

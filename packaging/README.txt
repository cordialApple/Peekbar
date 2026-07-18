Peekbar — quickstart
====================

Peekbar keeps your minimized browser windows glance-able. When you minimize a
browser, its tabs stay behind as hoverable chips in the taskbar's empty gap.
Hover a chip to fan out that window's tabs; click a tab to restore the window
and jump straight to it. You also get one-click launcher buttons in that same
gap for the sites, shortcuts, and project folders you use most.

It reads tab TITLES only (never full URLs), never injects into other processes,
and never touches the network.

Requirements
------------
- Windows 10 (x64) or Windows 11.
- No install, no admin rights, no dependencies to download.

Run it
------
1. Unzip anywhere (e.g. a folder in your Documents).
2. Double-click peekbar.exe.

That's it. Peekbar runs as a hidden coordinator and draws its strip in the
taskbar gap. There is no window in the taskbar. To stop it, end "peekbar.exe"
from Task Manager (Ctrl+Shift+Esc).

Add launcher buttons (optional)
-------------------------------
Your buttons live in a text file at  %LOCALAPPDATA%\Peekbar\config.txt

1. Create the folder:  %LOCALAPPDATA%\Peekbar
   (paste that path into the File Explorer address bar).
   If you ran install.ps1, this folder already holds config.sample.txt.
2. Copy config.sample.txt into it and rename the copy to  config.txt
3. Edit config.txt — see the comments inside for the line format. Peekbar
   hot-reloads within about a second; no restart needed.

Start automatically at logon (optional)
---------------------------------------
From the unzipped folder, in PowerShell:

    powershell -ExecutionPolicy Bypass -File .\install.ps1

That copies peekbar.exe to %LOCALAPPDATA%\Peekbar and registers a per-user
logon autostart (HKCU only — no admin, nothing system-wide). To undo:

    powershell -ExecutionPolicy Bypass -File .\uninstall.ps1

Remove it
---------
Delete the unzipped folder. If you ran install.ps1, run uninstall.ps1 first (or
just delete %LOCALAPPDATA%\Peekbar and the "Peekbar" value under
HKCU\Software\Microsoft\Windows\CurrentVersion\Run).

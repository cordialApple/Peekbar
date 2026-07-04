#pragma once
#include <windows.h>
#include <vector>
#include "Store.h"
#include "Launcher.h"

namespace Renderer
{
    struct CardHit   { RECT rect; HWND hwnd; };
    struct ButtonHit { RECT rect; int  index; };

    void Paint(HDC hdc, const RECT& rc, UINT dpi, const Store& store,
               const std::vector<Button>& buttons);

    // Card rects (dock client coords) for the minimized windows, stacked top→bottom.
    // Shared with hover hit-testing so the layout lives in exactly one place.
    std::vector<CardHit> CardLayout(const RECT& rc, UINT dpi, const Store& store);

    // Automation-button rects (dock client coords), pinned top-right, overlaying the
    // cards. Shared with click hit-testing — single source, same as CardLayout.
    std::vector<ButtonHit> ButtonLayout(const RECT& rc, UINT dpi,
                                        const std::vector<Button>& buttons);

    // Automation-button rects for the taskbar-gap overlay (5b): a left-anchored row of
    // pills, vertically centered, dropping any that don't fit so the overlay never
    // forces the taskbar. Shared with the overlay's click hit-testing.
    std::vector<ButtonHit> GapButtonLayout(const RECT& rc, UINT dpi,
                                           const std::vector<Button>& buttons);

    // Draw one automation-button pill. Shared by the dock strip and the gap overlay.
    void DrawButton(HDC hdc, const RECT& rc, const Button& b, int dpi);
}

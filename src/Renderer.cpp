#include "Renderer.h"
#include <string>

namespace
{
    constexpr COLORREF kBgColor      = RGB(28,  28,  30);
    constexpr COLORREF kCardBg       = RGB(44,  44,  48);
    constexpr COLORREF kChipBg       = RGB(60,  60,  66);
    constexpr COLORREF kTextPrimary  = RGB(220, 220, 220);
    constexpr COLORREF kTextSecond   = RGB(170, 170, 176);

    int ScalePx(int px, int dpiI) { return MulDiv(px, dpiI, 96); }

    HFONT MakeFont(int ptSize, int weight, int dpiI)
    {
        LOGFONTW lf      = {};
        lf.lfHeight      = -MulDiv(ptSize, dpiI, 72);
        lf.lfWeight      = weight;
        lf.lfCharSet     = DEFAULT_CHARSET;
        lf.lfQuality     = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        return CreateFontIndirectW(&lf);
    }

    void DrawCard(HDC hdc, const RECT& cardRc, const TrackedWindow& win, int dpiI)
    {
        HBRUSH cardBrush = CreateSolidBrush(kCardBg);
        FillRect(hdc, &cardRc, cardBrush);
        DeleteObject(cardBrush);

        const int pad = ScalePx(6, dpiI);
        const int cardH = cardRc.bottom - cardRc.top;

        HFONT titleFont = MakeFont(10, FW_SEMIBOLD, dpiI);
        HFONT tabFont   = MakeFont(9,  FW_NORMAL,   dpiI);

        HFONT old = static_cast<HFONT>(SelectObject(hdc, titleFont));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, kTextPrimary);

        RECT titleRc = { cardRc.left + pad, cardRc.top + pad,
                         cardRc.right - pad, cardRc.top + cardH / 2 };
        DrawTextW(hdc, win.title.c_str(), -1, &titleRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, tabFont);

        // Tab row: one truncated chip per tab, like the browser tab strip.
        const RECT rowRc = { cardRc.left + pad, cardRc.top + cardH / 2 + ScalePx(2, dpiI),
                             cardRc.right - pad, cardRc.bottom - pad };
        const int rowW = rowRc.right - rowRc.left;
        const int n    = static_cast<int>(win.tabs.size());

        const int gap      = ScalePx(4, dpiI);
        const int minChipW = ScalePx(46, dpiI);
        const int maxChipW = ScalePx(120, dpiI);
        const int chipPad  = ScalePx(6, dpiI);

        auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

        std::wstring trailer;
        if (n > 0 && rowW > minChipW)
        {
            int chipW    = clampi(rowW / n - gap, minChipW, maxChipW);
            int capacity = (rowW + gap) / (chipW + gap);
            if (capacity < 1) capacity = 1;

            int trailerW = 0;
            const bool mayOverflow = capacity < n;
            if (mayOverflow || win.tabsStale)
            {
                trailerW = ScalePx(80, dpiI) + gap;
                capacity = (rowW - trailerW + gap) / (chipW + gap);
                if (capacity < 1) capacity = 1;
            }

            const int visible = capacity < n ? capacity : n;

            int x = rowRc.left;
            for (int i = 0; i < visible; ++i)
            {
                RECT chip = { x, rowRc.top, x + chipW, rowRc.bottom };
                HBRUSH chipBrush = CreateSolidBrush(kChipBg);
                FillRect(hdc, &chip, chipBrush);
                DeleteObject(chipBrush);

                RECT txt = { chip.left + chipPad, chip.top,
                             chip.right - chipPad, chip.bottom };
                SetTextColor(hdc, kTextPrimary);
                DrawTextW(hdc, win.tabs[i].title.c_str(), -1, &txt,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                x += chipW + gap;
            }

            const int hidden = n - visible;
            if (hidden > 0)
            {
                wchar_t buf[16];
                swprintf_s(buf, L"+%d", hidden);
                trailer = buf;
            }
            if (win.tabsStale)
                trailer += trailer.empty() ? L"(stale)" : L" (stale)";

            if (!trailer.empty())
            {
                RECT tr = { x, rowRc.top, rowRc.right, rowRc.bottom };
                SetTextColor(hdc, kTextSecond);
                DrawTextW(hdc, trailer.c_str(), -1, &tr,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
        else if (win.tabsStale)
        {
            RECT tr = rowRc;
            SetTextColor(hdc, kTextSecond);
            DrawTextW(hdc, L"(stale)", -1, &tr,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(hdc, old);
        DeleteObject(titleFont);
        DeleteObject(tabFont);
    }
}

namespace Renderer
{
    void Paint(HDC hdc, const RECT& rc, UINT dpi, const Store& store)
    {
        HBRUSH bg = CreateSolidBrush(kBgColor);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        const int dpiI = dpi ? static_cast<int>(dpi) : 96;
        const auto& all = store.All();

        if (all.empty())
        {
            HFONT font    = MakeFont(12, FW_NORMAL, dpiI);
            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kTextPrimary);
            RECT textRc = rc;
            DrawTextW(hdc, L"browser: none", -1, &textRc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            return;
        }

        const int pad = ScalePx(4, dpiI);
        const int rcW = rc.right - rc.left;

        int nonMinCount = 0;
        for (const auto& [hwnd, w] : all)
            if (!w.minimized) ++nonMinCount;

        if (nonMinCount > 0)
        {
            const TrackedWindow* first = nullptr;
            for (const auto& [hwnd, w] : all)
                if (!w.minimized) { first = &w; break; }

            std::wstring label = L"browser: " + first->title;
            if (all.size() > 1)
            {
                wchar_t extra[32];
                swprintf_s(extra, L" (+%zu)", all.size() - 1);
                label += extra;
            }

            HFONT font    = MakeFont(12, FW_NORMAL, dpiI);
            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kTextPrimary);
            RECT textRc = rc;
            DrawTextW(hdc, label.c_str(), -1, &textRc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            return;
        }

        int minCount = static_cast<int>(all.size());
        int cardW    = (rcW - pad * (minCount + 1)) / minCount;
        int x        = rc.left + pad;

        for (const auto& [hwnd, win] : all)
        {
            RECT cardRc = { x, rc.top + pad, x + cardW, rc.bottom - pad };
            DrawCard(hdc, cardRc, win, dpiI);
            x += cardW + pad;
        }
    }
}


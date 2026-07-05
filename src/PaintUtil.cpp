#include "PaintUtil.h"

namespace Paint
{
    namespace
    {
        constexpr Theme kSlate = {
            RGB(74, 78, 86),  RGB(48, 50, 56),  RGB(96, 100, 108), kTextPrimary,
            RGB(60, 63, 70),  RGB(40, 42, 48),  kButtonBorder,     kTextPrimary,
            RGB(255, 150, 140), kChipActiveBg,  kTextActive,
            kRowHover,
            true,
        };

        constexpr Theme kMatte = {
            kButtonBg,     kButtonBg,     kButtonBorder, kTextOnBg,
            kCardBg,       kCardBg,       kButtonBorder, kTextPrimary,
            kChipActiveBg, kChipActiveBg, kTextActive,
            kRowHover,
            false,
        };

        constexpr Theme kSteel = {
            RGB(90, 98, 110), RGB(58, 64, 74),  RGB(120, 130, 144), kTextPrimary,
            RGB(66, 72, 82),  RGB(44, 48, 56),  RGB(104, 112, 124), kTextPrimary,
            RGB(255, 150, 140), kChipActiveBg,  kTextActive,
            kRowHover,
            true,
        };

        struct Named { const wchar_t* name; const Theme* theme; };
        constexpr Named kThemes[] = {
            { L"slate", &kSlate },
            { L"matte", &kMatte },
            { L"steel", &kSteel },
        };

        const Theme* g_active = &kSlate;
    }

    const Theme& ActiveTheme() { return *g_active; }

    void SetActiveTheme(const std::wstring& name)
    {
        for (const auto& t : kThemes)
        {
            if (name == t.name) { g_active = t.theme; return; }
        }
        g_active = &kSlate;
    }
}

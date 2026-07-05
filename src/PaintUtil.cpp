#include "PaintUtil.h"

namespace Paint
{
    namespace
    {
        constexpr Theme kSlate = {
            RGB(80, 84, 92),  RGB(44, 46, 52),  RGB(104, 108, 116), kTextPrimary,
            RGB(72, 76, 86),  RGB(38, 40, 48),  RGB(96, 100, 110),  kTextPrimary,
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
            RGB(92, 100, 114), RGB(54, 60, 72),  RGB(124, 134, 150), kTextPrimary,
            RGB(82, 90, 104),  RGB(46, 52, 64),  RGB(112, 122, 138), kTextPrimary,
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

#pragma once

#include "imgui.h"
#include "implot.h"

namespace sdr::ui {

struct ThemeColors {
    // Cyber Charcoal & Deep Space Slate
    static constexpr ImVec4 WindowBg         = ImVec4(0.055f, 0.065f, 0.085f, 1.00f); // #0e1116
    static constexpr ImVec4 CardBg           = ImVec4(0.085f, 0.100f, 0.135f, 0.98f); // #161a22
    static constexpr ImVec4 CardBgHovered    = ImVec4(0.120f, 0.145f, 0.190f, 1.00f); // #1f2530
    static constexpr ImVec4 CardBorder       = ImVec4(0.180f, 0.210f, 0.280f, 0.70f); // #2e3647
    static constexpr ImVec4 BorderActive     = ImVec4(1.000f, 0.460f, 0.000f, 0.90f); // #ff7500

    // Primary Cyber Orange Accent
    static constexpr ImVec4 PrimaryAccent    = ImVec4(1.000f, 0.450f, 0.000f, 1.00f); // #ff7300
    static constexpr ImVec4 PrimaryHovered   = ImVec4(1.000f, 0.550f, 0.180f, 1.00f); // #ff8c2e
    static constexpr ImVec4 PrimaryActive    = ImVec4(0.880f, 0.380f, 0.000f, 1.00f); // #e06100

    // Secondary & Complementary Accents
    static constexpr ImVec4 AmberBadge       = ImVec4(0.980f, 0.650f, 0.050f, 1.00f); // #fba60d (Gold/Amber)
    static constexpr ImVec4 CyanContrast     = ImVec4(0.220f, 0.740f, 0.970f, 1.00f); // #38bdf8 (Ice Cyan for Q-channel)
    static constexpr ImVec4 SuccessGreen     = ImVec4(0.100f, 0.800f, 0.500f, 1.00f); // #1abf80 (Emerald)
    static constexpr ImVec4 RedAlert         = ImVec4(0.950f, 0.250f, 0.250f, 1.00f); // #f24040

    // Chip & Button Surfaces
    static constexpr ImVec4 ChipBg           = ImVec4(0.105f, 0.125f, 0.165f, 0.95f); // #1b202a
    static constexpr ImVec4 ChipBorder       = ImVec4(0.200f, 0.240f, 0.320f, 0.60f); // #333d52

    // High Contrast Typography
    static constexpr ImVec4 TextPrimary      = ImVec4(0.980f, 0.985f, 0.990f, 1.00f); // #fafbfc
    static constexpr ImVec4 TextSecondary    = ImVec4(0.650f, 0.700f, 0.780f, 1.00f); // #a6b3c7
    static constexpr ImVec4 TextTertiary     = ImVec4(0.420f, 0.460f, 0.540f, 1.00f); // #6b758a
};

void apply_cyber_orange_theme();
void apply_implot_theme();

// Удобные виджеты
bool orange_button(const char* label, const ImVec2& size = ImVec2(0, 0), bool active = false);
bool filter_chip(const char* label, bool active = false);
void render_card_header(const char* title, const char* subtitle = nullptr);
void render_status_pill(const char* label, bool is_online);

} // namespace sdr::ui

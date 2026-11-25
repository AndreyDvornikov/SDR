#include "ui/theme.hpp"

namespace sdr::ui {

void apply_cyber_orange_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- Geometries & Rounding ---
    style.WindowRounding    = 16.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 10.0f;
    style.PopupRounding     = 12.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 10.0f;
    style.TabRounding       = 10.0f;

    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(14.0f, 14.0f);
    style.FramePadding      = ImVec2(12.0f, 7.0f);
    style.ItemSpacing       = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 8.0f;

    // --- Color Assignments ---
    colors[ImGuiCol_Text]                  = ThemeColors::TextPrimary;
    colors[ImGuiCol_TextDisabled]          = ThemeColors::TextTertiary;
    colors[ImGuiCol_WindowBg]              = ThemeColors::WindowBg;
    colors[ImGuiCol_ChildBg]               = ThemeColors::CardBg;
    colors[ImGuiCol_PopupBg]               = ThemeColors::CardBgHovered;

    colors[ImGuiCol_Border]                = ThemeColors::CardBorder;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Frame / Inputs / Chips
    colors[ImGuiCol_FrameBg]               = ThemeColors::ChipBg;
    colors[ImGuiCol_FrameBgHovered]        = ThemeColors::CardBgHovered;
    colors[ImGuiCol_FrameBgActive]         = ThemeColors::CardBg;

    // Titles & Menu
    colors[ImGuiCol_TitleBg]               = ThemeColors::CardBg;
    colors[ImGuiCol_TitleBgActive]         = ThemeColors::CardBg;
    colors[ImGuiCol_TitleBgCollapsed]      = ThemeColors::CardBg;
    colors[ImGuiCol_MenuBarBg]             = ThemeColors::CardBg;

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.28f, 0.36f, 0.50f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ThemeColors::PrimaryHovered;
    colors[ImGuiCol_ScrollbarGrabActive]   = ThemeColors::PrimaryAccent;

    // Sliders & Checks
    colors[ImGuiCol_CheckMark]             = ThemeColors::PrimaryAccent;
    colors[ImGuiCol_SliderGrab]            = ThemeColors::PrimaryAccent;
    colors[ImGuiCol_SliderGrabActive]      = ThemeColors::PrimaryHovered;

    // Buttons
    colors[ImGuiCol_Button]                = ThemeColors::ChipBg;
    colors[ImGuiCol_ButtonHovered]         = ThemeColors::CardBgHovered;
    colors[ImGuiCol_ButtonActive]          = ThemeColors::PrimaryAccent;

    // Headers
    colors[ImGuiCol_Header]                = ThemeColors::ChipBg;
    colors[ImGuiCol_HeaderHovered]         = ThemeColors::CardBgHovered;
    colors[ImGuiCol_HeaderActive]          = ThemeColors::PrimaryAccent;

    // Separators
    colors[ImGuiCol_Separator]             = ThemeColors::CardBorder;
    colors[ImGuiCol_SeparatorHovered]      = ThemeColors::BorderActive;
    colors[ImGuiCol_SeparatorActive]       = ThemeColors::PrimaryAccent;

    // Tabs
    colors[ImGuiCol_Tab]                   = ThemeColors::CardBg;
    colors[ImGuiCol_TabHovered]            = ThemeColors::CardBgHovered;
    colors[ImGuiCol_TabSelected]           = ThemeColors::PrimaryAccent;
    colors[ImGuiCol_TabDimmed]             = ThemeColors::CardBg;
    colors[ImGuiCol_TabDimmedSelected]     = ThemeColors::CardBgHovered;

    // Plotting defaults
    colors[ImGuiCol_PlotLines]             = ThemeColors::PrimaryAccent;
    colors[ImGuiCol_PlotLinesHovered]      = ThemeColors::AmberBadge;
    colors[ImGuiCol_PlotHistogram]         = ThemeColors::PrimaryAccent;
    colors[ImGuiCol_PlotHistogramHovered]  = ThemeColors::AmberBadge;

    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.04f, 0.05f, 0.08f, 0.75f);
}

void apply_implot_theme() {
    ImPlotStyle& pstyle = ImPlot::GetStyle();

    pstyle.MinorAlpha       = 0.15f;
    pstyle.MajorGridSize    = ImVec2(1.0f, 1.0f);
    pstyle.MinorGridSize    = ImVec2(0.5f, 0.5f);
    pstyle.PlotBorderSize   = 0.0f;

    pstyle.Colors[ImPlotCol_PlotBg]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    pstyle.Colors[ImPlotCol_PlotBorder]     = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    pstyle.Colors[ImPlotCol_LegendBg]       = ThemeColors::CardBg;
    pstyle.Colors[ImPlotCol_LegendBorder]   = ThemeColors::CardBorder;
    pstyle.Colors[ImPlotCol_LegendText]     = ThemeColors::TextSecondary;
    pstyle.Colors[ImPlotCol_TitleText]      = ThemeColors::TextPrimary;

    pstyle.Colors[ImPlotCol_AxisGrid]       = ImVec4(0.20f, 0.24f, 0.32f, 0.30f);
    pstyle.Colors[ImPlotCol_AxisText]       = ThemeColors::TextTertiary;

    // 4-color palette with Cyber Orange as hero
    static const ImVec4 cyberOrangePalette[4] = {
        ThemeColors::PrimaryAccent, // Warm Cyber Orange
        ThemeColors::CyanContrast,  // Ice Cyan
        ThemeColors::AmberBadge,    // Sunset Gold
        ThemeColors::SuccessGreen   // Emerald
    };
    ImPlot::AddColormap("CyberOrange", cyberOrangePalette, 4);
    pstyle.Colormap = ImPlot::GetColormapIndex("CyberOrange");

    // Dedicated Waterfall colormap: deep dark background -> cyan -> glowing cyber orange -> white peak
    static const ImVec4 waterfallPalette[5] = {
        ImVec4(0.045f, 0.055f, 0.075f, 1.0f), // 0.00: Deep Dark Space / Noise floor
        ImVec4(0.100f, 0.160f, 0.260f, 1.0f), // 0.25: Dark Navy
        ImVec4(0.150f, 0.550f, 0.750f, 1.0f), // 0.50: Ice Cyan / Mid signal
        ImVec4(1.000f, 0.450f, 0.000f, 1.0f), // 0.75: Cyber Orange / Strong signal
        ImVec4(1.000f, 0.950f, 0.800f, 1.0f)  // 1.00: White-Hot Peak
    };
    ImPlot::AddColormap("WaterfallHotOrange", waterfallPalette, 5);
}

bool orange_button(const char* label, const ImVec2& size, bool active) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ThemeColors::PrimaryHovered : ThemeColors::PrimaryAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColors::PrimaryHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ThemeColors::PrimaryActive);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

bool filter_chip(const char* label, bool active) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ThemeColors::PrimaryAccent : ThemeColors::ChipBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColors::CardBgHovered);
    ImGui::PushStyleColor(ImGuiCol_Border, active ? ThemeColors::BorderActive : ThemeColors::ChipBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, active ? ThemeColors::TextPrimary : ThemeColors::TextSecondary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

void render_card_header(const char* title, const char* subtitle) {
    ImGui::TextColored(ThemeColors::TextPrimary, "%s", title);
    if (subtitle && subtitle[0] != '\0') {
        ImGui::TextColored(ThemeColors::TextTertiary, "%s", subtitle);
    }
    ImGui::Spacing();
}

void render_status_pill(const char* label, bool is_online) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float radius = 5.0f;

    ImU32 color = is_online ? IM_COL32(26, 191, 128, 255) : IM_COL32(242, 64, 64, 255);
    draw_list->AddCircleFilled(ImVec2(pos.x + 8.0f, pos.y + 10.0f), radius, color);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
    ImGui::TextColored(is_online ? ThemeColors::TextPrimary : ThemeColors::TextSecondary, "%s", label);
}

} // namespace sdr::ui

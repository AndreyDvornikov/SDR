#include "ui/widgets.hpp"
#include "ui/theme.hpp"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace sdr::ui {

constexpr float PI = 3.14159265358979323846f;

void draw_radial_gauge(
    const char* id,
    const char* title,
    float value,
    float min_val,
    float max_val,
    const char* unit,
    ImVec4 arc_color,
    float radius)
{
    ImGui::PushID(id);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float diameter = radius * 2.0f;
    float padding = 8.0f;
    float card_w = diameter + padding * 2.0f;
    float card_h = diameter + 34.0f;

    ImVec2 p_min = cursor;
    ImVec2 p_max = ImVec2(cursor.x + card_w, cursor.y + card_h);

    ImGui::InvisibleButton(id, ImVec2(card_w, card_h));

    draw_list->PushClipRect(p_min, p_max, true);

    ImVec2 center(cursor.x + card_w * 0.5f, cursor.y + radius + padding);

    // Arc angles: 135 deg to 405 deg (270 degrees sweep)
    const float start_angle = 135.0f * (PI / 180.0f);
    const float end_angle   = 405.0f * (PI / 180.0f);
    const float sweep = end_angle - start_angle;
    const float track_thickness = 7.0f;

    // 1. Background circular track
    draw_list->PathClear();
    draw_list->PathArcTo(center, radius - track_thickness * 0.5f, start_angle, end_angle, 48);
    draw_list->PathStroke(IM_COL32(32, 38, 50, 180), 0, track_thickness);

    // 2. Active colored progress arc
    float clamped_val = std::clamp(value, min_val, max_val);
    float t = (max_val > min_val) ? ((clamped_val - min_val) / (max_val - min_val)) : 0.0f;
    float current_angle = start_angle + sweep * t;

    if (t > 0.005f) {
        ImU32 col_active = ImGui::ColorConvertFloat4ToU32(arc_color);
        draw_list->PathClear();
        draw_list->PathArcTo(center, radius - track_thickness * 0.5f, start_angle, current_angle, 48);
        draw_list->PathStroke(col_active, 0, track_thickness);

        // Glowing tip at the end of progress
        float tip_x = center.x + (radius - track_thickness * 0.5f) * std::cos(current_angle);
        float tip_y = center.y + (radius - track_thickness * 0.5f) * std::sin(current_angle);
        draw_list->AddCircleFilled(ImVec2(tip_x, tip_y), track_thickness * 0.65f, IM_COL32(255, 255, 255, 250));
    }

    // 3. Central numeric value readout
    char val_buf[32];
    std::snprintf(val_buf, sizeof(val_buf), "%.1f", value);
    ImVec2 text_sz = ImGui::CalcTextSize(val_buf);
    draw_list->AddText(ImVec2(center.x - text_sz.x * 0.5f, center.y - text_sz.y * 0.65f),
                       ImGui::ColorConvertFloat4ToU32(ThemeColors::TextPrimary), val_buf);

    // Unit label below value
    if (unit && unit[0] != '\0') {
        ImVec2 unit_sz = ImGui::CalcTextSize(unit);
        draw_list->AddText(ImVec2(center.x - unit_sz.x * 0.5f, center.y + text_sz.y * 0.45f),
                           ImGui::ColorConvertFloat4ToU32(ThemeColors::TextSecondary), unit);
    }

    // Title label at bottom
    if (title && title[0] != '\0') {
        ImVec2 title_sz = ImGui::CalcTextSize(title);
        draw_list->AddText(ImVec2(center.x - title_sz.x * 0.5f, center.y + radius + 4.0f),
                           ImGui::ColorConvertFloat4ToU32(ThemeColors::TextTertiary), title);
    }

    draw_list->PopClipRect();
    ImGui::PopID();
}

void draw_metric_card(
    const char* id,
    const char* title,
    const char* value_str,
    const char* subtitle,
    ImVec4 accent_color,
    const std::vector<float>& sparkline,
    ImVec2 card_size)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (card_size.x <= 0.0f) card_size.x = ImGui::GetContentRegionAvail().x;
    if (card_size.y <= 0.0f) card_size.y = 92.0f;

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + card_size.x, pos.y + card_size.y);

    ImGui::InvisibleButton(id, card_size);

    draw_list->PushClipRect(p_min, p_max, true);

    // 1. Card background with subtle border
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 24, 32, 245), 14.0f);
    draw_list->AddRect(p_min, p_max, IM_COL32(40, 48, 64, 160), 14.0f, 0, 1.0f);

    // Accent stripe on left edge
    ImU32 col_accent = ImGui::ColorConvertFloat4ToU32(accent_color);
    draw_list->AddRectFilled(ImVec2(p_min.x + 2.0f, p_min.y + 12.0f),
                             ImVec2(p_min.x + 5.0f, p_max.y - 12.0f),
                             col_accent, 3.0f);

    // 2. Title
    draw_list->AddText(ImVec2(p_min.x + 14.0f, p_min.y + 10.0f),
                       ImGui::ColorConvertFloat4ToU32(ThemeColors::TextSecondary), title);

    // 3. Value
    draw_list->AddText(ImVec2(p_min.x + 14.0f, p_min.y + 30.0f),
                       ImGui::ColorConvertFloat4ToU32(ThemeColors::TextPrimary), value_str);

    // 4. Subtitle
    if (subtitle && subtitle[0] != '\0') {
        draw_list->AddText(ImVec2(p_min.x + 14.0f, p_min.y + 64.0f),
                           col_accent, subtitle);
    }

    // 5. Sparkline on the right
    if (sparkline.size() >= 2) {
        float spark_w = card_size.x * 0.35f;
        float spark_h = 34.0f;
        float spark_x = p_max.x - spark_w - 14.0f;
        float spark_y = p_min.y + (card_size.y - spark_h) * 0.5f;

        float min_s = *std::min_element(sparkline.begin(), sparkline.end());
        float max_s = *std::max_element(sparkline.begin(), sparkline.end());
        if (std::abs(max_s - min_s) < 1e-4f) max_s = min_s + 1.0f;

        std::vector<ImVec2> pts;
        pts.reserve(sparkline.size());
        for (size_t i = 0; i < sparkline.size(); ++i) {
            float norm_x = spark_x + spark_w * (static_cast<float>(i) / (sparkline.size() - 1));
            float norm_y = (spark_y + spark_h) - spark_h * ((sparkline[i] - min_s) / (max_s - min_s));
            pts.push_back(ImVec2(norm_x, norm_y));
        }

        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            draw_list->AddLine(pts[i], pts[i + 1], col_accent, 2.0f);
        }
        draw_list->AddCircleFilled(pts.back(), 3.5f, IM_COL32(255, 255, 255, 250));
    }

    draw_list->PopClipRect();
    ImGui::PopID();
}

void draw_radar_constellation(
    const char* id,
    const std::vector<float>& rx_i,
    const std::vector<float>& rx_q,
    const std::vector<std::complex<float>>& ref_symbols,
    ImVec2 size,
    float evm_percent,
    float forced_scale)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0.0f) size.y = size.x;

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(id, size);

    draw_list->PushClipRect(p_min, p_max, true);

    // 1. Radar plate background
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(14, 17, 24, 250), 16.0f);
    draw_list->AddRect(p_min, p_max, IM_COL32(38, 46, 62, 180), 16.0f, 0, 1.0f);

    ImVec2 center(p_min.x + size.x * 0.5f, p_min.y + size.y * 0.5f);
    float radius = std::min(size.x, size.y) * 0.44f;

    // 2. Dynamic Auto-Scale
    float max_c = 1e-3f;
    for (float v : rx_i) max_c = std::max(max_c, std::abs(v));
    for (float v : rx_q) max_c = std::max(max_c, std::abs(v));

    float max_range = (forced_scale > 0.0f) ? forced_scale : (max_c > 0.1f ? max_c * 1.25f : 1.5f);

    // 3. Range rings
    const float ring_scales[] = { 0.33f, 0.67f, 1.0f };
    for (float sc : ring_scales) {
        draw_list->AddCircle(center, radius * sc, IM_COL32(50, 60, 80, 100), 48, 1.0f);
    }

    // 4. Crosshairs
    draw_list->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y),
                       IM_COL32(50, 60, 80, 140), 1.0f);
    draw_list->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius),
                       IM_COL32(50, 60, 80, 140), 1.0f);

    draw_list->AddText(ImVec2(center.x + radius - 16.0f, center.y + 4.0f),
                       IM_COL32(140, 150, 170, 180), "+I");
    draw_list->AddText(ImVec2(center.x + 4.0f, center.y - radius + 2.0f),
                       IM_COL32(140, 150, 170, 180), "+Q");

    auto iq_to_screen = [&](float i, float q) -> ImVec2 {
        float sx = center.x + (i / max_range) * radius;
        float sy = center.y - (q / max_range) * radius;
        return ImVec2(sx, sy);
    };

    // 5. Ideal reference targets (e.g. QPSK crosses)
    for (const auto& r : ref_symbols) {
        ImVec2 pt = iq_to_screen(r.real(), r.imag());
        float cross_sz = 6.0f;
        draw_list->AddLine(ImVec2(pt.x - cross_sz, pt.y), ImVec2(pt.x + cross_sz, pt.y),
                           IM_COL32(56, 189, 248, 220), 1.5f);
        draw_list->AddLine(ImVec2(pt.x, pt.y - cross_sz), ImVec2(pt.x, pt.y + cross_sz),
                           IM_COL32(56, 189, 248, 220), 1.5f);
        draw_list->AddCircle(pt, 8.0f, IM_COL32(56, 189, 248, 140), 16, 1.0f);
    }

    // 6. Received symbols cloud with Cyber Orange glowing points & persistence fade
    const size_t total_pts = std::min(rx_i.size(), rx_q.size());
    for (size_t k = 0; k < total_pts; ++k) {
        float norm_age = static_cast<float>(k) / static_cast<float>(total_pts);
        int alpha = static_cast<int>(35 + 220 * norm_age);
        ImVec2 pt = iq_to_screen(rx_i[k], rx_q[k]);

        // Draw glowing cyber orange dot
        draw_list->AddCircleFilled(pt, 3.5f, IM_COL32(255, 115, 0, alpha));
    }

    // 7. Corner Badges (EVM & Auto-scale factor)
    if (evm_percent >= 0.0f) {
        char evm_buf[48];
        std::snprintf(evm_buf, sizeof(evm_buf), "EVM: %.1f %%", evm_percent);
        ImVec4 evm_col = (evm_percent < 15.0f) ? ThemeColors::SuccessGreen :
                        ((evm_percent < 30.0f) ? ThemeColors::AmberBadge : ThemeColors::RedAlert);

        draw_list->AddRectFilled(ImVec2(p_min.x + 10.0f, p_min.y + 10.0f),
                                 ImVec2(p_min.x + 105.0f, p_min.y + 32.0f),
                                 IM_COL32(22, 26, 36, 230), 8.0f);
        draw_list->AddText(ImVec2(p_min.x + 16.0f, p_min.y + 13.0f),
                           ImGui::ColorConvertFloat4ToU32(evm_col), evm_buf);
    }

    draw_list->PopClipRect();
    ImGui::PopID();
}

void draw_gradient_spectrum(
    const char* id,
    const std::vector<float>& freq_axis,
    const std::vector<float>& power_db,
    ImVec2 size,
    float min_db,
    float max_db)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0.0f) size.y = 200.0f;

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(id, size);

    draw_list->PushClipRect(p_min, p_max, true);

    // Card background
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(14, 17, 24, 250), 16.0f);
    draw_list->AddRect(p_min, p_max, IM_COL32(38, 46, 62, 180), 16.0f, 0, 1.0f);

    float inner_pad_x = 12.0f;
    float inner_pad_y = 12.0f;
    ImVec2 plot_min(p_min.x + inner_pad_x, p_min.y + inner_pad_y);
    ImVec2 plot_max(p_max.x - inner_pad_x, p_max.y - inner_pad_y);
    float plot_w = plot_max.x - plot_min.x;
    float plot_h = plot_max.y - plot_min.y;

    // Grid lines
    for (int g = 1; g <= 4; ++g) {
        float gy = plot_min.y + plot_h * (g * 0.2f);
        draw_list->AddLine(ImVec2(plot_min.x, gy), ImVec2(plot_max.x, gy),
                           IM_COL32(36, 44, 60, 90), 1.0f);
    }

    if (power_db.size() >= 2) {
        const float db_range = (max_db > min_db) ? (max_db - min_db) : 1.0f;
        const size_t n = power_db.size();

        std::vector<ImVec2> line_pts;
        line_pts.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            float px = plot_min.x + plot_w * (static_cast<float>(i) / (n - 1));
            float norm_p = std::clamp((power_db[i] - min_db) / db_range, 0.0f, 1.0f);
            float py = plot_max.y - plot_h * norm_p;
            line_pts.push_back(ImVec2(px, py));
        }

        // Filled warm sunset/orange gradient area under the curve
        for (size_t i = 0; i + 1 < n; ++i) {
            ImVec2 quad[4] = {
                line_pts[i],
                line_pts[i + 1],
                ImVec2(line_pts[i + 1].x, plot_max.y),
                ImVec2(line_pts[i].x, plot_max.y)
            };
            draw_list->AddQuadFilled(quad[0], quad[1], quad[2], quad[3], IM_COL32(255, 115, 0, 55));
        }

        // Top illuminated bright cyber orange curve
        for (size_t i = 0; i + 1 < n; ++i) {
            draw_list->AddLine(line_pts[i], line_pts[i + 1], IM_COL32(255, 125, 10, 255), 2.0f);
        }
    }

    // Frequency markers along bottom edge
    draw_list->AddText(ImVec2(plot_min.x + 4.0f, plot_max.y - 18.0f),
                       IM_COL32(148, 163, 184, 180), "-500 kHz");
    draw_list->AddText(ImVec2(plot_min.x + plot_w * 0.5f - 20.0f, plot_max.y - 18.0f),
                       IM_COL32(255, 140, 20, 230), "f0 (Центр)");
    draw_list->AddText(ImVec2(plot_max.x - 65.0f, plot_max.y - 18.0f),
                       IM_COL32(148, 163, 184, 180), "+500 kHz");

    draw_list->PopClipRect();
    ImGui::PopID();
}

void draw_oscilloscope_screen(
    const char* id,
    const std::vector<float>& i_samples,
    const std::vector<float>& q_samples,
    ImVec2 size,
    float forced_scale,
    bool eye_diagram_mode,
    size_t sps)
{
    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (size.x <= 0.0f) size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0.0f) size.y = 200.0f;

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::InvisibleButton(id, size);

    // CRITICAL FIX: PushClipRect ensures ZERO lines spill outside the oscilloscope card!
    draw_list->PushClipRect(p_min, p_max, true);

    // Dark screen background
    draw_list->AddRectFilled(p_min, p_max, IM_COL32(11, 14, 20, 255), 14.0f);
    draw_list->AddRect(p_min, p_max, IM_COL32(36, 44, 60, 180), 14.0f, 0, 1.0f);

    float pad = 12.0f;
    ImVec2 s_min(p_min.x + pad, p_min.y + pad);
    ImVec2 s_max(p_max.x - pad, p_max.y - pad);
    float w = s_max.x - s_min.x;
    float h = s_max.y - s_min.y;
    float mid_y = s_min.y + h * 0.5f;

    // Grid divisions (8 horizontal divisions, 6 vertical divisions)
    for (int gx = 1; gx < 8; ++gx) {
        float x = s_min.x + w * (gx / 8.0f);
        draw_list->AddLine(ImVec2(x, s_min.y), ImVec2(x, s_max.y), IM_COL32(26, 33, 46, 110), 1.0f);
    }
    for (int gy = 1; gy < 6; ++gy) {
        float y = s_min.y + h * (gy / 6.0f);
        draw_list->AddLine(ImVec2(s_min.x, y), ImVec2(s_max.x, y), IM_COL32(26, 33, 46, 110), 1.0f);
    }
    // Center zero baseline
    draw_list->AddLine(ImVec2(s_min.x, mid_y), ImVec2(s_max.x, mid_y), IM_COL32(45, 56, 80, 180), 1.0f);

    // Dynamic auto-scale calculation prevents vertical overflow
    float max_val = 1e-3f;
    for (float v : i_samples) max_val = std::max(max_val, std::abs(v));
    for (float v : q_samples) max_val = std::max(max_val, std::abs(v));

    float amp_scale = (forced_scale > 0.0f) ? forced_scale : (max_val * 1.15f);
    if (amp_scale < 1e-4f) amp_scale = 1.0f;

    size_t count = std::min(i_samples.size(), q_samples.size());
    if (count >= 2) {
        float half_h = h * 0.44f;

        if (eye_diagram_mode) {
            // Eye diagram: overlay 2-symbol slices (2 * sps samples per trace)
            size_t window_len = 2 * (sps > 0 ? sps : 10);
            if (window_len < 4) window_len = 20;

            // Draw vertical symbol strobe cursor in the center of the eye (at t = 1.0 symbol)
            float strobe_x = s_min.x + w * 0.5f;
            draw_list->AddLine(ImVec2(strobe_x, s_min.y), ImVec2(strobe_x, s_max.y),
                               IM_COL32(255, 200, 50, 180), 1.5f);
            draw_list->AddText(ImVec2(strobe_x + 6.0f, s_min.y + 6.0f),
                               IM_COL32(255, 200, 50, 220), "СТРОБ t_opt");

            // Overlay multiple traces with alpha
            size_t step = (sps > 0) ? sps : 10;
            size_t max_traces = 48;
            size_t traces_drawn = 0;

            for (size_t start = 0; start + window_len <= count && traces_drawn < max_traces; start += step) {
                // Draw I-channel trace (Cyber Orange with alpha)
                for (size_t k = 0; k + 1 < window_len; ++k) {
                    float x1 = s_min.x + w * (static_cast<float>(k) / (window_len - 1));
                    float y1 = std::clamp(mid_y - half_h * (i_samples[start + k] / amp_scale), s_min.y, s_max.y);
                    float x2 = s_min.x + w * (static_cast<float>(k + 1) / (window_len - 1));
                    float y2 = std::clamp(mid_y - half_h * (i_samples[start + k + 1] / amp_scale), s_min.y, s_max.y);
                    draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 115, 0, 65), 1.5f);
                }

                // Draw Q-channel trace (Ice Cyan with alpha)
                for (size_t k = 0; k + 1 < window_len; ++k) {
                    float x1 = s_min.x + w * (static_cast<float>(k) / (window_len - 1));
                    float y1 = std::clamp(mid_y - half_h * (q_samples[start + k] / amp_scale), s_min.y, s_max.y);
                    float x2 = s_min.x + w * (static_cast<float>(k + 1) / (window_len - 1));
                    float y2 = std::clamp(mid_y - half_h * (q_samples[start + k + 1] / amp_scale), s_min.y, s_max.y);
                    draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(56, 189, 248, 65), 1.5f);
                }
                traces_drawn++;
            }
        } else {
            // Normal continuous waveform I (Cyber Orange)
            for (size_t k = 0; k + 1 < count; ++k) {
                float x1 = s_min.x + w * (static_cast<float>(k) / (count - 1));
                float y1 = std::clamp(mid_y - half_h * (i_samples[k] / amp_scale), s_min.y, s_max.y);
                float x2 = s_min.x + w * (static_cast<float>(k + 1) / (count - 1));
                float y2 = std::clamp(mid_y - half_h * (i_samples[k + 1] / amp_scale), s_min.y, s_max.y);
                draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 115, 0, 245), 2.0f);
            }

            // Waveform Q (Ice Cyan for crisp contrast against orange)
            for (size_t k = 0; k + 1 < count; ++k) {
                float x1 = s_min.x + w * (static_cast<float>(k) / (count - 1));
                float y1 = std::clamp(mid_y - half_h * (q_samples[k] / amp_scale), s_min.y, s_max.y);
                float x2 = s_min.x + w * (static_cast<float>(k + 1) / (count - 1));
                float y2 = std::clamp(mid_y - half_h * (q_samples[k + 1] / amp_scale), s_min.y, s_max.y);
                draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(56, 189, 248, 245), 2.0f);
            }
        }
    }

    // Legend Pills
    const char* label_i = eye_diagram_mode ? "I Eye (Синфазная)" : "I (Real)";
    const char* label_q = eye_diagram_mode ? "Q Eye (Квадратура)" : "Q (Imag)";

    draw_list->AddRectFilled(ImVec2(s_min.x + 8.0f, s_min.y + 6.0f),
                             ImVec2(s_min.x + (eye_diagram_mode ? 140.0f : 78.0f), s_min.y + 24.0f),
                             IM_COL32(20, 25, 36, 210), 6.0f);
    draw_list->AddCircleFilled(ImVec2(s_min.x + 18.0f, s_min.y + 15.0f), 3.5f, IM_COL32(255, 115, 0, 255));
    draw_list->AddText(ImVec2(s_min.x + 28.0f, s_min.y + 8.0f), IM_COL32(245, 248, 255, 240), label_i);

    float q_pill_start = s_min.x + (eye_diagram_mode ? 148.0f : 88.0f);
    draw_list->AddRectFilled(ImVec2(q_pill_start, s_min.y + 6.0f),
                             ImVec2(q_pill_start + (eye_diagram_mode ? 150.0f : 70.0f), s_min.y + 24.0f),
                             IM_COL32(20, 25, 36, 210), 6.0f);
    draw_list->AddCircleFilled(ImVec2(q_pill_start + 10.0f, s_min.y + 15.0f), 3.5f, IM_COL32(56, 189, 248, 255));
    draw_list->AddText(ImVec2(q_pill_start + 20.0f, s_min.y + 8.0f), IM_COL32(245, 248, 255, 240), label_q);

    // Auto-scale readout on top-right
    char scale_buf[32];
    std::snprintf(scale_buf, sizeof(scale_buf), "±%.1f V/div", amp_scale);
    draw_list->AddText(ImVec2(s_max.x - 90.0f, s_min.y + 8.0f), IM_COL32(148, 163, 184, 200), scale_buf);

    draw_list->PopClipRect();
    ImGui::PopID();
}

bool draw_segmented_selector(
    const char* id,
    const char* const items[],
    int count,
    int* selected_index,
    float width)
{
    ImGui::PushID(id);
    bool value_changed = false;

    if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
    float height = 34.0f;
    float item_w = width / static_cast<float>(count);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(id, ImVec2(width, height));
    bool is_hovered = ImGui::IsItemHovered();

    draw_list->PushClipRect(pos, ImVec2(pos.x + width, pos.y + height), true);

    // Background pill container
    draw_list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                             IM_COL32(18, 22, 30, 240), height * 0.5f);
    draw_list->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
                       IM_COL32(38, 46, 62, 160), height * 0.5f, 0, 1.0f);

    // Active item highlight in Cyber Orange
    float active_x = pos.x + item_w * (*selected_index);
    draw_list->AddRectFilled(ImVec2(active_x + 2.0f, pos.y + 2.0f),
                             ImVec2(active_x + item_w - 2.0f, pos.y + height - 2.0f),
                             IM_COL32(255, 115, 0, 255), (height - 4.0f) * 0.5f);

    // Mouse click handling
    if (is_hovered && ImGui::IsMouseClicked(0)) {
        float rel_x = ImGui::GetIO().MousePos.x - pos.x;
        int clicked_idx = static_cast<int>(rel_x / item_w);
        if (clicked_idx >= 0 && clicked_idx < count && clicked_idx != *selected_index) {
            *selected_index = clicked_idx;
            value_changed = true;
        }
    }

    // Text labels
    for (int i = 0; i < count; ++i) {
        ImVec2 text_sz = ImGui::CalcTextSize(items[i]);
        float tx = pos.x + item_w * i + (item_w - text_sz.x) * 0.5f;
        float ty = pos.y + (height - text_sz.y) * 0.5f;

        ImU32 text_col = (i == *selected_index) ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 175, 195, 200);
        draw_list->AddText(ImVec2(tx, ty), text_col, items[i]);
    }

    draw_list->PopClipRect();
    ImGui::PopID();
    return value_changed;
}

bool draw_glow_slider(
    const char* label,
    float* value,
    float min_val,
    float max_val,
    const char* format,
    ImVec4 track_color)
{
    ImGui::PushID(label);
    ImGui::TextColored(ThemeColors::TextSecondary, "%s", label);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ThemeColors::ChipBg);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, track_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ThemeColors::AmberBadge);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

    bool changed = ImGui::SliderFloat("##glowslider", value, min_val, max_val, format);

    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::PopID();
    return changed;
}

void draw_ponder_perspective_grid(
    ImDrawList* draw_list,
    ImVec2 p_min,
    ImVec2 p_max,
    float alpha)
{
    float w = p_max.x - p_min.x;
    float h = p_max.y - p_min.y;
    if (w <= 10.0f || h <= 10.0f) return;

    ImVec2 center(p_min.x + w * 0.5f, p_min.y + h * 0.40f);
    float horizon_y = p_min.y + h * 0.22f;
    float bottom_y = p_max.y;

    ImU32 line_col = IM_COL32(95, 130, 160, static_cast<int>(255 * alpha));
    ImU32 horizon_col = IM_COL32(242, 163, 27, static_cast<int>(180 * alpha));

    // Perspective floor lines spreading out from center horizon
    int num_rays = 16;
    for (int i = 0; i <= num_rays; ++i) {
        float norm_x = static_cast<float>(i) / num_rays;
        float bottom_x = p_min.x + w * norm_x;
        float top_x = center.x + (bottom_x - center.x) * 0.15f;
        draw_list->AddLine(ImVec2(top_x, horizon_y), ImVec2(bottom_x, bottom_y), line_col, 1.0f);
    }

    // Horizontal perspective grid rings
    int num_rings = 8;
    for (int i = 1; i <= num_rings; ++i) {
        float t = static_cast<float>(i) / num_rings;
        float ease_t = t * t; // quadratic expansion towards bottom
        float y = horizon_y + (bottom_y - horizon_y) * ease_t;
        float side_margin = (1.0f - ease_t) * w * 0.35f;
        draw_list->AddLine(ImVec2(p_min.x + side_margin, y), ImVec2(p_max.x - side_margin, y), line_col, 1.0f);
    }

    // Horizon line
    draw_list->AddLine(ImVec2(p_min.x + w * 0.3f, horizon_y), ImVec2(p_max.x - w * 0.3f, horizon_y), horizon_col, 1.5f);
}

bool draw_ponder_node(
    const char* id,
    const char* icon,
    const char* name,
    const char* sub,
    const char* pill,
    ImVec2 pos,
    ImVec2 size,
    bool is_active,
    bool is_done,
    ImVec4 accent_color)
{
    ImGui::PushID(id);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + size.x, pos.y + size.y);

    ImGui::SetCursorScreenPos(pos);
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();

    draw_list->PushClipRect(p_min, p_max, true);

    // Background card
    ImU32 bg_col = is_active ? IM_COL32(24, 34, 46, 250) : (hovered ? IM_COL32(22, 30, 40, 240) : IM_COL32(16, 22, 30, 225));
    draw_list->AddRectFilled(p_min, p_max, bg_col, 14.0f);

    // Border & Glow
    ImU32 border_col = is_active ? ImGui::ColorConvertFloat4ToU32(accent_color) :
                       (is_done ? IM_COL32(85, 214, 154, 200) : (hovered ? IM_COL32(80, 110, 140, 180) : IM_COL32(40, 56, 72, 140)));
    float border_thickness = is_active ? 2.0f : 1.0f;
    draw_list->AddRect(p_min, p_max, border_col, 14.0f, 0, border_thickness);

    if (is_active) {
        // Outer soft glow
        draw_list->AddRect(ImVec2(p_min.x - 2.0f, p_min.y - 2.0f), ImVec2(p_max.x + 2.0f, p_max.y + 2.0f),
                           IM_COL32(242, 163, 27, 60), 16.0f, 0, 2.0f);
    }

    // Icon Box on left
    ImVec2 icon_box_min(p_min.x + 12.0f, p_min.y + 12.0f);
    ImVec2 icon_box_max(icon_box_min.x + 38.0f, icon_box_min.y + 38.0f);
    ImU32 icon_bg = is_active ? IM_COL32(36, 48, 64, 255) : IM_COL32(13, 21, 28, 255);
    draw_list->AddRectFilled(icon_box_min, icon_box_max, icon_bg, 10.0f);
    draw_list->AddRect(icon_box_min, icon_box_max, border_col, 10.0f, 0, 1.0f);

    ImVec2 icon_sz = ImGui::CalcTextSize(icon);
    ImVec2 icon_txt_pos(icon_box_min.x + (38.0f - icon_sz.x) * 0.5f, icon_box_min.y + (38.0f - icon_sz.y) * 0.5f);
    draw_list->AddText(icon_txt_pos, is_active ? ImGui::ColorConvertFloat4ToU32(accent_color) : IM_COL32(200, 215, 230, 240), icon);

    // Name
    float text_left = icon_box_max.x + 12.0f;
    draw_list->AddText(ImVec2(text_left, p_min.y + 12.0f),
                       is_active ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 230, 240, 230), name);

    // Subtitle
    if (sub && sub[0] != '\0') {
        draw_list->AddText(ImVec2(text_left, p_min.y + 32.0f),
                           IM_COL32(130, 150, 165, 200), sub);
    }

    // Pill badge at bottom
    if (pill && pill[0] != '\0') {
        ImVec2 pill_sz = ImGui::CalcTextSize(pill);
        ImVec2 pill_min(p_min.x + 12.0f, p_max.y - 28.0f);
        ImVec2 pill_max(pill_min.x + pill_sz.x + 16.0f, pill_min.y + 20.0f);
        draw_list->AddRectFilled(pill_min, pill_max, IM_COL32(10, 15, 20, 220), 6.0f);
        draw_list->AddRect(pill_min, pill_max, IM_COL32(40, 56, 70, 150), 6.0f, 0, 1.0f);
        draw_list->AddText(ImVec2(pill_min.x + 8.0f, pill_min.y + 2.0f),
                           is_active ? ImGui::ColorConvertFloat4ToU32(accent_color) : IM_COL32(150, 175, 190, 220), pill);
    }

    // Done indicator tick
    if (is_done) {
        draw_list->AddCircleFilled(ImVec2(p_max.x - 14.0f, p_min.y + 14.0f), 5.0f, IM_COL32(85, 214, 154, 255));
    }

    draw_list->PopClipRect();
    ImGui::PopID();
    return clicked;
}

bool draw_ponder_timeline(
    const char* id,
    int* current_step,
    int total_steps,
    bool* is_playing,
    float width)
{
    ImGui::PushID(id);
    bool step_changed = false;

    if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
    float height = 44.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background timeline bar
    draw_list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                             IM_COL32(15, 20, 28, 240), 12.0f);
    draw_list->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
                       IM_COL32(40, 52, 68, 160), 12.0f, 0, 1.0f);

    // Transport buttons on left: [ ← ] [ ⏸/▶ ] [ → ]
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 8.0f, pos.y + 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColors::CardBgHovered);

    if (ImGui::Button(" ← ", ImVec2(34.0f, 30.0f))) {
        if (*current_step > 0) {
            (*current_step)--;
            step_changed = true;
        } else {
            *current_step = total_steps - 1;
            step_changed = true;
        }
    }
    ImGui::SameLine(0, 4.0f);

    bool playing = is_playing ? *is_playing : false;
    if (ImGui::Button(playing ? " ⏸ " : " ▶ ", ImVec2(34.0f, 30.0f))) {
        if (is_playing) *is_playing = !playing;
    }
    ImGui::SameLine(0, 4.0f);

    if (ImGui::Button(" → ", ImVec2(34.0f, 30.0f))) {
        *current_step = (*current_step + 1) % total_steps;
        step_changed = true;
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    // Step Ticks Track
    float buttons_width = 120.0f;
    float badge_width = 80.0f;
    float track_x = pos.x + buttons_width + 12.0f;
    float track_w = width - buttons_width - badge_width - 32.0f;
    float tick_h = 10.0f;
    float tick_y = pos.y + (height - tick_h) * 0.5f;

    if (total_steps > 0 && track_w > 50.0f) {
        float tick_gap = 6.0f;
        float single_tick_w = (track_w - tick_gap * (total_steps - 1)) / static_cast<float>(total_steps);

        for (int i = 0; i < total_steps; ++i) {
            float tx = track_x + i * (single_tick_w + tick_gap);
            ImVec2 t_min(tx, tick_y);
            ImVec2 t_max(tx + single_tick_w, tick_y + tick_h);

            ImU32 tick_col;
            if (i < *current_step) {
                tick_col = IM_COL32(85, 214, 154, 255); // Green Done
            } else if (i == *current_step) {
                tick_col = IM_COL32(242, 163, 27, 255); // Cyber Orange Current
            } else {
                tick_col = IM_COL32(35, 46, 60, 200);   // Dark Slate Upcoming
            }

            // Click handling on individual tick
            ImVec2 mouse = ImGui::GetIO().MousePos;
            bool tick_hovered = (mouse.x >= t_min.x && mouse.x <= t_max.x && mouse.y >= t_min.y - 6.0f && mouse.y <= t_max.y + 6.0f);
            if (tick_hovered) {
                tick_col = IM_COL32(255, 200, 100, 255);
                if (ImGui::IsMouseClicked(0)) {
                    *current_step = i;
                    step_changed = true;
                }
            }

            draw_list->AddRectFilled(t_min, t_max, tick_col, 5.0f);
        }
    }

    // Step Counter Badge on Right: "04 / 07"
    char step_buf[32];
    std::snprintf(step_buf, sizeof(step_buf), "%02d / %02d", *current_step + 1, total_steps);
    ImVec2 badge_pos(pos.x + width - badge_width - 8.0f, pos.y + 12.0f);
    draw_list->AddText(badge_pos, IM_COL32(242, 163, 27, 240), step_buf);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + 6.0f));
    ImGui::PopID();
    return step_changed;
}

} // namespace sdr::ui

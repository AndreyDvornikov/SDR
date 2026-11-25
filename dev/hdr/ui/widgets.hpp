#pragma once

#include "imgui.h"
#include <vector>
#include <complex>
#include <string>

namespace sdr::ui {

// Круговой стрелочный / секторный датчик (Radial Gauge) с оранжевой подсветкой
void draw_radial_gauge(
    const char* id,
    const char* title,
    float value,
    float min_val,
    float max_val,
    const char* unit,
    ImVec4 arc_color = ImVec4(1.0f, 0.45f, 0.0f, 1.0f),
    float radius = 48.0f
);

// Карточка с крупной цифрой, заголовком и мини-спарклайном
void draw_metric_card(
    const char* id,
    const char* title,
    const char* value_str,
    const char* subtitle,
    ImVec4 accent_color,
    const std::vector<float>& sparkline = {},
    ImVec2 card_size = ImVec2(0, 0)
);

// Круговой радарный экран созвездия (IQ Constellation Scope) с авто-масштабированием и клиппингом
void draw_radar_constellation(
    const char* id,
    const std::vector<float>& rx_i,
    const std::vector<float>& rx_q,
    const std::vector<std::complex<float>>& ref_symbols,
    ImVec2 size,
    float evm_percent = -1.0f,
    float forced_scale = 0.0f
);

// Спектр мощности с градиентной янтарно-оранжевой заливкой
void draw_gradient_spectrum(
    const char* id,
    const std::vector<float>& freq_axis,
    const std::vector<float>& power_db,
    ImVec2 size,
    float min_db = -100.0f,
    float max_db = 0.0f
);

// Цифровой осциллограф I/Q с динамическим авто-масштабом, клиппингом и режимом глазковой диаграммы
void draw_oscilloscope_screen(
    const char* id,
    const std::vector<float>& i_samples,
    const std::vector<float>& q_samples,
    ImVec2 size,
    float forced_scale = 0.0f,
    bool eye_diagram_mode = false,
    size_t sps = 10
);

// 3D Перспективная сетка радиоканала Ponder
void draw_ponder_perspective_grid(
    ImDrawList* draw_list,
    ImVec2 p_min,
    ImVec2 p_max,
    float alpha = 0.12f
);

// Интерактивный узел радиотракта Ponder (Ponder Node)
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
    ImVec4 accent_color = ImVec4(1.0f, 0.45f, 0.0f, 1.0f)
);

// Интерактивный таймлайн шагов радиотракта в стиле Ponder
bool draw_ponder_timeline(
    const char* id,
    int* current_step,
    int total_steps,
    bool* is_playing,
    float width = 0.0f
);

// Сегментированный переключатель (Pill Switch)
bool draw_segmented_selector(
    const char* id,
    const char* const items[],
    int count,
    int* selected_index,
    float width = 0.0f
);

// Светящийся слайдер с оранжевым треком
bool draw_glow_slider(
    const char* label,
    float* value,
    float min_val,
    float max_val,
    const char* format,
    ImVec4 track_color = ImVec4(1.0f, 0.45f, 0.0f, 1.0f)
);

} // namespace sdr::ui

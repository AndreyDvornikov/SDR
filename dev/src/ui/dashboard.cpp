#include "ui/dashboard.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"
#include "dsp/modulation.hpp"

#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace sdr::ui {

DashboardUI::DashboardUI(SDRController& controller)
    : controller_(controller)
{
    active_tab_ = DashboardTab::ChannelConstellationIQ;
    SDRConfig cfg = controller_.get_config();

    std::strncpy(custom_pcm_path_, "pcms/rxdata.pcm", sizeof(custom_pcm_path_) - 1);
    std::strncpy(tx_payload_buffer_, cfg.tx_payload.c_str(), sizeof(tx_payload_buffer_) - 1);

    persist_rx_i_.reserve(8192);
    persist_rx_q_.reserve(8192);
}

void DashboardUI::update_persistence() {
    std::vector<float> rx_i, rx_q;
    controller_.get_rx_constellation(rx_i, rx_q, 512);

    if (!rx_i.empty()) {
        persist_rx_i_.insert(persist_rx_i_.end(), rx_i.begin(), rx_i.end());
        persist_rx_q_.insert(persist_rx_q_.end(), rx_q.begin(), rx_q.end());
        if (persist_rx_i_.size() > constellation_history_size_) {
            size_t diff = persist_rx_i_.size() - constellation_history_size_;
            persist_rx_i_.erase(persist_rx_i_.begin(), persist_rx_i_.begin() + diff);
            persist_rx_q_.erase(persist_rx_q_.begin(), persist_rx_q_.begin() + diff);
        }
    }
}

void DashboardUI::render() {
    update_persistence();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
    ImGui::Begin("MainCanvas", nullptr, window_flags);
    ImGui::PopStyleVar();

    // 1. TOP HEADER & NAVIGATION
    render_top_bar();

    ImGui::Spacing();

    // 2. PLAYBACK & TRANSCEIVER DECK
    render_playback_deck();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. MAIN WORKSPACE: HUGE CHARTS
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("MainTabContent", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), false);

    switch (active_tab_) {
        case DashboardTab::ChannelConstellationIQ:
            render_tab_channel_iq();
            break;
        case DashboardTab::TxRxCompare:
            render_tab_tx_rx_compare();
            break;
        case DashboardTab::SpectrumAndWaterfall:
            render_tab_spectrum_and_waterfall();
            break;
        case DashboardTab::SdrControlsAndSettings:
            render_tab_sdr_controls();
            break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}

void DashboardUI::render_top_bar() {
    float win_w = ImGui::GetWindowWidth();

    // Brand title on left
    ImGui::BeginGroup();
    ImGui::TextColored(ThemeColors::PrimaryAccent, "PLUTO SDR STUDIO");
    if (win_w > 1050.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ThemeColors::TextSecondary, "|  СибГУТИ  •  ИКС-431  •  Лабораторная №8");
    }
    ImGui::EndGroup();

    // 4 Clean Navigation Tabs
    const char* label1 = (win_w > 1150.0f) ? " Радиоканал " : " Канал ";
    const char* label2 = (win_w > 1150.0f) ? " Сравнение TX/RX " : " TX / RX ";
    const char* label3 = (win_w > 1150.0f) ? " Спектр & Водопад " : " Спектр ";
    const char* label4 = (win_w > 1150.0f) ? " Настройки SDR " : " Настройки ";

    ImGui::SameLine();
    float avail = ImGui::GetContentRegionAvail().x;
    float tab_bar_width = (win_w > 1150.0f) ? 620.0f : 460.0f;
    if (avail > tab_bar_width) {
        ImGui::SameLine(win_w - tab_bar_width - 16.0f);
    }

    auto nav_tab = [this](const char* label, DashboardTab tab) {
        bool active = (active_tab_ == tab);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColors::PrimaryHovered);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColors::CardBgHovered);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextSecondary);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button(label, ImVec2(0, 28.0f))) {
            active_tab_ = tab;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 5.0f);
    };

    nav_tab(label1, DashboardTab::ChannelConstellationIQ);
    nav_tab(label2, DashboardTab::TxRxCompare);
    nav_tab(label3, DashboardTab::SpectrumAndWaterfall);
    nav_tab(label4, DashboardTab::SdrControlsAndSettings);
}

void DashboardUI::render_playback_deck() {
    SignalSource src = controller_.get_source();
    auto& pcm = controller_.get_pcm_player();
    const auto& metrics = controller_.get_metrics();
    SDRConfig cfg = controller_.get_config();

    float deck_h = (src == SignalSource::PCMFile) ? 106.0f : 58.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
    ImGui::BeginChild("PlaybackDeck", ImVec2(ImGui::GetContentRegionAvail().x, deck_h), true);

    // Row 1: Source selector & Quick Buttons & Transport & Speed Selector
    auto source_btn = [this, src](const char* label, SignalSource s) {
        bool active = (src == s);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextSecondary);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button(label, ImVec2(0, 26.0f))) {
            controller_.set_source(s);
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 6.0f);
    };

    source_btn(" PCM Файл (Офлайн) ", SignalSource::PCMFile);
    source_btn(" ADALM-Pluto (Живой эфир) ", SignalSource::ADALM_Pluto);

    ImGui::SameLine(0, 8.0f);
    ImGui::TextColored(ThemeColors::CardBorder, "|");
    ImGui::SameLine(0, 8.0f);

    if (src == SignalSource::PCMFile) {
        // Quick file buttons
        if (ImGui::Button(" rxdata.pcm ", ImVec2(0, 26.0f))) {
            controller_.load_pcm_file("pcms/rxdata.pcm");
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }
        ImGui::SameLine(0, 5.0f);
        if (ImGui::Button(" txdata.pcm ", ImVec2(0, 26.0f))) {
            controller_.load_pcm_file("pcms/txdata.pcm");
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }

        ImGui::SameLine(0, 8.0f);
        ImGui::TextColored(ThemeColors::CardBorder, "|");
        ImGui::SameLine(0, 8.0f);

        // Transport: Play / Pause / Reset
        bool playing = pcm.is_playing();
        if (ImGui::Button(playing ? " Пауза " : " Пуск ", ImVec2(72.0f, 26.0f))) {
            pcm.toggle_play();
        }
        ImGui::SameLine(0, 5.0f);
        if (ImGui::Button(" С начала ", ImVec2(80.0f, 26.0f))) {
            pcm.reset();
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }

        ImGui::NewLine();

        // Row 2: Telemetry on left + Playback Speed Pills on right
        size_t cur_s = pcm.current_index();
        size_t tot_s = pcm.total_samples();
        float ratio = pcm.playback_ratio();
        double time_cur = static_cast<double>(cur_s) / 1e6;
        double time_tot = static_cast<double>(tot_s) / 1e6;

        ImGui::TextColored(ThemeColors::AmberBadge,
                           "%.2f / %.2f с (%.1f %%)  |  Сэмпл: %zu  |  Файл: %s",
                           time_cur, time_tot, ratio * 100.0f, cur_s, pcm.current_filename().c_str());

        // Speed pills aligned to right of Row 2
        ImGui::SameLine();
        float speed_group_w = 280.0f;
        float avail_deck = ImGui::GetContentRegionAvail().x;
        if (avail_deck > speed_group_w) {
            ImGui::SameLine(ImGui::GetCursorPosX() + avail_deck - speed_group_w);
        }

        ImGui::TextColored(ThemeColors::TextSecondary, "Скорость:");
        ImGui::SameLine(0, 5.0f);

        float cur_spd = pcm.get_playback_speed();
        auto speed_pill = [&](const char* label, float spd) {
            bool active = (std::abs(cur_spd - spd) < 0.05f);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
                ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextPrimary);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
                ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextSecondary);
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            if (ImGui::Button(label, ImVec2(0, 22.0f))) {
                pcm.set_playback_speed(spd);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0, 3.0f);
        };

        speed_pill("0.25x", 0.25f);
        speed_pill("0.5x", 0.5f);
        speed_pill("1x", 1.0f);
        speed_pill("2x", 2.0f);
        speed_pill("5x", 5.0f);

        ImGui::NewLine();

        // Row 3: Full-Width Sleek Seek Slider
        float seek_val = ratio;
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 4.0f);
        if (ImGui::SliderFloat("##SeekSlider", &seek_val, 0.0f, 1.0f, "")) {
            pcm.seek_ratio(seek_val);
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }
        ImGui::PopItemWidth();
    } else {
        // ADALM-Pluto hardware controls & status
        bool running = metrics.is_running.load();
        bool connected = metrics.is_connected.load();

        if (connected) {
            render_status_pill("ПОДКЛЮЧЕНО (USB)", true);
        } else {
            render_status_pill("ПЛАТА НЕ ПОДКЛЮЧЕНА", false);
        }

        ImGui::SameLine(0, 10.0f);

        if (running && connected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::RedAlert);
            if (ImGui::Button(" Остановить эфир ", ImVec2(150.0f, 28.0f))) {
                controller_.stop();
            }
            ImGui::PopStyleColor();
        } else {
            if (orange_button(" Переподключить Pluto ", ImVec2(180.0f, 28.0f))) {
                controller_.start();
            }
        }

        ImGui::SameLine(0, 10.0f);

        // Кнопка переключения Loopback прямо в деке
        bool lb = cfg.loopback;
        if (lb) {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::TextSecondary);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button(lb ? " Loopback: ВКЛ " : " Loopback: ВЫКЛ ", ImVec2(0, 28.0f))) {
            controller_.set_loopback(!lb);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 12.0f);
        ImGui::TextColored(ThemeColors::TextSecondary,
                           "Несущая: %.0f МГц  |  TX: %.0f дБ  |  RX: %.0f дБ  |  1.0 MSps",
                           cfg.frequency / 1e6, cfg.tx_gain, cfg.rx_gain);

        if (std::strlen(metrics.last_error) > 0) {
            ImGui::SameLine(0, 12.0f);
            ImGui::TextColored(ThemeColors::AmberBadge, "%s", metrics.last_error);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DashboardUI::render_tab_channel_iq() {
    SDRConfig cfg = controller_.get_config();
    const auto& metrics = controller_.get_metrics();

    std::vector<float> rx_wave_i, rx_wave_q;
    if (eye_diagram_mode_) {
        controller_.get_rx_filtered_time_domain(rx_wave_i, rx_wave_q, 1024);
    } else {
        controller_.get_rx_time_domain(rx_wave_i, rx_wave_q, 512);
    }

    float full_w = ImGui::GetContentRegionAvail().x;
    float full_h = ImGui::GetContentRegionAvail().y;

    float controls_w = std::clamp(full_w * 0.22f, 290.0f, 360.0f);
    float graphs_w = full_w - controls_w - 12.0f;

    // --- СЕКЦИЯ ГРАФИКОВ (КРУПНЫЕ ГРАФИКИ) ---
    ImGui::BeginGroup();

    float chart_w = (graphs_w - 12.0f) * 0.5f;
    float chart_h = full_h - 10.0f;

    // 1. РАДАР СОЗВЕЗДИЯ (IQ CONSTELLATION)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("ConstellationCard", ImVec2(chart_w, chart_h), true);

    render_card_header("РАДАР СОЗВЕЗДИЯ (IQ CONSTELLATION)", "Фазовая плоскость с оранжевой персистентностью");

    auto ref = dsp::get_reference_constellation(cfg.modulation);
    float scope_dim = std::min(chart_w - 24.0f, chart_h - 70.0f);
    draw_radar_constellation("RadarScopeBig", persist_rx_i_, persist_rx_q_, ref,
                             ImVec2(scope_dim, scope_dim));

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.0f);

    // 2. ЦИФРОВОЙ ОСЦИЛЛОГРАФ / ГЛАЗКОВАЯ ДИАГРАММА
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("OscilloscopeCard", ImVec2(chart_w, chart_h), true);

    ImGui::TextColored(ThemeColors::TextPrimary, eye_diagram_mode_ ? "ГЛАЗКОВАЯ ДИАГРАММА" : "ОСЦИЛЛОГРАФ I/Q");
    ImGui::SameLine();
    float eye_avail = ImGui::GetContentRegionAvail().x;
    float eye_btn_w = 95.0f;
    if (eye_avail > eye_btn_w) {
        ImGui::SameLine(ImGui::GetCursorPosX() + eye_avail - eye_btn_w);
    }
    if (ImGui::Button(eye_diagram_mode_ ? " Волна " : " Глазок ", ImVec2(eye_btn_w, 24.0f))) {
        eye_diagram_mode_ = !eye_diagram_mode_;
    }
    ImGui::TextColored(ThemeColors::TextTertiary, eye_diagram_mode_ ? "Наложение 2-символьных траекторий" : "Синфазная (I) и квадратурная (Q)");

    ImGui::Spacing();

    draw_oscilloscope_screen("OscilloscopeBig", rx_wave_i, rx_wave_q,
                             ImVec2(chart_w - 24.0f, chart_h - 74.0f),
                             0.0f, eye_diagram_mode_, cfg.upsample_factor);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::EndGroup();

    ImGui::SameLine(0, 12.0f);

    // --- ПАНЕЛЬ ОПЕРАТИВНОГО УПРАВЛЕНИЯ ПРИЁМОМ ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("ControlsPanel", ImVec2(controls_w, full_h - 10.0f), true);

    render_card_header("ПАРАМЕТРЫ РАДИОКАНАЛА", "Управление SDR и обработкой");

    bool cfg_changed = false;

    // Выбор несущей частоты: строго 800 МГц или 850 МГц из Лаб. 8
    ImGui::TextColored(ThemeColors::TextPrimary, "Несущая частота:");
    auto freq_btn = [&](const char* label, double f) {
        bool active = (std::abs(cfg.frequency - f) < 1e5);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
        else ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
        if (ImGui::Button(label, ImVec2(100.0f, 28.0f))) {
            cfg.frequency = f;
            cfg_changed = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8.0f);
    };

    freq_btn(" 800 МГц ", 800e6);
    freq_btn(" 850 МГц ", 850e6);
    ImGui::NewLine();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Аппаратный loopback
    ImGui::TextColored(ThemeColors::TextPrimary, "Аппаратный цифровой loopback:");
    if (ImGui::Checkbox("ШЛЕЙФ (LOOPBACK) PLUTO", &cfg.loopback)) {
        cfg_changed = true;
    }
    ImGui::TextColored(ThemeColors::TextTertiary, "Замыкает TX на RX внутри чипа AD9364");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Согласованный фильтр (прямоугольная свёртка)
    ImGui::TextColored(ThemeColors::TextPrimary, "Согласованный фильтр:");
    if (ImGui::Checkbox("ВКЛЮЧИТЬ СВЁРТКУ (h_size=10)", &cfg.rx_matched_filter)) {
        cfg_changed = true;
        persist_rx_i_.clear();
        persist_rx_q_.clear();
    }
    ImGui::TextColored(ThemeColors::TextTertiary, "Прямоугольная свёртка (нормировка 0.1)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Компенсатор частотного сдвига CFO
    ImGui::TextColored(ThemeColors::TextPrimary, "Сдвиг гетеродина (CFO):");
    if (draw_glow_slider("CFO (Гц)", &cfg.rx_cfo_hz, -2500.0f, 2500.0f, "%.1f Гц", ThemeColors::PrimaryAccent)) {
        cfg.rx_cfo_correction = (std::abs(cfg.rx_cfo_hz) > 0.1f);
        cfg_changed = true;
    }

    // Точный пресет Лаб. 8 (-1696.25 Гц)
    if (ImGui::Button(" Пресет: -1696 Гц (Лаб. 8) ", ImVec2(ImGui::GetContentRegionAvail().x, 32.0f))) {
        cfg.rx_cfo_hz = -1696.25f;
        cfg.rx_cfo_correction = true;
        cfg_changed = true;
        persist_rx_i_.clear();
        persist_rx_q_.clear();
    }
    if (ImGui::Button(" 0 Гц (Без коррекции) ", ImVec2(ImGui::GetContentRegionAvail().x, 26.0f))) {
        cfg.rx_cfo_hz = 0.0f;
        cfg.rx_cfo_correction = false;
        cfg_changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Реальные физические измерения уровня сигнала (без фейковых SNR/EVM)
    ImGui::TextColored(ThemeColors::TextPrimary, "Уровень сигнала RX:");
    float rms = metrics.rx_rms_level.load();
    float peak = metrics.rx_peak_level.load();
    float rms_dbfs = (rms > 1e-6f) ? (20.0f * std::log10(rms)) : -100.0f;
    float peak_dbfs = (peak > 1e-6f) ? (20.0f * std::log10(peak)) : -100.0f;

    ImGui::TextColored(ThemeColors::CyanContrast, "RMS:  %.2f (%.1f dBFS)", rms, rms_dbfs);
    ImGui::TextColored(ThemeColors::AmberBadge,   "Peak: %.2f (%.1f dBFS)", peak, peak_dbfs);

    ImGui::Spacing();
    ImGui::TextColored(ThemeColors::TextSecondary, "Пакеты: TX %lu  |  RX %lu",
                       metrics.tx_packets.load(), metrics.rx_packets.load());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Очистить созвездие
    if (ImGui::Button(" Очистить экран созвездия ", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
        persist_rx_i_.clear();
        persist_rx_q_.clear();
    }

    if (cfg_changed) {
        controller_.set_config(cfg);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DashboardUI::render_tab_tx_rx_compare() {
    SDRConfig cfg = controller_.get_config();

    std::vector<float> tx_i, tx_q;
    controller_.get_tx_constellation(tx_i, tx_q, 512);

    std::vector<float> tx_wave_i, tx_wave_q, rx_wave_i, rx_wave_q;
    controller_.get_tx_time_domain(tx_wave_i, tx_wave_q, 512);
    controller_.get_rx_filtered_time_domain(rx_wave_i, rx_wave_q, 512);

    float full_w = ImGui::GetContentRegionAvail().x;
    float full_h = ImGui::GetContentRegionAvail().y;
    float half_w = (full_w - 12.0f) * 0.5f;
    float half_h = (full_h - 12.0f) * 0.5f;

    auto ref = dsp::get_reference_constellation(cfg.modulation);
    float scope_sz = std::min(half_w - 24.0f, half_h - 60.0f);

    // 4 Квадранта:
    // Слева: TX эталон (созвездие и сформированная волна)
    // Справа: RX приём (принятое созвездие и волна после свёртки)

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("TxConstCard", ImVec2(half_w, half_h), true);
    render_card_header("TX ЭТАЛОННОЕ СОЗВЕЗДИЕ", "Передаваемые символы модулятора");
    draw_radar_constellation("RadarTxCompare", tx_i, tx_q, ref, ImVec2(scope_sz, scope_sz));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("TxWaveCard", ImVec2(half_w, half_h), true);
    render_card_header("TX СИГНАЛ ВО ВРЕМЕНИ", "Сформированные отсчеты после прямоугольной свёртки");
    draw_oscilloscope_screen("ScopeTxCompare", tx_wave_i, tx_wave_q, ImVec2(half_w - 24.0f, half_h - 60.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    ImGui::SameLine(0, 12.0f);

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("RxConstCard", ImVec2(half_w, half_h), true);
    render_card_header("RX ПРИНЯТОЕ СОЗВЕЗДИЕ", "Реальные символы со шлейфа / эфира с компенсацией CFO");
    draw_radar_constellation("RadarRxCompare", persist_rx_i_, persist_rx_q_, ref, ImVec2(scope_sz, scope_sz));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("RxWaveCard", ImVec2(half_w, half_h), true);
    render_card_header("RX ПРИНЯТЫЙ СИГНАЛ", "Отсчеты после согласованного фильтра свёртки");
    draw_oscilloscope_screen("ScopeRxCompare", rx_wave_i, rx_wave_q, ImVec2(half_w - 24.0f, half_h - 60.0f));
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

void DashboardUI::render_tab_spectrum_and_waterfall() {
    std::vector<float> freq_axis, power_db;
    controller_.get_spectrum(freq_axis, power_db);

    const float* waterfall_data = nullptr;
    size_t wf_w = 0, wf_h = 0;
    controller_.get_waterfall(&waterfall_data, &wf_w, &wf_h);

    float full_w = ImGui::GetContentRegionAvail().x;
    float full_h = ImGui::GetContentRegionAvail().y;
    float half_h = (full_h - 16.0f) * 0.5f;

    // 1. АНАЛИЗАТОР СПЕКТРА FFT
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("SpectrumCardBig", ImVec2(full_w, half_h), true);

    render_card_header("АНАЛИЗАТОР СПЕКТРА FFT (dBFS vs FREQUENCY)", "Быстрое преобразование Фурье (1024 точки, окно Ханна)");

    draw_gradient_spectrum("BigGradientSpectrum", freq_axis, power_db,
                           ImVec2(full_w - 24.0f, half_h - 58.0f), -100.0f, 0.0f);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // 2. ВОДОПАД (2D SPECTROGRAM)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("WaterfallCardBig", ImVec2(full_w, half_h), true);

    render_card_header("СПЕКТРОГРАММА (WATERFALL)", "Непрерывная 2D-тепловая карта спектра во времени");

    ImPlot::PushColormap("WaterfallHotOrange");
    if (ImPlot::BeginPlot("##WaterfallPlotBig", ImVec2(full_w - 24.0f, half_h - 58.0f), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("Частотные бины БПФ (1024 точки)", "Время (отсчеты)", ImPlotAxisFlags_None, ImPlotAxisFlags_Invert);

        if (waterfall_data && wf_w > 0 && wf_h > 0) {
            ImPlot::PlotHeatmap("##WaterfallHeatmap", waterfall_data, static_cast<int>(wf_h), static_cast<int>(wf_w), 0.0, 1.0, nullptr,
                ImPlotPoint(0, 0), ImPlotPoint(wf_w, wf_h));
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopColormap();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void DashboardUI::render_tab_sdr_controls() {
    SDRConfig cfg = controller_.get_config();
    bool cfg_changed = false;

    float full_w = ImGui::GetContentRegionAvail().x;
    float full_h = ImGui::GetContentRegionAvail().y;
    float col_w = (full_w - 24.0f) / 3.0f;

    // КОЛОНКА 1: РАДИОЧАСТОТНЫЙ ТРАКТ
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("RFSettingsCard", ImVec2(col_w, full_h - 6.0f), true);

    render_card_header("РАДИОЧАСТОТНЫЙ ТРАКТ", "Несущие частоты, усиление и loopback");

    // Выбор несущей частоты: строго 800 и 850 МГц
    ImGui::TextColored(ThemeColors::TextPrimary, "Несущая частота (FREQUENCIES_HZ):");
    auto f_btn = [&](const char* label, double f) {
        bool active = (std::abs(cfg.frequency - f) < 1e5);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
        else ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
        if (ImGui::Button(label, ImVec2(100.0f, 28.0f))) {
            cfg.frequency = f;
            cfg_changed = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8.0f);
    };

    f_btn(" 800 МГц ", 800e6);
    f_btn(" 850 МГц ", 850e6);
    ImGui::NewLine();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Частота дискретизации (фиксировано)
    ImGui::TextColored(ThemeColors::TextPrimary, "Частота дискретизации:");
    ImGui::TextColored(ThemeColors::CyanContrast, "1.0 MSps (SAMPLE_RATE = 1e6)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Loopback
    ImGui::TextColored(ThemeColors::TextPrimary, "Аппаратный цифровой Loopback:");
    if (ImGui::Checkbox("ВКЛЮЧИТЬ LOOPBACK", &cfg.loopback)) {
        cfg_changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Аппаратное усиление
    ImGui::TextColored(ThemeColors::TextPrimary, "Усиление приёмника (RX Gain):");
    float rx_g = static_cast<float>(cfg.rx_gain);
    if (ImGui::SliderFloat("##RXGainSlider", &rx_g, 0.0f, 73.0f, "%.0f дБ")) {
        cfg.rx_gain = rx_g;
        cfg_changed = true;
    }

    ImGui::TextColored(ThemeColors::TextPrimary, "Усиление передатчика (TX Gain):");
    float tx_g = static_cast<float>(cfg.tx_gain);
    if (ImGui::SliderFloat("##TXGainSlider", &tx_g, -80.0f, 0.0f, "%.0f дБ")) {
        cfg.tx_gain = tx_g;
        cfg_changed = true;
    }

    ImGui::Spacing();
    if (orange_button(" Применить настройки к плате ", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
        controller_.set_config(cfg);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.0f);

    // КОЛОНКА 2: ЦИФРОВОЙ МОДУЛЯТОР И ПЕРЕДАЧА
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("ModulatorSettingsCard", ImVec2(col_w, full_h - 6.0f), true);

    render_card_header("МОДУЛЯЦИЯ И СВЁРТКА", "Схема созвездия и параметры передачи");

    // Выбор модуляции (строго QPSK и BPSK)
    ImGui::TextColored(ThemeColors::TextPrimary, "Схема цифровой модуляции:");
    auto mod_btn = [&](const char* label, dsp::ModulationType m) {
        bool active = (cfg.modulation == m);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::PrimaryAccent);
        else ImGui::PushStyleColor(ImGuiCol_Button, ThemeColors::ChipBg);
        if (ImGui::Button(label, ImVec2(0, 28.0f))) {
            cfg.modulation = m;
            cfg_changed = true;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 8.0f);
    };

    mod_btn(" QPSK (Лаб 8) ", dsp::ModulationType::QPSK);
    mod_btn(" BPSK ", dsp::ModulationType::BPSK);
    ImGui::NewLine();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Импульсная свёртка
    ImGui::TextColored(ThemeColors::TextPrimary, "Формирование импульсов:");
    ImGui::TextColored(ThemeColors::CyanContrast, "Прямоугольная свёртка (h_size = 10, scale = 1.0)");
    ImGui::TextColored(ThemeColors::TextTertiary, "Апсемплинг x10 + прямоугольное окно");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Режим передачи данных
    ImGui::TextColored(ThemeColors::TextPrimary, "Режим передачи потока:");
    ImGui::TextColored(ThemeColors::SuccessGreen, "Непрерывный поток («данные данные данные»)");
    ImGui::TextColored(ThemeColors::TextTertiary, "Без пауз и пустых буферов тишины");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Полезная нагрузка TX
    ImGui::TextColored(ThemeColors::TextPrimary, "Полезная нагрузка пакета (TX Payload):");
    if (ImGui::InputText("##TXPayloadInput", tx_payload_buffer_, sizeof(tx_payload_buffer_))) {
        cfg.tx_payload = tx_payload_buffer_;
        cfg_changed = true;
    }
    if (ImGui::Button(" SIBGUTI_IKS431 (Лаб 8) ", ImVec2(180.0f, 26.0f))) {
        std::strncpy(tx_payload_buffer_, "SIBGUTI_IKS431", sizeof(tx_payload_buffer_) - 1);
        cfg.tx_payload = tx_payload_buffer_;
        cfg_changed = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.0f);

    // КОЛОНКА 3: ДИСПЕТЧЕР ФАЙЛОВ PCM
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::CardBg);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::BeginChild("FileManagerCard", ImVec2(col_w, full_h - 6.0f), true);

    render_card_header("ДИСПЕТЧЕР PCM ФАЙЛОВ", "Загрузка реальных записей эфира");

    ImGui::TextColored(ThemeColors::TextPrimary, "Путь к произвольному файлу:");
    ImGui::InputText("##CustomPCMInput", custom_pcm_path_, sizeof(custom_pcm_path_));
    if (ImGui::Button(" Загрузить указанный файл ", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
        controller_.load_pcm_file(custom_pcm_path_);
        persist_rx_i_.clear();
        persist_rx_q_.clear();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ThemeColors::TextPrimary, "Обнаруженные дампы СибГУТИ:");

    auto file_entry = [&](const char* title, const char* path, const char* desc) {
        ImGui::BeginGroup();
        ImGui::TextColored(ThemeColors::AmberBadge, "[ %s ]", title);
        ImGui::TextColored(ThemeColors::TextTertiary, "%s", desc);
        ImGui::EndGroup();
        ImGui::SameLine();
        float btn_x = ImGui::GetWindowWidth() - 110.0f;
        if (btn_x > 0.0f) ImGui::SameLine(btn_x);
        if (ImGui::Button((" Загрузить##" + std::string(path)).c_str(), ImVec2(90.0f, 26.0f))) {
            controller_.load_pcm_file(path);
            persist_rx_i_.clear();
            persist_rx_q_.clear();
        }
        ImGui::Spacing();
    };

    file_entry("rxdata.pcm", "pcms/rxdata.pcm", "Реальная запись эфира ADALM-Pluto (1.28M сэмплов, CFO ~ -1696 Гц)");
    file_entry("txdata.pcm", "pcms/txdata.pcm", "Полный передаваемый эталонный поток (210 МБ)");
    file_entry("backup rxdata", "pcms/backup_clean_20261110/rxdata.pcm", "Чистая резервная копия записи лаб. 8");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ThemeColors::TextPrimary, "Сброс рабочих буферов:");
    if (ImGui::Button(" Очистить буферы и сбросить спектр ", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
        controller_.clear_buffers();
        persist_rx_i_.clear();
        persist_rx_q_.clear();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (cfg_changed) {
        controller_.set_config(cfg);
    }
}

} // namespace sdr::ui

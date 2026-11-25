#pragma once

#include "sdr/sdr_controller.hpp"
#include <string>
#include <vector>

namespace sdr::ui {

enum class DashboardTab {
    ChannelConstellationIQ = 0, // Радиоканал (Созвездие, Осциллограф / Глазковая, CFO, Фильтр, Loopback)
    TxRxCompare = 1,            // Сравнение TX / RX (Эталон vs Принятый сигнал)
    SpectrumAndWaterfall = 2,   // Спектр & Водопад (Реальный БПФ + 2D Waterfall)
    SdrControlsAndSettings = 3  // Настройки SDR & Файлы (Частоты 800/850, Loopback, QPSK/BPSK, PCM)
};

class DashboardUI {
public:
    explicit DashboardUI(SDRController& controller);
    ~DashboardUI() = default;

    void render();
    void set_active_tab(DashboardTab tab) { active_tab_ = tab; }

private:
    void render_top_bar();
    void render_playback_deck();

    void render_tab_channel_iq();
    void render_tab_tx_rx_compare();
    void render_tab_spectrum_and_waterfall();
    void render_tab_sdr_controls();

    void update_persistence();

    SDRController& controller_;
    DashboardTab active_tab_{DashboardTab::ChannelConstellationIQ};

    // UI state cache
    char custom_pcm_path_[512];
    char tx_payload_buffer_[256];

    // Режим осциллографа: false = Waveform I/Q, true = Глазковая диаграмма
    bool eye_diagram_mode_{false};

    // Буфер персистентности созвездия (обновляется непрерывно на всех вкладках)
    std::vector<float> persist_rx_i_;
    std::vector<float> persist_rx_q_;
    size_t constellation_history_size_{2048};
};

} // namespace sdr::ui

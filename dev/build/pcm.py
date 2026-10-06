import numpy as np
import matplotlib.pyplot as plt


# ---------------- Настройки ----------------
TX_FILE = "txdata.pcm"
RX_FILE = "rxdata.pcm"

MAX_POINTS = 50_000      # сколько точек рисовать на каждый график
MAX_SAMPLES = None       # ограничить чтение, например 5_000_000; None = весь файл
# ------------------------------------------


def read_pcm(path, max_samples=None):
    """Читает PCM CS16 (int16, чередование I Q I Q ...) и возвращает I, Q."""
    count = 2 * max_samples if max_samples else -1
    d = np.fromfile(path, dtype=np.int16, count=count)
    n = len(d) // 2 * 2           # обрезаем до чётного
    d = d[:n]
    I = d[0::2].astype(np.int32)
    Q = d[1::2].astype(np.int32)
    return I, Q


def decimate(x, y, max_points=MAX_POINTS):
    """Прореживает пару (x, y), чтобы уложиться в max_points."""
    n = len(x)
    if n <= max_points:
        return x, y
    step = n // max_points + 1
    return x[::step], y[::step]


def main():
    # ---- Чтение ----
    print("Читаем TX:", TX_FILE)
    real, imag = read_pcm(TX_FILE, MAX_SAMPLES)
    print(f"  TX samples: {len(real)}")

    print("Читаем RX:", RX_FILE)
    r, i = read_pcm(RX_FILE, MAX_SAMPLES)
    print(f"  RX samples: {len(r)}")

    # ---- Прореживание ----
    tx_x = np.arange(len(real), dtype=np.int64)
    rx_x = np.arange(len(r), dtype=np.int64)

    tx_x_d, tx_real_d = decimate(tx_x, real)
    _,      tx_imag_d = decimate(tx_x, imag)

    rx_x_d, rx_r_d = decimate(rx_x, r)
    _,      rx_i_d = decimate(rx_x, i)

    # ---- Графики ----
    fig, axs = plt.subplots(2, 1, layout='constrained', figsize=(12, 7))

    # TX
    axs[0].plot(tx_x_d, tx_real_d, color='blue', linewidth=0.7, label='I')
    axs[0].plot(tx_x_d, tx_imag_d, color='red',  linewidth=0.7, label='Q')
    axs[0].set_title(f"TX ({TX_FILE})")
    axs[0].set_xlabel("sample")
    axs[0].set_ylabel("amplitude")
    axs[0].legend()
    axs[0].grid(True, alpha=0.3)

    # RX
    axs[1].plot(rx_x_d, rx_r_d, color='blue', linewidth=0.7, label='I')
    axs[1].plot(rx_x_d, rx_i_d, color='red',  linewidth=0.7, label='Q')
    axs[1].set_title(f"RX ({RX_FILE})")
    axs[1].set_xlabel("sample")
    axs[1].set_ylabel("amplitude")
    axs[1].legend()
    axs[1].grid(True, alpha=0.3)

    plt.show()


if __name__ == "__main__":
    main()
